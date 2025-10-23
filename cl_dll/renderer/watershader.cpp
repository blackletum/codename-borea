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

#include "r_efx.h"
#include "r_studioint.h"
#include "studio_util.h"
#include "event_api.h"
#include "event_args.h"

#include "StudioModelRenderer.h"

CWaterShader gWaterShader;

//===========================================
// GLSL SHADER START
//
//===========================================

#include "glshaders/water_glsl.h"

//===========================================
// GLSL SHADER END
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

	m_WaterFragmentShader = new GL_ShaderProgram(water_depth_vertex, water_fragment_water_regular);

	m_WaterShader_locs[watershader_renderorigin] = m_WaterFragmentShader->GetUniformLoc("renderorigin");

	m_WaterShader_locs[watershader_projviewmodelmatrix] = m_WaterFragmentShader->GetUniformLoc("projviewmodelmatrix");

	m_WaterShader_locs[watershader_underwater] = m_WaterFragmentShader->GetUniformLoc("underwater");

	m_WaterShader_locs[watershader_waterfog] = m_WaterFragmentShader->GetUniformLoc("waterfog");
	m_WaterShader_locs[watershader_fogstart] = m_WaterFragmentShader->GetUniformLoc("fogstart");
	m_WaterShader_locs[watershader_fogend] = m_WaterFragmentShader->GetUniformLoc("fogend");
	m_WaterShader_locs[watershader_m_flFresnelTerm] = m_WaterFragmentShader->GetUniformLoc("m_flFresnelTerm");
	m_WaterShader_locs[watershader_flTime] = m_WaterFragmentShader->GetUniformLoc("flTime");

	m_WaterShader_locs[watershader_normalscale] = m_WaterFragmentShader->GetUniformLoc("normalscale");
	m_WaterShader_locs[watershader_watertex_scale] = m_WaterFragmentShader->GetUniformLoc("watertex_scale");
	//m_WaterShader_locs[watershader_refraction_scale] = m_WaterFragmentShader->GetUniformLoc("refraction_scale");
	//m_WaterShader_locs[watershader_reflection_scale] = m_WaterFragmentShader->GetUniformLoc("reflection_scale");


	m_WaterFragmentShader->Bind();
	m_WaterFragmentShader->Uniform1i(m_WaterFragmentShader->GetUniformLoc("texture0"), 0);
	m_WaterFragmentShader->Uniform1i(m_WaterFragmentShader->GetUniformLoc("texture1"), 1);
	m_WaterFragmentShader->Uniform1i(m_WaterFragmentShader->GetUniformLoc("texture2"), 2);
	m_WaterFragmentShader->Uniform1i(m_WaterFragmentShader->GetUniformLoc("texture3"), 3);

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
		delete m_waterFBO;
		delete m_waterDepthBuffer;

		m_waterFBO = nullptr;
		m_waterDepthBuffer = nullptr;
	}

	if (!m_waterFBO && !m_waterDepthBuffer)
	{
		m_waterFBO = new GL_FBOHandler();
		m_waterDepthBuffer = new GL_RBOHandler();

		m_waterFBO->Bind(GL_FBOHandler::Framebuffer);
		m_waterDepthBuffer->Bind();
		m_waterDepthBuffer->RenderBufferStorage(GL_DEPTH_COMPONENT16, m_pCvarWaterResolution->value, m_pCvarWaterResolution->value);
		m_waterFBO->FramebufferRenderbuffer(GL_FBOHandler::Framebuffer, GL_FBOHandler::DepthAttachment, m_waterDepthBuffer);

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
	msurface_t* psurfaces = engine_cl->worldmodel->surfaces + entity->model->firstmodelsurface;
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
	pWater->surfaces = (msurface_t**)malloc(sizeof(msurface_t*) * isurfacecount);

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

	pWater->origin[0] = (pWater->mins[0] + pWater->maxs[0]) * 0.5f;
	pWater->origin[1] = (pWater->mins[1] + pWater->maxs[1]) * 0.5f;
	pWater->origin[2] = (pWater->mins[2] + pWater->maxs[2]) * 0.5f;
}

glm::mat4 oldviewmatrix;
glm::mat4 oldprojectionmatrix;

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

	Vector vDist_;
	Vector vNorm_;

	Vector vForward_;
	Vector vRight_;
	Vector vUp_;

	AngleVectors(pparams->viewangles, &vForward_, &vRight_, &vUp_);
	VectorSubtract(GetWaterOrigin() - Vector(0, 0, 10), pparams->vieworg, vDist_);

	VectorInverse(vRight_);
	VectorInverse(vUp_);

	glm::vec3 vForward(vForward_.x, vForward_.y, vForward_.z);
	glm::vec3 vRight(vRight_.x, vRight_.y, vRight_.z);
	glm::vec3 vUp(vUp_.x, vUp_.y, vUp_.z);
	glm::vec3 vDist(vDist_.x, vDist_.y, vDist_.z);

	glm::vec4 plane;
	auto waterPlane = glm::vec3(m_pCurWater->wplane.normal.x, m_pCurWater->wplane.normal.y, m_pCurWater->wplane.normal.z);
	if (negative)
	{
		plane.x = glm::dot(vRight, -waterPlane);
		plane.y = glm::dot(vUp, -waterPlane);
		plane.z = glm::dot(vForward, -waterPlane);
		plane.w = glm::dot(vDist, -waterPlane);
	}
	else
	{
		plane.x = glm::dot(vRight, waterPlane);
		plane.y = glm::dot(vUp, waterPlane);
		plane.z = glm::dot(vForward, waterPlane);
		plane.w = glm::dot(vDist, waterPlane);
	}

	oldprojectionmatrix = gBSPRenderer.m_ProjectionMatrix;
	auto &projection = gBSPRenderer.m_ProjectionMatrix;

	// Calculate clip-space corner point
	glm::vec4 q;
	q.x = (glm::sign(plane.x) + projection[0][2]) / projection[0][0];
	q.y = (glm::sign(plane.y) + projection[1][2]) / projection[1][1];
	q.z = -1.0f;
	q.w = (1.0f + projection[2][2]) / projection[3][2];

	// Scale plane so it fits
	float scale = 2.0f / glm::dot(plane, q);

	// Modify projection matrix
	projection[0][2] = plane.x * scale;
	projection[1][2] = plane.y * scale;
	projection[2][2] = plane.z * scale + 1.0f;
	projection[3][2] = plane.w * scale;
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

	m_waterFBO->Bind(GL_FBOHandler::Framebuffer);

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
		gBSPRenderer.DrawNormalTriangles_Cheap(true);
	else
		gBSPRenderer.DrawNormalTriangles_Cheap(false);

	if(m_pCvarWaterForceReflectEntities->value)
	{
		g_StudioRenderer.StudioDrawModels(false);

		g_BeamRenderer.R_DrawBeams(engine_cl->time - engine_cl->oldtime);
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
	m_waterFBO->FramebufferTexture2D(GL_FBOHandler::Framebuffer, GL_FBOHandler::ColorAttachment, GL_TEXTURE_2D, m_pCurWater->refract->GetTextureID(), 0);

	// Completely clear everything
	glClearColor(GL_ZERO, GL_ZERO, GL_ZERO, GL_ONE);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_ACCUM_BUFFER_BIT);

	if (!ViewInWater())
	{
		gHUD.viewFrustum.SetExtraCullBox(m_pCurWater->entity->curstate.mins, m_pCurWater->entity->curstate.maxs);
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

	m_pViewParams->viewangles = m_pViewParams->viewangles + m_pViewParams->punchangle;

	m_pWaterParams = *m_pViewParams;

	AngleVectors(m_pViewParams->viewangles, &vForward, NULL, NULL);

	float flDist = abs(GetWaterOrigin().z - m_vViewOrigin[2]);
	VectorMA(m_vViewOrigin, -2 * flDist, m_pCurWater->wplane.normal, m_pWaterParams.vieworg);

	flDist = DotProduct(vForward, -m_pCurWater->wplane.normal);
	VectorMA(vForward, -2 * flDist, -m_pCurWater->wplane.normal, vForward);

	m_pWaterParams.viewangles[0] = -asin(vForward[2]) / M_PI * 180;
	m_pWaterParams.viewangles[1] = atan2(vForward[1], vForward[0]) / M_PI * 180;
	m_pWaterParams.viewangles[2] = -m_pViewParams->viewangles[2];

	AngleVectors(m_pWaterParams.viewangles, &m_pWaterParams.forward, &m_pWaterParams.right, &m_pWaterParams.up);
	VectorCopy(m_pWaterParams.viewangles, m_pWaterParams.cl_viewangles);

	m_waterFBO->FramebufferTexture2D(GL_FBOHandler::Framebuffer, GL_FBOHandler::ColorAttachment, GL_TEXTURE_2D, m_pCurWater->reflect->GetTextureID(), 0);

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

	glm::vec3 viewangles = glm::vec3(m_vViewAngles.x + m_RefParams.punchangle.x, m_vViewAngles.y + m_RefParams.punchangle.y, m_vViewAngles.z + m_RefParams.punchangle.z);
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
	gBSPRenderer.m_ProjectionMatrix = oldprojectionmatrix;
	R_SetupView(m_pViewParams);

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

	m_WaterFragmentShader->Bind();

	m_WaterFragmentShader->UniformMatrix4fv(m_WaterShader_locs[watershader_projviewmodelmatrix], 1, GL_FALSE, glm::value_ptr(gBSPRenderer.m_ProjectionMatrix * gBSPRenderer.m_ViewMatrix * gBSPRenderer.m_ModelMatrix));
	m_WaterFragmentShader->Uniform3fv(m_WaterShader_locs[watershader_renderorigin], 1, gBSPRenderer.m_vRenderOrigin);
	m_WaterFragmentShader->Uniform1f(m_WaterShader_locs[watershader_flTime], flTime);
	m_WaterFragmentShader->Uniform1i(m_WaterShader_locs[watershader_underwater], m_bViewInWater ? 1 : 0);

	gBSPRenderer.m_pBSP_VAO->BindVAO();

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

				m_WaterFragmentShader->Uniform1f(m_WaterShader_locs[watershader_normalscale], m_pWaterEntInfo[j].normal_scale);
				m_WaterFragmentShader->Uniform1f(m_WaterShader_locs[watershader_watertex_scale], m_pWaterEntInfo[j].watertex_scale);
				//m_WaterFragmentShader->Uniform1f(m_WaterShader_locs[watershader_refraction_scale], m_pWaterEntInfo[j].refraction_scale);
				//m_WaterFragmentShader->Uniform1f(m_WaterShader_locs[watershader_reflection_scale], m_pWaterEntInfo[j].reflection_scale);
				m_WaterFragmentShader->Uniform1f(m_WaterShader_locs[watershader_m_flFresnelTerm], m_pWaterEntInfo[j].fresnel);
				break;
			}
		}

		if (ViewInWater())
			glCullFace(GL_BACK);

		m_WaterFragmentShader->Uniform3fv(m_WaterShader_locs[watershader_waterfog], 1, m_pWaterFogSettings.color);
		m_WaterFragmentShader->Uniform1f(m_WaterShader_locs[watershader_fogstart], m_pWaterFogSettings.start);
		m_WaterFragmentShader->Uniform1f(m_WaterShader_locs[watershader_fogend], m_pWaterFogSettings.end);

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
			gBSPRenderer.DrawPolyFromArray(engine_cl->worldmodel->surfaces, m_pCurWater->surfaces[j]);
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
		return pwater->origin + pwater->entity->curstate.origin;
	else
		return m_pCurWater->origin + m_pCurWater->entity->curstate.origin;
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