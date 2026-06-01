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

#include "hud.h"
#include "cl_util.h"

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

#include "SDL2/SDL.h"

#include "propmanager.h"
#include "particle_engine.h"
#include "bsprenderer.h"
#include "watershader.h"
#include "mirrormanager.h"
#include "goldsrc_beamrenderer.h"

#include "opengl_utils/GL_FBO.h"
#include "opengl_utils/GL_ShaderProgram.h"
#include "opengl_utils/GL_StateHandler.h"
#include "opengl_utils/GL_TextureHandler.h"
#include "opengl_utils/GL_Buffers.h"
#include "opengl_utils/GL_VertexArrayObject.h"
#include "opengl_utils/GL_Mesh.h"

#include "r_efx.h"
#include "r_studioint.h"
#include "studio_util.h"
#include "event_api.h"
#include "event_args.h"

#include "StudioModelRenderer.h"

CWaterShader gWaterShader;

//===========================================
// OPENGL START
//
//===========================================

#include "glshaders/water_glsl.h"


static GL_ShaderProgram s_WaterFragmentShader(water_depth_vertex, water_fragment_water_regular);

static GL_FBOHandler* s_waterFBO;
static GL_RBOHandler* s_waterDepthBuffer;

enum watershader_uniforms
{
	watershader_renderorigin,

	watershader_projviewmodelmatrix,

	watershader_underwater,

	watershader_waterfog, // program.local[1] = (r, g, b)
	watershader_fogstart,
	watershader_fogend,
	watershader_m_flFresnelTerm, // program.local[2] = float
	watershader_flTime,			 // program.local[3] = client time

	watershader_normalscale,
	watershader_watertex_scale,
	watershader_refraction_scale,
	watershader_reflection_scale,

	_watershader_locsize

}; static GLuint s_WaterShader_locs[_watershader_locsize];

//===========================================
// OPENGL END
//
//===========================================

/*
====================
Init

====================
*/
void CWaterShader::Init(void)
{
	// Set up cvar
	m_pCvarWaterShader = gEngfuncs.pfnRegisterVariable("r_watershader", "1", FCVAR_ARCHIVE);
	m_pCvarWaterDebug = gEngfuncs.pfnRegisterVariable("r_watershader_debug", "0", 0);
	m_pCvarWaterResolution = gEngfuncs.pfnRegisterVariable("r_waterreflection_res", "512", FCVAR_ARCHIVE); // MAX:1024

	m_pCvarWaterFogStart = gEngfuncs.pfnRegisterVariable("r_watershader_fogstart", "200", 0);
	m_pCvarWaterFogEnd = gEngfuncs.pfnRegisterVariable("r_watershader_fogend", "600", 0);
	m_pCvarWaterTexscale = gEngfuncs.pfnRegisterVariable("r_watershader_texscale", "0.9", 0);
	m_pCvarWaterRefractScale = gEngfuncs.pfnRegisterVariable("r_watershader_refractscale", "1.4", 0);
	m_pCvarWaterReflectScale = gEngfuncs.pfnRegisterVariable("r_watershader_reflectscale", "2.4", 0);
	m_pCvarWaterNormalScale = gEngfuncs.pfnRegisterVariable("r_watershader_normalscale", "0.13", 0);
	m_pCvarWaterFresnel = gEngfuncs.pfnRegisterVariable("r_watershader_fresnel", "0.5", 0);

	m_pCvarWaterForceExpensive = gEngfuncs.pfnRegisterVariable("r_waterforceexpensive", "1", FCVAR_ARCHIVE);
	m_pCvarWaterForceReflectEntities = gEngfuncs.pfnRegisterVariable("r_waterforcereflectentities", "1", FCVAR_ARCHIVE);

	s_WaterShader_locs[watershader_renderorigin] = s_WaterFragmentShader.GetUniformLoc("renderorigin");

	s_WaterShader_locs[watershader_projviewmodelmatrix] = s_WaterFragmentShader.GetUniformLoc("projviewmodelmatrix");

	s_WaterShader_locs[watershader_underwater] = s_WaterFragmentShader.GetUniformLoc("underwater");

	s_WaterShader_locs[watershader_waterfog] = s_WaterFragmentShader.GetUniformLoc("waterfog");
	s_WaterShader_locs[watershader_fogstart] = s_WaterFragmentShader.GetUniformLoc("fogstart");
	s_WaterShader_locs[watershader_fogend] = s_WaterFragmentShader.GetUniformLoc("fogend");
	s_WaterShader_locs[watershader_m_flFresnelTerm] = s_WaterFragmentShader.GetUniformLoc("m_flFresnelTerm");
	s_WaterShader_locs[watershader_flTime] = s_WaterFragmentShader.GetUniformLoc("flTime");

	s_WaterShader_locs[watershader_normalscale] = s_WaterFragmentShader.GetUniformLoc("normalscale");
	s_WaterShader_locs[watershader_watertex_scale] = s_WaterFragmentShader.GetUniformLoc("watertex_scale");
	//s_WaterShader_locs[watershader_refraction_scale] = s_WaterFragmentShader.GetUniformLoc("refraction_scale");
	//s_WaterShader_locs[watershader_reflection_scale] = s_WaterFragmentShader.GetUniformLoc("reflection_scale");


	s_WaterFragmentShader.Bind();
	s_WaterFragmentShader.Uniform1i(s_WaterFragmentShader.GetUniformLoc("texture0"), 0);
	s_WaterFragmentShader.Uniform1i(s_WaterFragmentShader.GetUniformLoc("texture1"), 1);
	s_WaterFragmentShader.Uniform1i(s_WaterFragmentShader.GetUniformLoc("texture2"), 2);
	s_WaterFragmentShader.Uniform1i(s_WaterFragmentShader.GetUniformLoc("texture3"), 3);

	GL_ShaderProgram::ResetShaderBind();
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
		delete m_pWaterEntities[i].reflect;
		delete m_pWaterEntities[i].refract;
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
	// Load texture
	m_pNormalTexture = gTextureLoader.LoadTexture("gfx/textures/watershader.tga");

	m_iNumPasses = 0;

	if (!m_pNormalTexture)
	{
		gEngfuncs.pfnClientCmd("escape\n");
		MessageBox(NULL, "VIDEO ERROR: Could not load 'gfx/textures/watershader.tga'!\n", "ERROR", MB_OK);
		gEngfuncs.pfnClientCmd("quit\n");
	}

	if (m_iLastWaterRes != m_pCvarWaterResolution->value)
	{
		m_iLastWaterRes = m_pCvarWaterResolution->value;
		delete s_waterFBO;
		delete s_waterDepthBuffer;

		s_waterFBO = nullptr;
		s_waterDepthBuffer = nullptr;
	}

	if (!s_waterFBO && !s_waterDepthBuffer)
	{
		s_waterFBO = new GL_FBOHandler();
		s_waterDepthBuffer = new GL_RBOHandler();

		s_waterFBO->Bind(GL_FBOHandler::Framebuffer);
		s_waterDepthBuffer->Bind();
		s_waterDepthBuffer->RenderBufferStorage(GL_DEPTH_COMPONENT16, m_pCvarWaterResolution->value, m_pCvarWaterResolution->value);
		s_waterFBO->FramebufferRenderbuffer(GL_FBOHandler::Framebuffer, GL_FBOHandler::DepthAttachment, s_waterDepthBuffer);

		GL_FBOHandler::ResetToMainFBO();
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
	//no script, every func_water has their own fog settings
	m_pWaterFogSettings.active = true;
}

/*
====================
ShouldReflect

====================
*/
bool CWaterShader::ShouldReflect(int index)
{
	if (ViewInWater())
		return true;

	// Optimization: Try and find a water entity on the same z coord
	//for (int i = 0; i < index; i++)
	//{
	//	 if (m_pWaterEntities[i].draw && m_pWaterEntities[i].rendered)
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
	clientmsurface_t* psurfaces = BSPWorld_Model::m_pWorldSurfaces + entity->model->firstmodelsurface;
	for (int i = 0; i < entity->model->nummodelsurfaces; i++)
	{
		int j = 0;
		for (; j < psurfaces[i].polys->numverts; j++)
		{
			if (psurfaces[i].polys->verts[0][2] != (entity->curstate.maxs.z - 1))
				break;
		}

		//if (j != psurfaces[i].polys->numverts)
		//	continue;

		if (psurfaces[i].flags & SURF_PLANEBACK)
			continue;

		if (psurfaces[i].plane->normal[2] != 1)
			continue;

		isurfacecount++;
	}

	// Allocate array of pointers
	pWater->surfaces = (clientmsurface_t**)malloc(sizeof(clientmsurface_t*) * isurfacecount);

	for (int i = 0; i < entity->model->nummodelsurfaces; i++)
	{
		int j = 0;
		for (; j < psurfaces[i].polys->numverts; j++)
		{
			if (psurfaces[i].polys->verts[0][2] != (entity->curstate.maxs.z - 1))
				break;
		}

		//if (j != psurfaces[i].polys->numverts)
		//	continue;

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

	GL_TextureHandler::gl_texturecreationinfo_t waternormal_texinfo =
	{
			std::string(), GL_TextureHandler::_2DTexture, GL_RGB8, m_pCvarWaterResolution->value, m_pCvarWaterResolution->value, 0, GL_RGB, GL_UNSIGNED_BYTE
	};

	pWater->reflect = new GL_TextureHandler(&waternormal_texinfo);
	pWater->refract = new GL_TextureHandler(&waternormal_texinfo);

	pWater->origin[0] = pWater->entity->curstate.origin[0] + ( (pWater->mins[0] + pWater->maxs[0]) * 0.5f );
	pWater->origin[1] = pWater->entity->curstate.origin[1] + ( (pWater->mins[1] + pWater->maxs[1]) * 0.5f );
	pWater->origin[2] = pWater->entity->curstate.origin[2] + ( (pWater->mins[2] + pWater->maxs[2]) * 0.5f );
}

glm::mat4 oldviewmatrix;

/*
====================
SetupClipping

====================
*/
void CWaterShader::SetupClipping(ref_params_t* pparams, bool negative)
{
	mplane_t plane = m_pCurWater->wplane;
	float z_extent = m_pCurWater->maxs.z - (m_pCurWater->mins[2] + m_pCurWater->maxs[2]) * 0.5f;
	z_extent -= 4; //small bias to avoid exposing backface polys
	plane.dist = abs(GetWaterOrigin().z + z_extent);
	if (!negative)
	{
		plane.normal = -plane.normal;
	}

	R_SetClippingPlane(plane);
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
		mins[i] = m_pCurWater->entity->curstate.origin[i] + m_pCurWater->entity->model->mins[i];
		maxs[i] = m_pCurWater->entity->curstate.origin[i] + m_pCurWater->entity->model->maxs[i];
	}

	if (m_pCvarWaterShader->value < 1)
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

===================='
*/
void CWaterShader::DrawWaterPasses(ref_params_t* pparams)
{
	if (m_pCvarWaterShader->value < 1)
	{
		// get the fog in atleast
		if (gHUD.m_pFogSettings.color != m_pWaterFogSettings.color)
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

	if (!m_iNumWaterEntities)
		return;

	m_iNumPasses = NULL;
	m_bViewInWater = false;
	m_pViewParams = pparams;
	m_pMainFogSettings = gHUD.m_pFogSettings;
	gBSPRenderer.m_bMirroring = true;

	VectorCopy(pparams->vieworg, m_vViewOrigin);
	memcpy(&m_pWaterParams, m_pViewParams, sizeof(ref_params_t));

	bool onlyrenderthiswater = false;

	for (int i = 0; i < m_iNumWaterEntities; i++)
		m_pWaterEntities[i].rendered = false;

	FrustumCheck oldfrustum = gHUD.viewFrustum;

	s_waterFBO->Bind(GL_FBOHandler::Framebuffer);

	glViewport(0, 0, m_pCvarWaterResolution->value, m_pCvarWaterResolution->value);


	for (int i = 0; i < m_iNumWaterEntities; i++)
	{
		m_pCurWater = &m_pWaterEntities[i];

		if (ViewInWater())
		{
			onlyrenderthiswater = true;
		}
		if (!m_pCurWater->draw)
			continue;

		m_pCurWater->origin[0] = m_pCurWater->entity->curstate.origin[0] + ( (m_pCurWater->mins[0] + m_pCurWater->maxs[0]) * 0.5f);
		m_pCurWater->origin[1] = m_pCurWater->entity->curstate.origin[1] + ( (m_pCurWater->mins[1] + m_pCurWater->maxs[1]) * 0.5f);
		m_pCurWater->origin[2] = m_pCurWater->entity->curstate.origin[2] + ( (m_pCurWater->mins[2] + m_pCurWater->maxs[2]) * 0.5f);

		gHUD.viewFrustum.SetFrustum(pparams->viewangles, pparams->vieworg, gHUD.m_iFOV, gHUD.m_pFogSettings.end, true);
		if (gHUD.viewFrustum.CullBox(m_pCurWater->mins + m_pCurWater->entity->curstate.origin, m_pCurWater->maxs + m_pCurWater->entity->curstate.origin) && !onlyrenderthiswater)
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
		if (ShouldReflect(i))
		{
			SetupRefract();
			DrawScene(m_pViewParams, true);
			FinishRefract();
		}

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

	gHUD.viewFrustum = oldfrustum;

	gBSPRenderer.m_bMirroring = false;

	GL_FBOHandler::ResetToMainFBO();
	glViewport(0, 0, ScreenWidth, ScreenHeight);

	g_GlobalGLState.SetBlend(false);
}

/*
====================
DrawScene

====================
*/
void CWaterShader::DrawScene(ref_params_t* pparams, bool isrefracting)
{
	R_SetupView(pparams);

	//glClearColor(gHUD.m_pFogSettings.color[0], gHUD.m_pFogSettings.color[1], gHUD.m_pFogSettings.color[2], 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Draw world
	if(m_pCvarWaterForceExpensive->value)
		gBSPRenderer.DrawNormalTriangles_Cheap(true, m_pCvarWaterForceReflectEntities->value);
	else
		gBSPRenderer.DrawNormalTriangles_Cheap(false);

	if(m_pCvarWaterForceReflectEntities->value)
	{
		g_StudioRenderer.StudioDrawModels(false);

		gPropManager.RenderProps();
	}

	// Render any props
	//gPropManager.RenderProps();

	// Render any transparent triangles
	//gBSPRenderer.DrawTransparentTriangles();

	//if ((m_pCvarWaterShader->value > 1) || isrefracting)
	//	gParticleEngine.DrawParticles();

	m_iNumPasses++;
}

/*
====================
SetupRefract

====================
*/
void CWaterShader::SetupRefract(void)
{
	s_waterFBO->FramebufferTexture2D(GL_FBOHandler::Framebuffer, GL_FBOHandler::ColorAttachment, GL_TEXTURE_2D, m_pCurWater->refract->GetTextureID(), 0);

	// Completely clear everything
	glClearColor(GL_ZERO, GL_ZERO, GL_ZERO, GL_ONE);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_ACCUM_BUFFER_BIT);

	if (!ViewInWater())
	{
		gHUD.viewFrustum.SetExtraCullBox(m_pCurWater->mins + m_pCurWater->entity->curstate.origin, m_pCurWater->maxs + m_pCurWater->entity->curstate.origin);
	}
	else
	{
		Vector vMins, vMaxs;
		VectorCopy(engine_cl->worldmodel->maxs, vMaxs);
		VectorCopy(engine_cl->worldmodel->mins, vMins);
		vMins.z = GetWaterOrigin().z;

		gHUD.viewFrustum.SetExtraCullBox(vMins, vMaxs);
		// gHUD.m_pFogSettings = m_pMainFogSettings;
	}
}

/*
====================
FinishRefract

====================
*/
void CWaterShader::FinishRefract(void)
{
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

	m_pViewParams->viewangles = m_pViewParams->viewangles;

	m_pWaterParams = *m_pViewParams;
	m_pWaterParams.viewangles.x *= -1;
	m_pWaterParams.viewangles.z *= -1;

	float z_extent = m_pCurWater->maxs.z - (m_pCurWater->mins[2] + m_pCurWater->maxs[2]) * 0.5f;

	float flDist = abs(GetWaterOrigin().z + z_extent - m_vViewOrigin[2]);
	VectorMA(m_vViewOrigin, 2 * -flDist, m_pCurWater->wplane.normal, m_pWaterParams.vieworg);

	AngleVectors(m_pWaterParams.viewangles, &m_pWaterParams.forward, &m_pWaterParams.right, &m_pWaterParams.up);
	VectorCopy(m_pWaterParams.viewangles, m_pWaterParams.cl_viewangles);

	s_waterFBO->FramebufferTexture2D(GL_FBOHandler::Framebuffer, GL_FBOHandler::ColorAttachment, GL_TEXTURE_2D, m_pCurWater->reflect->GetTextureID(), 0);

	// Cull everything below the water plane
	VectorCopy(engine_cl->worldmodel->maxs, vMaxs);
	VectorCopy(engine_cl->worldmodel->mins, vMins);
	vMins.z = GetWaterOrigin().z;

	gHUD.viewFrustum.SetExtraCullBox(vMins, vMaxs);
	SetupClipping(&m_pWaterParams, true);
	
	oldviewmatrix = gBSPRenderer.m_ViewMatrix;

	auto& m_vViewAngles = m_pWaterParams.viewangles;
	auto &m_RefParams = gBSPRenderer.m_RefParams;
	auto& m_vRenderOrigin = m_pWaterParams.vieworg;

	glm::vec3 viewangles = glm::vec3(m_vViewAngles.x, m_vViewAngles.y, m_vViewAngles.z);
	Vector forward_, up_;
	AngleVectors(Vector(viewangles.x, viewangles.y, viewangles.z), &forward_, nullptr, &up_);

	glm::vec3 forward = glm::vec3(forward_.x, forward_.y, forward_.z);

	glm::vec3 cameraPos = glm::vec3(m_vRenderOrigin.x, m_vRenderOrigin.y, m_vRenderOrigin.z);
	glm::vec3 cameraTarget = cameraPos + forward;
	glm::vec3 cameraUp = glm::vec3(up_.x, up_.y, up_.z);

	gBSPRenderer.m_ViewMatrix = glm::lookAt(cameraPos, cameraTarget, cameraUp);
}

/*
====================
FinishReflect

====================
*/
void CWaterShader::FinishReflect(void)
{
	// Turn culling off
	gHUD.viewFrustum.DisableExtraCullBox();

	gBSPRenderer.m_ViewMatrix = oldviewmatrix;
	R_SetupView(m_pViewParams);

	R_DisableClippingPlane();

	m_pCurWater->rendered = true;
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

	if (!m_iNumWaterEntities)
		return;

	float flTime = engine_cl->time * 0.5;

	s_WaterFragmentShader.Bind();

	s_WaterFragmentShader.Uniform3fv(s_WaterShader_locs[watershader_renderorigin], 1, gBSPRenderer.m_vRenderOrigin);
	s_WaterFragmentShader.Uniform1f(s_WaterShader_locs[watershader_flTime], flTime);
	s_WaterFragmentShader.Uniform1i(s_WaterShader_locs[watershader_underwater], m_bViewInWater ? 1 : 0);

	gBSPRenderer.m_pBSPMesh->BindMesh();

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

		if (gHUD.viewFrustum.CullBox(m_pCurWater->mins + m_pCurWater->entity->curstate.origin, m_pCurWater->maxs + m_pCurWater->entity->curstate.origin) && !onlyrenderthiswater)
			continue;

		for (int j = 0; j < m_iNumWaterData; j++)
		{

			if (m_pWaterEntInfo[j].entity == m_pCurWater->entity)
			{
				m_pWaterFogSettings.color = m_pWaterEntInfo[j].waterfog_color;
				m_pWaterFogSettings.start = m_pWaterEntInfo[j].waterfog_start;
				m_pWaterFogSettings.end = m_pWaterEntInfo[j].waterfog_end;

				s_WaterFragmentShader.Uniform1f(s_WaterShader_locs[watershader_normalscale], m_pWaterEntInfo[j].normal_scale);
				s_WaterFragmentShader.Uniform1f(s_WaterShader_locs[watershader_watertex_scale], m_pWaterEntInfo[j].watertex_scale);
				//s_WaterFragmentShader.Uniform1f(s_WaterShader_locs[watershader_refraction_scale], m_pWaterEntInfo[j].refraction_scale);
				//s_WaterFragmentShader.Uniform1f(s_WaterShader_locs[watershader_reflection_scale], m_pWaterEntInfo[j].reflection_scale);
				s_WaterFragmentShader.Uniform1f(s_WaterShader_locs[watershader_m_flFresnelTerm], m_pWaterEntInfo[j].fresnel);
				break;
			}
		}

		if (ViewInWater())
			glCullFace(GL_BACK);

		glm::mat4 modelmatrix = glm::mat4(1.0f);
		modelmatrix = glm::translate(modelmatrix, glm::vec3(m_pCurWater->entity->curstate.origin.x, m_pCurWater->entity->curstate.origin.y, m_pCurWater->entity->curstate.origin.z));
		modelmatrix = glm::rotate(modelmatrix, glm::radians(m_pCurWater->entity->curstate.angles.y), glm::vec3(0.0f, 0.0f, 1.0f));
		modelmatrix = glm::rotate(modelmatrix, glm::radians(m_pCurWater->entity->curstate.angles.x), glm::vec3(0.0f, 1.0f, 0.0f));
		modelmatrix = glm::rotate(modelmatrix, glm::radians(m_pCurWater->entity->curstate.angles.z), glm::vec3(1.0f, 0.0f, 0.0f));

		s_WaterFragmentShader.UniformMatrix4fv(s_WaterShader_locs[watershader_projviewmodelmatrix], 1, GL_FALSE, glm::value_ptr(gBSPRenderer.m_ProjectionMatrix * gBSPRenderer.m_ViewMatrix * modelmatrix));
		s_WaterFragmentShader.Uniform3fv(s_WaterShader_locs[watershader_waterfog], 1, m_pWaterFogSettings.color);
		s_WaterFragmentShader.Uniform1f(s_WaterShader_locs[watershader_fogstart], m_pWaterFogSettings.start);
		s_WaterFragmentShader.Uniform1f(s_WaterShader_locs[watershader_fogend], m_pWaterFogSettings.end);

		gBSPRenderer.BindGLTexture(GL_TEXTURE0, m_pNormalTexture->iIndex);
		gBSPRenderer.BindGLTexture(GL_TEXTURE3, m_pCurWater->surfaces[0]->texinfo->texture->gl_texturenum);

		// Optimization: Try and find a water entity on the same z coord
		//int j = 0;
		//for (; j < i; j++)
		//{
		//	if (m_pWaterEntities[j].draw && m_pWaterEntities[j].rendered)
		//	{
		//		if (GetWaterOrigin(&m_pWaterEntities[j]).z == GetWaterOrigin().z)
		//		{
		//			gBSPRenderer.BindGLTexture(GL_TEXTURE1, m_pWaterEntities[j].refract->GetTextureID());
		//			gBSPRenderer.BindGLTexture(GL_TEXTURE2, m_pWaterEntities[j].reflect->GetTextureID());
		//			break;
		//		}
		//	}
		//}
		//
		//if (j == i)
		//{
			gBSPRenderer.BindGLTexture(GL_TEXTURE2, m_pCurWater->reflect->GetTextureID());
			gBSPRenderer.BindGLTexture(GL_TEXTURE1, m_pCurWater->refract->GetTextureID());
		//}

		for (int j = 0; j < m_pCurWater->numsurfaces; j++)
		{
			gBSPRenderer.DrawPolyFromArray(BSPWorld_Model::m_pWorldSurfaces, m_pCurWater->surfaces[j]);
		}
		if (onlyrenderthiswater)
			break;
	}

	glCullFace(GL_FRONT);

	GL_ShaderProgram::ResetShaderBind();

	gBSPRenderer.BindGLTexture(GL_TEXTURE3, 0);
	gBSPRenderer.BindGLTexture(GL_TEXTURE4, 0);
}

/*
====================
GetWaterOrigin

====================
*/
Vector CWaterShader::GetWaterOrigin(cl_water_t* pwater)
{
	if (pwater)
		return pwater->origin;
	else
		return m_pCurWater->origin;
}

int CWaterShader::MsgWaterInfo(const char* pszName, int iSize, void* pbuf)
{
	// order:
	//  entindex
	//  waterfog_color
	//  waterfog_start
	//  waterfog_end
	//  watertex_scale
	//  normal_scale
	//  fresnel

	cl_waterinfo_t* waterinfo = &m_pWaterEntInfo[m_iNumWaterData];
	m_iNumWaterData++;
	BEGIN_READ(pbuf, iSize);
	waterinfo->entity = gEngfuncs.GetEntityByIndex(READ_SHORT());
	waterinfo->waterfog_color.x = (float)READ_BYTE() / 255.f;
	waterinfo->waterfog_color.y = (float)READ_BYTE() / 255.f;
	waterinfo->waterfog_color.z = (float)READ_BYTE() / 255.f;
	waterinfo->waterfog_start = READ_SHORT();
	waterinfo->waterfog_end = READ_SHORT();
	waterinfo->watertex_scale = READ_FLOAT();
	waterinfo->refraction_scale = READ_FLOAT();
	waterinfo->reflection_scale = READ_FLOAT();
	waterinfo->normal_scale = READ_FLOAT();
	waterinfo->fresnel = READ_FLOAT();
	if (waterinfo->waterfog_color == Vector(0, 0, 0))
		waterinfo->waterfog_color = Vector(70.f / 255.f, 155.f / 255.f, 155.f / 255.f); // default water fog color

	if (!waterinfo->waterfog_start)
		waterinfo->waterfog_start = m_pCvarWaterFogStart->value;

	if (!waterinfo->waterfog_end)
		waterinfo->waterfog_end = m_pCvarWaterFogEnd->value;

	if (!waterinfo->watertex_scale)
		waterinfo->watertex_scale = m_pCvarWaterTexscale->value;

	if (!waterinfo->refraction_scale)
		waterinfo->refraction_scale = m_pCvarWaterRefractScale->value;

	if (!waterinfo->reflection_scale)
		waterinfo->reflection_scale = m_pCvarWaterReflectScale->value;

	if (!waterinfo->normal_scale)
		waterinfo->normal_scale = m_pCvarWaterNormalScale->value;

	if (!waterinfo->fresnel)
		waterinfo->fresnel = m_pCvarWaterFresnel->value;

	return 1;
}