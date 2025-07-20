/*
Trinity Rendering Engine - Copyright Andrew Lucas 2009-2012

The Trinity Engine is free software, distributed in the hope th-
at it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU Lesser General Public License for more det-
ails.

Water Shader
Written by Andrew Lucas
*/

#include <cstdio>
#include <cstdlib>
#include <cmath>

#include "windows.h"

#include "gl/glew.h"
#include "hud.h"
#include "cl_util.h"
#include <gl/glu.h>

#include "const.h"
#include "studio.h"
#include "entity_state.h"
#include "triangleapi.h"
#include "event_api.h"
#include "pm_defs.h"

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <math.h>

#include "propmanager.h"
#include "particle_engine.h"
#include "bsprenderer.h"
#include "watershader.h"
#include "mirrormanager.h"

#include "r_efx.h"
#include "r_studioint.h"
#include "studio_util.h"
#include "event_api.h"
#include "event_args.h"

#include "StudioModelRenderer.h"
#include "GameStudioModelRenderer.h"
#ifdef GOLDSRC_PHYS
#include "PhyStudioModelRenderer.h"
extern PhyStudioModelRenderer g_StudioRenderer;
#else
extern CGameStudioModelRenderer g_StudioRenderer;
#endif

extern inline float sgn(float a);

//===========================================
// GLSL SHADERS
//===========================================
const char* water_depth_vertex =
R"(
	#version 120

	uniform mat4 projectionMatrix;

	uniform mat4 modelMatrix;

	attribute vec4 vertexPosition;  // vertex.position
	attribute vec2 vertexTexCoord;  // vertex.texcoord

	uniform int underwater;

	varying vec4 texcoord0;
	varying vec4 texcoord1;
	varying vec3 texcoord2;
	varying float fogcoord;

	vec4 R1;

	void main() {

		R1 = projectionMatrix * (modelMatrix * gl_Vertex);

		gl_Position = R1;

		texcoord0.xy = gl_MultiTexCoord0.xy;

		texcoord0.xy /= 128;

		// Pass finalPosition as texcoord[1]
		texcoord1 = R1;

		texcoord2.xyz = gl_Vertex.xyz;

		// Set fog coordinate based on depth
		gl_FogFragCoord = R1.z;
	}
	
	)";

const char* water_fragment_water_regular =
R"(
	#version 120

	uniform vec3 renderorigin;   // program.local[0] = (x, y, z)
	uniform vec3 waterfog;       // program.local[1] = (r, g, b)
	uniform float m_flFresnelTerm; // program.local[2] = float
	uniform float flTime;        // program.local[3] = client time
	
	uniform sampler2D texture0;  // normal texture
	uniform sampler2D texture1;  // refraction texture
	uniform sampler2D texture2;  // reflection texture
	uniform sampler2D texture3;  // original water texture

	uniform float normalscale;
	uniform float watertex_scale;
	uniform float refraction_scale;
	uniform float reflection_scale;

	uniform int underwater;

	uniform float texelSize; //float because water texture is square
	
	varying vec4 texcoord0;
	varying vec4 texcoord1;
	varying vec3 texcoord2;

	float getaveragebrightness(sampler2D tex, vec2 uv) {
	    float brightness = 0.0;
	
	    for (int x = -4; x <= 4; ++x) {
	        for (int y = -4; y <= 4; ++y) {
	            vec2 offset = vec2(x, y) * texelSize;
	            vec4 sample = texture2D(tex, uv + offset);
	            brightness += dot(sample.rgb, vec3(0.299, 0.587, 0.114));
	        }
	    }
	
	    return brightness / 16.0; // 4x4 kernel
	}
	
	void main() {
		const vec4 c4 = vec4(1.3, 0.97, 0.5, 0.0);
		const vec4 c5 = vec4(0.2, 0.15, 0.13, 0.11);
		const vec4 c6 = vec4(0.17, 0.14, 0.16, 1.0);
		const vec2 c7 = vec2(0.23, 0.33333334);

		// --- Animated Normal Sampling (4 samples) ---
		vec2 offsets[4];
		offsets[0] = texcoord0.xy + c5.xy * flTime;
		offsets[1] = texcoord0.xy + vec2(c5.w * flTime, c5.z * -flTime);
		offsets[2] = texcoord0.xy + vec2(c6.x * flTime, -c6.yz * flTime);
		offsets[3] = texcoord0.xy + vec2(-c6.z * flTime, c6.y * flTime);

		vec3 normal = vec3(0.0);
		for (int i = 0; i < 4; ++i) {
		    normal += texture2D(texture0, offsets[i]).rgb;
		}
		normal *= c4.z;
		normal -= c6.w;
		normal = normal / length(normal);
		normal.xy *= normalscale;

		float depthFactor = clamp(-normal.y * 4, 0.0, 1.0); // Bigger when normal points down

		// --- Wiggle effect (on distortion coords) ---
		vec2 wiggle = vec2(
		    sin(texcoord0.y * 15.0 + flTime * 3.0),
		    cos(texcoord0.x * 15.0 + flTime * 3.0)
		) * 0.01;

		// --- Distorted UVs ---
		float projW = 1.0 / texcoord1.w;
		vec2 refractUV = texcoord1.xy * projW + normal.xy;
		vec2 reflectUV = vec2(texcoord1.x, -texcoord1.y) * projW + normal.xy;

		// --- Sampling reflection/refraction ---
		vec4 refraction = texture2D(texture1, refractUV * 0.5 + 0.5);
		vec4 reflection = texture2D(texture2, reflectUV * 0.5 + 0.5);

		//refraction
		float brightness = dot(refraction.rgb, vec3(0.299, 0.587, 0.114));
		float blendFactor = smoothstep(0.0, 0.6, brightness);

		//reflection
		float brightness2 = dot(reflection.rgb, vec3(0.299, 0.587, 0.114));
		float blendFactor2 = smoothstep(0.0, 0.6, brightness2);

		// --- Blend with base texture (wiggled) ---
		vec4 base = texture2D(texture3, texcoord0.xy + normal.xy * 0.8);
		float refractionbrightness = getaveragebrightness(texture1, refractUV * 0.5 + 0.5);
		float reflectionbrightness = getaveragebrightness(texture2, reflectUV * 0.5 + 0.5);
		base.rgb *= clamp( ( ( refractionbrightness + reflectionbrightness )) * 2, 0.0, 1.0);
		base.rgb *= exp(-depthFactor * normalscale);
		base *= clamp(watertex_scale, 0.0, 1.0);

		// Base + Refraction
		//refraction = mix(base, refraction, blendFactor * refraction_scale);
		//reflection = mix(base, reflection, blendFactor2 * reflection_scale);

		refraction = mix(refraction, base, watertex_scale);
		reflection = mix(reflection, base, watertex_scale);

		refraction.rgb *= exp(-depthFactor * 4);
		reflection.rgb *= exp(-depthFactor * 4);

		//refraction *= refraction_scale;
		reflection *= reflection_scale;

		// --- Fresnel ---
		vec3 viewDir = normalize(renderorigin - texcoord2);
		float fresnel = pow(1.0 - max(dot(viewDir, vec3(0, 0, 1)), 0.0), 2.0);
		fresnel = clamp(fresnel * m_flFresnelTerm, 0.0, 1.0);

		// --- Final Color ---
		vec4 waterColor; 
		if(underwater == 0)
			waterColor = mix(refraction, reflection, fresnel);
		else
			waterColor = refraction;

		waterColor.rgb *= 1.1;

		// --- Fog ---
		float fogFactor = clamp((waterColor.r + waterColor.g + waterColor.b) / 3.0, 0.0, 1.0);
		vec3 fogged = mix(waterColor.rgb, waterfog, fogFactor * 0.3);

		gl_FragColor = vec4(fogged, 1.0);
	}
	
	)";

/*
====================
Init

====================
*/
void CWaterShader::Init(void)
{
	// Set up cvar
	m_pCvarWaterShader = gEngfuncs.pfnRegisterVariable("te_water", "1", FCVAR_ARCHIVE);
	m_pCvarWaterDebug = gEngfuncs.pfnRegisterVariable("te_water_debug", "0", 0);
	m_pCvarWaterResolution = gEngfuncs.pfnRegisterVariable("te_water_resolution", "512", FCVAR_ARCHIVE); //MAX:1024

	if (!gBSPRenderer.m_bShaderSupport)
		return;

	m_WaterFragmentShader = gBSPRenderer.createShaderProgram(water_depth_vertex, water_fragment_water_regular);
}

/*
====================
ClearEntities

====================
*/
void CWaterShader::ClearEntities(void)
{
	if (!m_iNumWaterEntities)
		return;

	for (int i = 0; i < m_iNumWaterEntities; i++)
	{
		glDeleteTextures(1, &m_pWaterEntities[i].reflect);
		glDeleteTextures(1, &m_pWaterEntities[i].refract);
		free(m_pWaterEntities[i].surfaces);
	}

	memset(m_pWaterEntities, NULL, sizeof(m_pWaterEntities));
	m_iNumWaterEntities = NULL;

	memset(m_pWaterEntInfo, NULL, sizeof(m_pWaterEntInfo));
	m_iNumWaterData = NULL;
}

/*
====================
Shutdown

====================
*/
void CWaterShader::Shutdown(void)
{
	ClearEntities();
}

/*
====================
VidInit

====================
*/
void CWaterShader::VidInit(void)
{
	int iCurrentBinding;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &iCurrentBinding);

	// Load texture
	m_pNormalTexture = gTextureLoader.LoadTexture("gfx/textures/watershader.tga");
	glBindTexture(GL_TEXTURE_2D, iCurrentBinding);

	if (!m_pNormalTexture)
	{
		gEngfuncs.pfnClientCmd("escape\n");
		MessageBox(NULL, "VIDEO ERROR: Could not load 'gfx/textures/watershader.tga'!\n", "ERROR", MB_OK);
		gEngfuncs.pfnClientCmd("quit\n");
	}

	ClearEntities();
}

/*
====================
Restore

====================
*/
void CWaterShader::Restore(void)
{
	if (m_pCvarWaterShader->value < 1)
		return;

	if (!gBSPRenderer.m_bShaderSupport)
		return;

	if (!m_iNumWaterEntities)
		return;

	if (!m_bViewInWater)
		return;

	// End of frame, so reset
	gHUD.m_pFogSettings = m_pMainFogSettings;
}

/*
====================
LoadScript

====================
*/
void CWaterShader::LoadScript(void)
{
	char szFile[64];
	char szMapName[64];
	const char* levelname = gEngfuncs.pfnGetLevelName();

	FilenameFromPath((char*)levelname, szMapName);
	sprintf(szFile, "scripts/water_%s.txt", szMapName);

	char* pFile = (char*)gEngfuncs.COM_LoadFile(szFile, 5, NULL);

	if (!pFile)
		pFile = (char*)gEngfuncs.COM_LoadFile("scripts/water_default.txt", 5, NULL);

	if (!pFile)
	{
		gEngfuncs.Con_Printf("Could not load default water definition file 'scripts/water_default.txt'!\n");

		memset(&m_pWaterFogSettings, 0, sizeof(fog_settings_t));
		m_flFresnelTerm = 1;
		return;
	}

	char* pToken = pFile;
	while (1)
	{
		char szField[32];
		char szValue[32];

		pToken = gEngfuncs.COM_ParseFile(pToken, szField);

		if (!pToken)
			break;

		pToken = gEngfuncs.COM_ParseFile(pToken, szValue);

		if (!pToken)
			break;

		if (!strcmp(szField, "fresnel"))
			m_flFresnelTerm = atof(szValue);
		else if (!strcmp(szField, "colr"))
			m_pWaterFogSettings.color[0] = atof(szValue) / 255.0;
		else if (!strcmp(szField, "colg"))
			m_pWaterFogSettings.color[1] = atof(szValue) / 255.0;
		else if (!strcmp(szField, "colb"))
			m_pWaterFogSettings.color[2] = atof(szValue) / 255.0;
		else if (!strcmp(szField, "fogend"))
			m_pWaterFogSettings.end = atof(szValue);
		else if (!strcmp(szField, "fogstart"))
			m_pWaterFogSettings.start = atof(szValue);
		else
			gEngfuncs.Con_Printf("Unknown field: %s\n", szField);
	}
	gEngfuncs.COM_FreeFile(pFile);

	// always true
	m_pWaterFogSettings.affectsky = true;

	if (m_pWaterFogSettings.end < 1 && m_pWaterFogSettings.start < 1)
		m_pWaterFogSettings.active = false;
	else
		m_pWaterFogSettings.active = true;

	//if (m_flFresnelTerm <= 0)
	//	m_flFresnelTerm = 1;
}

/*
====================
ShouldReflect

====================
*/
bool CWaterShader::ShouldReflect(int index)
{
	//if (ViewInWater())
		return true;

	// Optimization: Try and find a water entity on the same z coord
	//for (int i = 0; i < index; i++)
	//{
	//	if (m_pWaterEntities[i].draw)
	//	{
	//		if (GetWaterOrigin(&m_pWaterEntities[i]).z == GetWaterOrigin().z)
	//			return false;
	//	}
	//}
	return true;
}

/*
====================
AddEntity

====================
*/
void CWaterShader::AddEntity(cl_entity_t* entity)
{
	if (m_iNumWaterEntities == MAX_WATER_ENTITIES)
		return;

	for (int i = 0; i < m_iNumWaterEntities; i++)
	{
		if (m_pWaterEntities[i].entity == entity)
			return; // Already in cache
	}

	cl_water_t* pWater = &m_pWaterEntities[m_iNumWaterEntities];
	pWater->index = m_iNumWaterEntities;
	m_iNumWaterEntities++;

	int isurfacecount = 0;
	msurface_t* psurfaces = entity->model->surfaces + entity->model->firstmodelsurface;
	for (int i = 0; i < entity->model->nummodelsurfaces; i++)
	{
		int j = 0;
		for (; j < psurfaces[i].polys->numverts; j++)
		{
			if (psurfaces[i].polys->verts[0][2] != (entity->curstate.maxs.z - 1))
				break;
		}

		if (j != psurfaces[i].polys->numverts)
			continue;

		if (psurfaces[i].flags & SURF_PLANEBACK)
			continue;

		if (psurfaces[i].plane->normal[2] != 1)
			continue;

		isurfacecount++;
	}

	// Allocate array of pointers
	pWater->surfaces = (msurface_t**)malloc(sizeof(msurface_t*) * isurfacecount);

	for (int i = 0; i < entity->model->nummodelsurfaces; i++)
	{
		int j = 0;
		for (; j < psurfaces[i].polys->numverts; j++)
		{
			if (psurfaces[i].polys->verts[0][2] != (entity->curstate.maxs.z - 1))
				break;
		}

		if (j != psurfaces[i].polys->numverts)
			continue;

		if (psurfaces[i].flags & SURF_PLANEBACK)
			continue;

		if (psurfaces[i].plane->normal[2] != 1)
			continue;

		pWater->surfaces[pWater->numsurfaces] = &psurfaces[i];
		pWater->numsurfaces++;
	}

	if (!pWater->numsurfaces)
	{
		memset(&m_pWaterEntities[m_iNumWaterEntities], 0, sizeof(cl_water_t));
		m_iNumWaterEntities--;
		return;
	}

	pWater->mins = Vector(9999, 9999, 9999);
	pWater->maxs = Vector(-9999, -9999, -9999);

	for (int i = 0; i < pWater->numsurfaces; i++)
	{
		for (glpoly_t* bp = pWater->surfaces[i]->polys; bp; bp = bp->next)
		{
			for (int j = 0; j < bp->numverts; j++)
			{
				if (pWater->mins[0] > bp->verts[j][0])
					pWater->mins[0] = bp->verts[j][0];

				if (pWater->mins[1] > bp->verts[j][1])
					pWater->mins[1] = bp->verts[j][1];

				if (pWater->mins[2] > bp->verts[j][2])
					pWater->mins[2] = bp->verts[j][2];

				if (pWater->maxs[0] < bp->verts[j][0])
					pWater->maxs[0] = bp->verts[j][0];

				if (pWater->maxs[1] < bp->verts[j][1])
					pWater->maxs[1] = bp->verts[j][1];

				if (pWater->maxs[2] < bp->verts[j][2])
					pWater->maxs[2] = bp->verts[j][2];
			}
		}
	}

	pWater->entity = entity;
	pWater->entity->efrag = (efrag_s*)pWater;

	pWater->wplane.dist = psurfaces->plane->dist;
	pWater->wplane.type = psurfaces->plane->type;
	pWater->wplane.pad[0] = psurfaces->plane->pad[0];
	pWater->wplane.pad[1] = psurfaces->plane->pad[1];
	pWater->wplane.signbits = psurfaces->plane->signbits;
	pWater->wplane.normal[2] = 1;

	pWater->reflect = current_ext_texture_id;
	current_ext_texture_id++;
	pWater->refract = current_ext_texture_id;
	current_ext_texture_id++;

	pWater->origin[0] = (pWater->mins[0] + pWater->maxs[0]) * 0.5f;
	pWater->origin[1] = (pWater->mins[1] + pWater->maxs[1]) * 0.5f;
	pWater->origin[2] = (pWater->mins[2] + pWater->maxs[2]) * 0.5f;

	glBindTexture(GL_TEXTURE_2D, pWater->reflect);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glBindTexture(GL_TEXTURE_2D, pWater->refract);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

/*
====================
SetupClipping

====================
*/
void CWaterShader::SetupClipping(ref_params_t* pparams, bool negative)
{
	float dot;
	float eq1[4];
	float eq2[4];
	float projection[16];

	Vector vDist;
	Vector vNorm;
	
	Vector vForward;
	Vector vRight;
	Vector vUp;

	AngleVectors(pparams->viewangles, vForward, vRight, vUp);
	VectorSubtract(GetWaterOrigin(), pparams->vieworg, vDist);

	VectorInverse(vRight);
	VectorInverse(vUp);

	if (negative)
	{
		eq1[0] = DotProduct(vRight, -m_pCurWater->wplane.normal);
		eq1[1] = DotProduct(vUp, -m_pCurWater->wplane.normal);
		eq1[2] = DotProduct(vForward, -m_pCurWater->wplane.normal);
		eq1[3] = DotProduct(vDist, -m_pCurWater->wplane.normal);

		//DotProductSSE(&eq1[0], vRight, -m_pCurWater->wplane.normal);
		//DotProductSSE(&eq1[1], vUp, -m_pCurWater->wplane.normal);
		//DotProductSSE(&eq1[2], vForward, -m_pCurWater->wplane.normal);
		//DotProductSSE(&eq1[3], vDist, -m_pCurWater->wplane.normal);
	}
	else
	{
		eq1[0] = DotProduct(vRight, m_pCurWater->wplane.normal);
		eq1[1] = DotProduct(vUp, m_pCurWater->wplane.normal);
		eq1[2] = DotProduct(vForward, m_pCurWater->wplane.normal);
		eq1[3] = DotProduct(vDist, m_pCurWater->wplane.normal);

		//DotProductSSE(&eq1[0], vRight, m_pCurWater->wplane.normal);
		//DotProductSSE(&eq1[1], vUp, m_pCurWater->wplane.normal);
		//DotProductSSE(&eq1[2], vForward, m_pCurWater->wplane.normal);
		//DotProductSSE(&eq1[3], vDist, m_pCurWater->wplane.normal);
	}

	// Change current projection matrix into an oblique projection matrix
	glGetFloatv(GL_PROJECTION_MATRIX, projection);

	eq2[0] = (sgn(eq1[0]) + projection[8]) / projection[0];
	eq2[1] = (sgn(eq1[1]) + projection[9]) / projection[5];
	eq2[2] = -1.0F;
	eq2[3] = (1.0F + projection[10]) / projection[14];

	dot = eq1[0] * eq2[0] + eq1[1] * eq2[1] + eq1[2] * eq2[2] + eq1[3] * eq2[3];

	projection[2] = eq1[0] * (2.0f / dot);
	projection[6] = eq1[1] * (2.0f / dot);
	projection[10] = eq1[2] * (2.0f / dot) + 1.0F;
	projection[14] = eq1[3] * (2.0f / dot);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadMatrixf(projection);

	glMatrixMode(GL_MODELVIEW);
}

/*
====================
ViewInWater

====================
*/
bool CWaterShader::ViewInWater(void)
{
	Vector mins, maxs;
	for (int i = 0; i < 3; i++)
	{
		mins[i] = m_pCurWater->entity->model->mins[i];
		maxs[i] = m_pCurWater->entity->model->maxs[i];
	}

	if(m_pCvarWaterShader->value < 1)
	{
		if (gBSPRenderer.m_vRenderOrigin[0] > mins[0] &&
			gBSPRenderer.m_vRenderOrigin[1] > mins[1] &&
			gBSPRenderer.m_vRenderOrigin[2] > mins[2] &&
			gBSPRenderer.m_vRenderOrigin[0] < maxs[0] &&
			gBSPRenderer.m_vRenderOrigin[1] < maxs[1] &&
			gBSPRenderer.m_vRenderOrigin[2] < maxs[2])
			return true;
	}
	else
	{
		if (m_vViewOrigin[0] > mins[0] &&
			m_vViewOrigin[1] > mins[1] &&
			m_vViewOrigin[2] > mins[2] &&
			m_vViewOrigin[0] < maxs[0] &&
			m_vViewOrigin[1] < maxs[1] &&
			m_vViewOrigin[2] < maxs[2])
			return true;
	}

	return false;
}

/*
====================
DrawWaterPasses

====================
*/
void CWaterShader::DrawWaterPasses(ref_params_t* pparams)
{
	if (m_pCvarWaterShader->value < 1)
	{
		//get the fog in atleast
		if(gHUD.m_pFogSettings.color != m_pWaterFogSettings.color)
			m_pMainFogSettings = gHUD.m_pFogSettings;
		for (int i = 0; i < m_iNumWaterEntities; i++)
		{
			m_pCurWater = &m_pWaterEntities[i];

			if (ViewInWater())
			{
				gHUD.m_pFogSettings = m_pWaterFogSettings;
				m_bViewInWater = true;
				break;
			}
			else
			{
				gHUD.m_pFogSettings = m_pMainFogSettings;
				m_bViewInWater = false;
				break;
			}
		}
		return;
	}

	if (!gBSPRenderer.m_bShaderSupport)
		return;

	if (!m_iNumWaterEntities)
		return;

	// Completely clear everything
	glClearColor(GL_ZERO, GL_ZERO, GL_ZERO, GL_ONE);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_ACCUM_BUFFER_BIT);

	m_iNumPasses = NULL;
	m_bViewInWater = false;
	m_pViewParams = pparams;
	m_pMainFogSettings = gHUD.m_pFogSettings;
	gBSPRenderer.m_bMirroring = true;

	VectorCopy(pparams->vieworg, m_vViewOrigin);
	memcpy(&m_pWaterParams, m_pViewParams, sizeof(ref_params_t));

	bool onlyrenderthiswater = false;

	for (int i = 0; i < m_iNumWaterEntities; i++)
	{
		m_pCurWater = &m_pWaterEntities[i];

		if (ViewInWater())
		{
			onlyrenderthiswater = true;
		}
		if (!m_pCurWater->draw)
			continue;

		gHUD.viewFrustum.SetFrustum(pparams->viewangles, pparams->vieworg, gHUD.m_iFOV, gHUD.m_pFogSettings.end, true);
		if (gHUD.viewFrustum.CullBox(m_pCurWater->mins, m_pCurWater->maxs) && !onlyrenderthiswater)
		{
			// YOU MUST DIE
			m_pCurWater->draw = false;
			continue;
		}

		for (int j = 0; j < m_iNumWaterData; j++)
		{
			if (m_pWaterEntInfo[j].entity == m_pCurWater->entity)
			{
				m_pWaterFogSettings.color = m_pWaterEntInfo[j].waterfog_color;
				m_pWaterFogSettings.start = m_pWaterEntInfo[j].waterfog_start;
				m_pWaterFogSettings.end = m_pWaterEntInfo[j].waterfog_end;
			}

		}

		SetupRefract();
		DrawScene(m_pViewParams, true);
		FinishRefract();

		if (ShouldReflect(i))
		{
			SetupReflect();
			DrawScene(&m_pWaterParams, false);
			FinishReflect();
		}
		if (ViewInWater())
			break;
	}

	for (int i = 0; i < m_iNumWaterEntities; i++)
	{
		m_pCurWater = &m_pWaterEntities[i];

		if (ViewInWater())
		{
			gHUD.m_pFogSettings = m_pWaterFogSettings;
			m_bViewInWater = true;
			break;
		}
	}

	if (m_pCvarWaterDebug->value)
		gEngfuncs.Con_Printf("A total of %d passes drawn for water shader.\n", m_iNumPasses);

	gBSPRenderer.m_bMirroring = false;
	glViewport(GL_ZERO, GL_ZERO, ScreenWidth, ScreenHeight);
}

/*
====================
DrawScene

====================
*/
void CWaterShader::DrawScene(ref_params_t* pparams, bool isrefracting)
{
	// Set world renderer
	gBSPRenderer.RendererRefDef(pparams);

	// Draw world
	gBSPRenderer.DrawNormalTriangles();

	if ((m_pCvarWaterShader->value > 1) || isrefracting)
	{
		for (int i = 0; i < gBSPRenderer.m_iNumRenderEntities; i++)
		{
			if (gBSPRenderer.m_pRenderEntities[i]->model->type != mod_studio || gBSPRenderer.m_pRenderEntities[i]->index == 0)
				continue;

			if (!gBSPRenderer.m_pRenderEntities[i]->player)
			{
				g_StudioRenderer.m_pCurrentEntity = gBSPRenderer.m_pRenderEntities[i];
				g_StudioRenderer.StudioDrawModel(STUDIO_RENDER);
			}
			else if (gBSPRenderer.m_pRenderEntities[i] != gEngfuncs.GetLocalPlayer())
			{
				entity_state_t* pPlayer = IEngineStudio.GetPlayerState((gBSPRenderer.m_pRenderEntities[i]->index - 1));
				g_StudioRenderer.m_pCurrentEntity = gBSPRenderer.m_pRenderEntities[i];
				g_StudioRenderer.StudioDrawPlayer(STUDIO_RENDER, pPlayer);
			}
		}
	}

	if ((m_pCvarWaterShader->value > 1) || isrefracting)
	{
		for (int i = 0; i < gBSPRenderer.m_iNumRenderEntities; i++)
		{
			if (gBSPRenderer.m_pRenderEntities[i]->model->type == mod_studio && gBSPRenderer.m_pRenderEntities[i]->index == 0)
			{
				g_StudioRenderer.m_pCurrentEntity = gBSPRenderer.m_pRenderEntities[i];
				g_StudioRenderer.StudioDrawModel(STUDIO_RENDER);
			}
		}
	}

	R_SaveGLStates();

	// Render any props
	gPropManager.RenderProps();

	// Render any transparent triangles
	gBSPRenderer.DrawTransparentTriangles();

	if ((m_pCvarWaterShader->value > 1) || isrefracting)
		gParticleEngine.DrawParticles();

	if (m_pCvarWaterDebug->value)
	{
		if (isrefracting)
		{
			gEngfuncs.Con_Printf("Water No %d Refract: %d wpolys, %d epolys, %d studio polys drawn\n",
				m_pCurWater->index, gBSPRenderer.m_iWorldPolyCounter, gBSPRenderer.m_iBrushPolyCounter,
				gBSPRenderer.m_iStudioPolyCounter);
		}
		else
		{
			gEngfuncs.Con_Printf("Water No %d Reflect: %d wpolys, %d epolys, %d studio polys drawn\n",
				m_pCurWater->index, gBSPRenderer.m_iWorldPolyCounter, gBSPRenderer.m_iBrushPolyCounter,
				gBSPRenderer.m_iStudioPolyCounter);
		}
	}

	R_RestoreGLStates();
	m_iNumPasses++;
}

/*
====================
SetupRefract

====================
*/
void CWaterShader::SetupRefract(void)
{
	glCullFace(GL_FRONT);
	glColor4f(GL_ONE, GL_ONE, GL_ONE, GL_ONE);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glRotatef(-90, 1, 0, 0); // put X going down
	glRotatef(90, 0, 0, 1);	 // put Z going up
	glRotatef(-m_pViewParams->viewangles[2], 1, 0, 0);
	glRotatef(-m_pViewParams->viewangles[0], 0, 1, 0);
	glRotatef(-m_pViewParams->viewangles[1], 0, 0, 1);
	glTranslatef(-m_vViewOrigin[0], -m_vViewOrigin[1], -m_vViewOrigin[2]);

	// bacontsu - custom per mirror resolution
	if (!m_pCurWater->res)
		glViewport(GL_ZERO, GL_ZERO, m_pCvarWaterResolution->value, m_pCvarWaterResolution->value);
	else
		glViewport(GL_ZERO, GL_ZERO, m_pCurWater->res, m_pCurWater->res);

	gHUD.m_pFogSettings = m_pWaterFogSettings;

	if (!ViewInWater())
	{
		gHUD.viewFrustum.SetExtraCullBox(m_pCurWater->entity->curstate.mins, m_pCurWater->entity->curstate.maxs);
		SetupClipping(m_pViewParams, false);
	}
	else
	{
		Vector vMins, vMaxs;
		VectorCopy(gBSPRenderer.m_pWorld->maxs, vMaxs);
		VectorCopy(gBSPRenderer.m_pWorld->mins, vMins);
		vMins.z = GetWaterOrigin().z;

		gHUD.viewFrustum.SetExtraCullBox(vMins, vMaxs);
		SetupClipping(m_pViewParams, true);
		//gHUD.m_pFogSettings = m_pMainFogSettings;
	}

	RenderFog();
}

/*
====================
FinishRefract

====================
*/
void CWaterShader::FinishRefract(void)
{
	// Save mirrored image
	glBindTexture(GL_TEXTURE_2D, m_pCurWater->refract);
	
	// bacontsu - custom per water surface res
	if (!m_pCurWater->res)
		glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, m_pCvarWaterResolution->value, m_pCvarWaterResolution->value, 0);
	else
		glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, m_pCurWater->res, m_pCurWater->res, 0);

	// Completely clear everything
	glClearColor(GL_ZERO, GL_ZERO, GL_ZERO, GL_ONE);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_ACCUM_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	// Restore modelview
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	gHUD.m_pFogSettings = m_pMainFogSettings;

	// Disable culling
	gHUD.viewFrustum.DisableExtraCullBox();
}

/*
====================
SetupReflect

====================
*/
void CWaterShader::SetupReflect(void)
{
	Vector vForward;
	Vector vMins, vMaxs;
	AngleVectors(m_pViewParams->viewangles, vForward, NULL, NULL);

	float flDist = abs(GetWaterOrigin().z - m_vViewOrigin[2]);
	VectorMA(m_vViewOrigin, -2 * flDist, m_pCurWater->wplane.normal, m_pWaterParams.vieworg);
	//VectorMASSE(m_vViewOrigin, -2 * flDist, m_pCurWater->wplane.normal, m_pWaterParams.vieworg);

	flDist = DotProduct(vForward, -m_pCurWater->wplane.normal);
	VectorMA(vForward, -2 * flDist, -m_pCurWater->wplane.normal, vForward);
	//VectorMASSE(vForward, -2 * flDist, -m_pCurWater->wplane.normal, vForward);

	m_pWaterParams.viewangles[0] = -asin(vForward[2]) / M_PI * 180;
	m_pWaterParams.viewangles[1] = atan2(vForward[1], vForward[0]) / M_PI * 180;
	m_pWaterParams.viewangles[2] = -m_pViewParams->viewangles[2];

	AngleVectors(m_pWaterParams.viewangles, m_pWaterParams.forward, m_pWaterParams.right, m_pWaterParams.up);
	VectorCopy(m_pWaterParams.viewangles, m_pWaterParams.cl_viewangles);

	glCullFace(GL_FRONT);
	glColor4f(GL_ONE, GL_ONE, GL_ONE, GL_ONE);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glRotatef(-90, 1, 0, 0); // put X going down
	glRotatef(90, 0, 0, 1);	 // put Z going up
	glRotatef(-m_pWaterParams.viewangles[2], 1, 0, 0);
	glRotatef(-m_pWaterParams.viewangles[0], 0, 1, 0);
	glRotatef(-m_pWaterParams.viewangles[1], 0, 0, 1);
	glTranslatef(-m_pWaterParams.vieworg[0], -m_pWaterParams.vieworg[1], -m_pWaterParams.vieworg[2]);

	// bacontsu - custom per mirror resolution
	if (!m_pCurWater->res)
		glViewport(GL_ZERO, GL_ZERO, m_pCvarWaterResolution->value, m_pCvarWaterResolution->value);
	else
		glViewport(GL_ZERO, GL_ZERO, m_pCurWater->res, m_pCurWater->res);

	// Cull everything below the water plane
	VectorCopy(gBSPRenderer.m_pWorld->maxs, vMaxs);
	VectorCopy(gBSPRenderer.m_pWorld->mins, vMins);
	vMins.z = GetWaterOrigin().z;

	gHUD.viewFrustum.SetExtraCullBox(vMins, vMaxs);
	SetupClipping(&m_pWaterParams, true);
	RenderFog();
}

/*
====================
FinishReflect

====================
*/
void CWaterShader::FinishReflect(void)
{
	// Save mirrored image
	glBindTexture(GL_TEXTURE_2D, m_pCurWater->reflect);

	// bacontsu - custom per water surface res
	if (!m_pCurWater->res)
		glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, m_pCvarWaterResolution->value, m_pCvarWaterResolution->value, 0);
	else
		glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, m_pCurWater->res, m_pCurWater->res, 0);

	// Completely clear everything
	glClearColor(GL_ZERO, GL_ZERO, GL_ZERO, GL_ONE);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_ACCUM_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();

	// Restore modelview
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	// Turn culling off
	gHUD.viewFrustum.DisableExtraCullBox();
}

void MultiplyMatrices4x4_OpenGLStyle(float* out, const float* a, const float* b)
{
	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 4; j++)
		{
			out[j * 4 + i] = a[0 * 4 + i] * b[j * 4 + 0] +
				a[1 * 4 + i] * b[j * 4 + 1] +
				a[2 * 4 + i] * b[j * 4 + 2] +
				a[3 * 4 + i] * b[j * 4 + 3];
		}
	}
}

/*
====================
DrawWater

====================
*/
void CWaterShader::DrawWater(void)
{
	if (m_pCvarWaterShader->value < 1)
		return;

	if (!gBSPRenderer.m_bShaderSupport)
		return;

	if (!m_iNumWaterEntities)
		return;

	float flMatrix[16];
	float flTime = gEngfuncs.GetClientTime() * 0.5;

	gBSPRenderer.EnableVertexArray();
	gBSPRenderer.SetTexPointer(0, TC_TEXTURE);

	glUseProgram(m_WaterFragmentShader);

	glGetFloatv(GL_PROJECTION_MATRIX, flMatrix);
	glUniformMatrix4fv(glGetUniformLocation(m_WaterFragmentShader, "projectionMatrix"), 1, GL_FALSE, flMatrix);

	glUniform1i(glGetUniformLocation(m_WaterFragmentShader, "underwater"), m_bViewInWater? 1 : 0);


	bool onlyrenderthiswater = false;

	for (int i = 0; i < m_iNumWaterEntities; i++)
	{
		m_pCurWater = &m_pWaterEntities[i];

		if (ViewInWater())
		{
			onlyrenderthiswater = true;
		}
		else if (!m_pWaterEntities[i].draw)
			continue;

		if (gHUD.viewFrustum.CullBox(m_pCurWater->mins, m_pCurWater->maxs) && !onlyrenderthiswater)
			continue;

		for (int j = 0; j < m_iNumWaterData; j++)
		{

			if (m_pWaterEntInfo[j].entity == m_pCurWater->entity)
			{
				m_pWaterFogSettings.color = m_pWaterEntInfo[j].waterfog_color;
				m_pWaterFogSettings.start = m_pWaterEntInfo[j].waterfog_start;
				m_pWaterFogSettings.end = m_pWaterEntInfo[j].waterfog_end;

				glUniform1f(glGetUniformLocation(m_WaterFragmentShader, "normalscale"), m_pWaterEntInfo[j].normal_scale);

				glUniform1f(glGetUniformLocation(m_WaterFragmentShader, "watertex_scale"), m_pWaterEntInfo[j].watertex_scale);

				glUniform1f(glGetUniformLocation(m_WaterFragmentShader, "refraction_scale"), m_pWaterEntInfo[j].refraction_scale);

				glUniform1f(glGetUniformLocation(m_WaterFragmentShader, "reflection_scale"), m_pWaterEntInfo[j].reflection_scale);

				glUniform1f(glGetUniformLocation(m_WaterFragmentShader, "m_flFresnelTerm"), m_pWaterEntInfo[j].fresnel);
			}

		}

		float modelMatrix[16];
		float modelViewMatrix[16];
		Vector origin = m_pCurWater->entity->curstate.origin;

		// Identity + translation matrix
		// Column-major order
		memset(modelMatrix, 0, sizeof(modelMatrix));
		modelMatrix[0] = 1.0f;
		modelMatrix[5] = 1.0f;
		modelMatrix[10] = 1.0f;
		modelMatrix[15] = 1.0f;
		modelMatrix[12] = origin.x;
		modelMatrix[13] = origin.y;
		modelMatrix[14] = origin.z;

		glGetFloatv(GL_MODELVIEW_MATRIX, flMatrix);

		MultiplyMatrices4x4_OpenGLStyle(modelViewMatrix, flMatrix, modelMatrix);

		glUniformMatrix4fv(glGetUniformLocation(m_WaterFragmentShader, "modelMatrix"), 1, GL_FALSE, modelViewMatrix);

		if (ViewInWater())
			glCullFace(GL_BACK);

		glUniform3fv(glGetUniformLocation(m_WaterFragmentShader, "renderorigin"), 1, gBSPRenderer.m_vRenderOrigin);
		glUniform3fv(glGetUniformLocation(m_WaterFragmentShader, "waterfog"), 1, m_pWaterFogSettings.color);
		glUniform1f(glGetUniformLocation(m_WaterFragmentShader, "flTime"), flTime);
		glUniform1i(glGetUniformLocation(m_WaterFragmentShader, "texture0"), 0);
		glUniform1i(glGetUniformLocation(m_WaterFragmentShader, "texture1"), 1);
		glUniform1i(glGetUniformLocation(m_WaterFragmentShader, "texture2"), 2);
		glUniform1i(glGetUniformLocation(m_WaterFragmentShader, "texture3"), 3); //original water texture

		gBSPRenderer.Bind2DTexture(GL_TEXTURE0_ARB, m_pNormalTexture->iIndex);
		gBSPRenderer.Bind2DTexture(GL_TEXTURE1_ARB, m_pCurWater->refract);
		//gBSPRenderer.Bind2DTexture(GL_TEXTURE3_ARB, m_pCurWater->surfaces[0]->texinfo->texture->gl_texturenum);
		int idx = GL_TEXTURE3_ARB - GL_TEXTURE0_ARB;
		gBSPRenderer.m_uiCurrentBinds[idx] = m_pCurWater->surfaces[0]->texinfo->texture->gl_texturenum;
		glActiveTextureARB(GL_TEXTURE3_ARB);
		glBindTexture(GL_TEXTURE_2D, m_pCurWater->surfaces[0]->texinfo->texture->gl_texturenum);
		glEnable(GL_TEXTURE_2D);

		// Optimization: Try and find a water entity on the same z coord
		//int j = 0;
		//for (; j < i; j++)
		//{
		//	if (m_pWaterEntities[j].draw)
		//	{
		//		if (GetWaterOrigin(&m_pWaterEntities[j]).z == GetWaterOrigin().z)
		//		{
		//			gBSPRenderer.Bind2DTexture(GL_TEXTURE2_ARB, m_pWaterEntities[j].reflect);
		//			break;
		//		}
		//	}
		//}
		//
		//if (j == i)
			gBSPRenderer.Bind2DTexture(GL_TEXTURE2_ARB, m_pCurWater->reflect);

		for (int j = 0; j < m_pCurWater->numsurfaces; j++)
		{
			gBSPRenderer.DrawPolyFromArray(m_pCurWater->surfaces[j]->polys);
		}
		if (onlyrenderthiswater)
			break;
	}

	glCullFace(GL_FRONT);

	glUseProgram(0);

	int idx = GL_TEXTURE3_ARB - GL_TEXTURE0_ARB;
	gBSPRenderer.m_uiCurrentBinds[idx] = 0;
	glActiveTextureARB(GL_TEXTURE3_ARB);
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);

	gBSPRenderer.DisableVertexArray();
}

/*
====================
GetWaterOrigin

====================
*/
Vector CWaterShader::GetWaterOrigin(cl_water_t* pwater)
{
	if (pwater)
		return pwater->origin + pwater->entity->curstate.origin;
	else
		return m_pCurWater->origin + m_pCurWater->entity->curstate.origin;
}

int CWaterShader::MsgWaterInfo(const char* pszName, int iSize, void* pbuf)
{
	//order:
	// entindex
	// waterfog_color
	// waterfog_start
	// waterfog_end
	// watertex_scale
	// normal_scale
	// fresnel

	cl_waterinfo_t *waterinfo = &m_pWaterEntInfo[m_iNumWaterData];
	m_iNumWaterData++;
	BEGIN_READ(pbuf, iSize);
	waterinfo->entity = gEngfuncs.GetEntityByIndex(READ_LONG());
	waterinfo->waterfog_color.x = READ_FLOAT() / 255;
	waterinfo->waterfog_color.y = READ_FLOAT() / 255;
	waterinfo->waterfog_color.z = READ_FLOAT() / 255;
	waterinfo->waterfog_start = READ_LONG();
	waterinfo->waterfog_end = READ_LONG();
	waterinfo->watertex_scale = atof(READ_STRING());
	waterinfo->refraction_scale = atof(READ_STRING());
	waterinfo->reflection_scale = atof(READ_STRING());
	waterinfo->normal_scale = atof(READ_STRING());
	waterinfo->fresnel = atof(READ_STRING());
	if (waterinfo->waterfog_color == Vector(0, 0, 0))
		waterinfo->waterfog_color = Vector(70.f / 255.f, 155.f / 255.f, 155.f / 255.f); //default water fog color

	if (!waterinfo->waterfog_start)
		waterinfo->waterfog_start = 200;

	if (!waterinfo->waterfog_end)
		waterinfo->waterfog_end = 600;

	if (!waterinfo->watertex_scale)
		waterinfo->watertex_scale = 0.9;

	if (!waterinfo->refraction_scale)
		waterinfo->refraction_scale = 1.4;

	if (!waterinfo->reflection_scale)
		waterinfo->reflection_scale = 2.4;

	if (!waterinfo->normal_scale)
		waterinfo->normal_scale = 0.13;

	if (!waterinfo->fresnel)
		waterinfo->fresnel = 0.5;

	return 1;
}