/*
Trinity Rendering Engine - Copyright Andrew Lucas 2009-2012

The Trinity Engine is free software, distributed in the hope th-
at it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU Lesser General Public License for more det-
ails.

Mirror Manager code
Code by Andrew Lucas
Additional code taken from Id Software
*/

#include "PlatformHeaders.h"
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

#include "propmanager.h"
#include "particle_engine.h"
#include "bsprenderer.h"
#include "mirrormanager.h"
#include "watershader.h"

#include "goldsrc_beamrenderer.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "r_efx.h"
#include "r_studioint.h"
#include "studio_util.h"
#include "event_api.h"
#include "event_args.h"

#include "opengl_utils/GL_FBO.h"
#include "opengl_utils/GL_ShaderProgram.h"
#include "opengl_utils/GL_TextureHandler.h"
#include "opengl_utils/GL_Buffers.h"
#include "opengl_utils/GL_VertexArrayObject.h"

#include "goldsrc_spriterenderer.h"
#include "StudioModelRenderer.h"

CMirrorManager gMirrorManager;



//===========================================
// GLSL SHADER START
//
//===========================================

#include "glshaders/mirror_glsl.h"

//===========================================
// GLSL SHADER END
//
//===========================================



extern glm::mat4 oldviewmatrix;
extern glm::mat4 oldprojectionmatrix;
Vector m_vRestoreRenderOrigin;
Vector m_vRestoreViewAngles;

/*
====================
Init

====================
*/
void CMirrorManager::Init(void)
{
	m_pCvarDrawMirrors = gEngfuncs.pfnRegisterVariable("r_mirrors", "1", 0);

	m_MirrorShader = new GL_ShaderProgram(mirror_vertex, mirror_fragment);

	m_MirrorShader->Bind();
	m_MirrorShader->Uniform1i(m_MirrorShader->GetUniformLoc("texture0"), 0);

	GL_ShaderProgram::ResetShaderBind();
}

/*
====================
VidInit

====================
*/
void CMirrorManager::VidInit(void)
{
	for (int i = 0; i < m_iNumMirrors; i++)
		delete m_pMirrors[i].texture;

	memset(m_pMirrors, 0, sizeof(m_pMirrors));
	m_iNumMirrors = 0;
	m_iNumPasses = 0;

	if (!mirrorFBO)
		mirrorFBO = new GL_FBOHandler();
	if (!mirrorDepthBuffer)
		mirrorDepthBuffer = new GL_RBOHandler();

	mirrorFBO->Bind(GL_FBOHandler::Framebuffer);

	mirrorDepthBuffer->Bind();
	mirrorDepthBuffer->RenderBufferStorage(GL_DEPTH_COMPONENT24, MIRROR_RESOLUTION, MIRROR_RESOLUTION);
	mirrorFBO->FramebufferRenderbuffer(GL_FBOHandler::Framebuffer, GL_FBOHandler::DepthAttachment, mirrorDepthBuffer);

	GL_FBOHandler::ResetToMainFBO();

}

GL_TextureHandler::gl_texturecreationinfo_t mirror_textureinfo =
{
	std::string(), GL_TextureHandler::_2DTexture, GL_RGBA8, MIRROR_RESOLUTION, MIRROR_RESOLUTION, 0, GL_RGBA, GL_UNSIGNED_BYTE
};

/*
====================
AllocNewMirror

====================
*/
void CMirrorManager::AllocNewMirror(cl_entity_t* entity)
{
	if (m_iNumMirrors == MAX_MIRRORS)
		return;

	if (entity->model->nummodelsurfaces > 1)
	{
		gEngfuncs.Con_Printf("ERROR: Mirror bmodel has more than 1 polygon!\n");
		return;
	}

	cl_mirror_t* pMirror = &m_pMirrors[m_iNumMirrors];
	m_iNumMirrors++;

	msurface_t* psurf = &engine_cl->worldmodel->surfaces[entity->model->firstmodelsurface];

	pMirror->mins = Vector(9999, 9999, 9999);
	pMirror->maxs = Vector(-9999, -9999, -9999);

	for (int i = 0; i < psurf->polys->numverts; i++)
	{
		// mins
		if (pMirror->mins.x > psurf->polys->verts[i][0])
			pMirror->mins.x = psurf->polys->verts[i][0];

		if (pMirror->mins.y > psurf->polys->verts[i][1])
			pMirror->mins.y = psurf->polys->verts[i][1];

		if (pMirror->mins.z > psurf->polys->verts[i][2])
			pMirror->mins.z = psurf->polys->verts[i][2];

		// maxs
		if (pMirror->maxs.x < psurf->polys->verts[i][0])
			pMirror->maxs.x = psurf->polys->verts[i][0];

		if (pMirror->maxs.y < psurf->polys->verts[i][1])
			pMirror->maxs.y = psurf->polys->verts[i][1];

		if (pMirror->maxs.z < psurf->polys->verts[i][2])
			pMirror->maxs.z = psurf->polys->verts[i][2];
	}

	pMirror->entity = entity;
	pMirror->entity->efrag = (efrag_s*)pMirror;

	pMirror->texture = new GL_TextureHandler(&mirror_textureinfo);
	pMirror->origin[0] = (pMirror->mins[0] + pMirror->maxs[0]) * 0.5f;
	pMirror->origin[1] = (pMirror->mins[1] + pMirror->maxs[1]) * 0.5f;
	pMirror->origin[2] = (pMirror->mins[2] + pMirror->maxs[2]) * 0.5f;
	pMirror->surface = psurf;
}

/*
====================
DrawMirrorPasses

====================
*/
void CMirrorManager::DrawMirrorPasses(ref_params_t* pparams)
{
	if (m_pCvarDrawMirrors->value < 1)
		return;

	if (!m_iNumMirrors)
		return;

	m_iNumPasses = 0;

	// Make sure depthranged stuff is not clipped
	gBSPRenderer.m_bMirroring = true;

	memcpy(&m_pMirrorParams, pparams, sizeof(ref_params_t));
	m_pViewParams = pparams;

	FrustumCheck restorefrustum = gHUD.viewFrustum;

	for (int i = 0; i < m_iNumMirrors; i++)
	{
		m_pCurrentMirror = &m_pMirrors[i];

		if (!m_pCurrentMirror->draw)
			continue;

		gHUD.viewFrustum.SetFrustum(pparams->viewangles, pparams->vieworg, gHUD.m_iFOV, gHUD.m_pFogSettings.end, true);

		if (gHUD.viewFrustum.CullBox(m_pCurrentMirror->mins, m_pCurrentMirror->maxs))
		{
			// YOU MUST DIE
			m_pCurrentMirror->draw = false;
			continue;
		}

		SetupMirrorPass();
		DrawMirrorPass();
		FinishMirrorPass();
	}

	gBSPRenderer.m_bMirroring = false;

	gHUD.viewFrustum = restorefrustum;

	glViewport(GL_ZERO, GL_ZERO, ScreenWidth, ScreenHeight);
}

/*
====================
SetupClipping

====================
*/
void CMirrorManager::SetupClipping(void)
{

	Vector vDist_;
	Vector vForward_;
	Vector vRight_;
	Vector vUp_;

	AngleVectors(m_pMirrorParams.viewangles, &vForward_, &vRight_, &vUp_);
	VectorSubtract(m_pCurrentMirror->origin, m_pMirrorParams.vieworg, vDist_);

	VectorInverse(vRight_);
	VectorInverse(vUp_);

	glm::vec3 vForward(vForward_.x, vForward_.y, vForward_.z);
	glm::vec3 vRight(vRight_.x, vRight_.y, vRight_.z);
	glm::vec3 vUp(vUp_.x, vUp_.y, vUp_.z);
	glm::vec3 vDist(vDist_.x, vDist_.y, vDist_.z);

	glm::vec4 plane;
	auto mirrorplane = glm::vec3(m_pCurrentMirror->surface->plane->normal.x, m_pCurrentMirror->surface->plane->normal.y, m_pCurrentMirror->surface->plane->normal.z);

	if (m_pCurrentMirror->surface->flags & SURF_PLANEBACK)
	{
		plane.x = glm::dot(vRight, mirrorplane);
		plane.y = glm::dot(vUp, mirrorplane);
		plane.z = glm::dot(vForward, mirrorplane);
		plane.w = glm::dot(vDist, mirrorplane);
	}
	else
	{
		plane.x = glm::dot(vRight, -mirrorplane);
		plane.y = glm::dot(vUp, -mirrorplane);
		plane.z = glm::dot(vForward, -mirrorplane);
		plane.w = glm::dot(vDist, -mirrorplane);
	}

	oldprojectionmatrix = gBSPRenderer.m_ProjectionMatrix;
	auto& projection = gBSPRenderer.m_ProjectionMatrix;

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
DrawMirrorPass

====================
*/
void CMirrorManager::DrawMirrorPass(void)
{
	// Set world renderer
	R_SetupView(&m_pMirrorParams);

	// Draw world
	gBSPRenderer.DrawNormalTriangles(true);

	g_StudioRenderer.StudioDrawModels();

	g_LegacySpriteRenderer.DrawSpriteEntities();

	g_BeamRenderer.R_DrawBeams(engine_cl->time - engine_cl->oldtime);

	//// Draw props
	//gPropManager.RenderProps();
	//
	//// Draw Transparent world polys
	//gBSPRenderer.DrawTransparentTriangles();
	//
	//// Draw particles
	//gParticleEngine.DrawParticles();

	m_iNumPasses++;
}

/*
====================
SetupMirrorPass

====================
*/
void CMirrorManager::SetupMirrorPass(void)
{
	Vector forward_;
	Vector up_;

	Vector original_viewangles = m_pViewParams->viewangles;
	original_viewangles = original_viewangles + gBSPRenderer.m_RefParams.punchangle;

	AngleVectors(original_viewangles, &forward_, NULL, NULL);

	float flDist = DotProduct(m_pViewParams->vieworg, m_pCurrentMirror->surface->plane->normal) - m_pCurrentMirror->surface->plane->dist;
	VectorMA(m_pViewParams->vieworg, -2 * flDist, m_pCurrentMirror->surface->plane->normal, m_pMirrorParams.vieworg);

	if (m_pCurrentMirror->surface->flags & SURF_PLANEBACK)
	{
		flDist = DotProduct(forward_, m_pCurrentMirror->surface->plane->normal);
		VectorMA(forward_, -2 * flDist, m_pCurrentMirror->surface->plane->normal, forward_);
	}
	else
	{
		flDist = DotProduct(forward_, -m_pCurrentMirror->surface->plane->normal);
		VectorMA(forward_, -2 * flDist, -m_pCurrentMirror->surface->plane->normal, forward_);
	}

	m_pMirrorParams.viewangles[0] = -asin(forward_[2]) / M_PI * 180;
	m_pMirrorParams.viewangles[1] = atan2(forward_[1], forward_[0]) / M_PI * 180;
	m_pMirrorParams.viewangles[2] = -original_viewangles[2];

	AngleVectors(m_pMirrorParams.viewangles, &m_pMirrorParams.forward, &m_pMirrorParams.right, &m_pMirrorParams.up);
	VectorCopy(m_pMirrorParams.viewangles, m_pMirrorParams.cl_viewangles);

	oldviewmatrix = gBSPRenderer.m_ViewMatrix;

	auto& m_vViewAngles = m_pMirrorParams.viewangles;
	auto& m_RefParams = gBSPRenderer.m_RefParams;
	auto& m_vRenderOrigin = m_pMirrorParams.vieworg;

	glm::vec3 viewangles = glm::vec3(m_vViewAngles.x + m_RefParams.punchangle.x, m_vViewAngles.y + m_RefParams.punchangle.y, m_vViewAngles.z + m_RefParams.punchangle.z);

	AngleVectors(Vector(viewangles.x, viewangles.y, viewangles.z), nullptr, nullptr, &up_);

	glm::vec3 forward = glm::vec3(forward_.x, forward_.y, forward_.z);

	glm::vec3 cameraPos = glm::vec3(m_vRenderOrigin.x, m_vRenderOrigin.y, m_vRenderOrigin.z);
	glm::vec3 cameraTarget = cameraPos + forward;
	glm::vec3 cameraUp = glm::vec3(up_.x, up_.y, up_.z);

	m_vRestoreRenderOrigin = gBSPRenderer.m_vRenderOrigin;
	m_vRestoreViewAngles = gBSPRenderer.m_vViewAngles;
	gBSPRenderer.m_vRenderOrigin = m_vRenderOrigin;
	gBSPRenderer.m_vViewAngles = m_vViewAngles;

	gBSPRenderer.m_ViewMatrix = glm::lookAt(cameraPos, cameraTarget, cameraUp);

	mirrorFBO->Bind(GL_FBOHandler::Framebuffer);
	mirrorFBO->FramebufferTexture2D(GL_FBOHandler::Framebuffer, GL_FBOHandler::ColorAttachment, GL_TEXTURE_2D, m_pCurrentMirror->texture->GetTextureID(), 0);

	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(glm::value_ptr(gBSPRenderer.m_ViewMatrix * gBSPRenderer.m_ModelMatrix));

	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(glm::value_ptr(gBSPRenderer.m_ProjectionMatrix));

	// Completely clear everything
	glClearColor(GL_ZERO, GL_ZERO, GL_ZERO, GL_ONE);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_ACCUM_BUFFER_BIT);

	glViewport(GL_ZERO, GL_ZERO, MIRROR_RESOLUTION, MIRROR_RESOLUTION);

	// Set up clipping
	SetupClipping();
}

/*
====================
FinishMirrorPass

====================
*/
void CMirrorManager::FinishMirrorPass(void)
{
	GL_FBOHandler::ResetToMainFBO();

		// Completely clear everything
	glClearColor(GL_ZERO, GL_ZERO, GL_ZERO, GL_ONE);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_ACCUM_BUFFER_BIT);

	// Turn culling off
	gHUD.viewFrustum.DisableExtraCullBox();

	gBSPRenderer.m_ViewMatrix = oldviewmatrix;
	gBSPRenderer.m_ProjectionMatrix = oldprojectionmatrix;
	R_SetupView(m_pViewParams);

	gBSPRenderer.m_vRenderOrigin = m_vRestoreRenderOrigin;
	gBSPRenderer.m_vViewAngles = m_vRestoreViewAngles;
	m_vRestoreRenderOrigin = Vector(0, 0, 0);
	m_vRestoreViewAngles = Vector(0, 0, 0);

	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(glm::value_ptr(gBSPRenderer.m_ViewMatrix * gBSPRenderer.m_ModelMatrix));

	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(glm::value_ptr(gBSPRenderer.m_ProjectionMatrix));
}

/*
====================
DrawMirrors

====================
*/
void CMirrorManager::DrawMirrors(void)
{
	if (m_pCvarDrawMirrors->value < 1)
		return;

	if (!m_iNumMirrors)
		return;


	m_MirrorShader->Bind();

	m_MirrorShader->UniformMatrix4fv(m_MirrorShader->GetUniformLoc("projectionMatrix"), 1, GL_FALSE, glm::value_ptr(gBSPRenderer.m_ProjectionMatrix));
	m_MirrorShader->UniformMatrix4fv(m_MirrorShader->GetUniformLoc("modelMatrix"), 1, GL_FALSE, glm::value_ptr(gBSPRenderer.m_ModelMatrix));


	gBSPRenderer.m_pBSP_VAO->BindVAO();

	for (int i = 0; i < m_iNumMirrors; i++)
	{
		m_pCurrentMirror = &m_pMirrors[i];

		if (!m_pCurrentMirror->draw)
			continue;

		if (gHUD.viewFrustum.CullBox(m_pCurrentMirror->mins, m_pCurrentMirror->maxs))
			continue;

		GLuint location = m_MirrorShader->GetUniformLoc("viewMatrix");

		m_MirrorShader->UniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(gBSPRenderer.m_ViewMatrix));

		model_t* model = m_pMirrors[i].entity->model;
		clientmsurface_t* psurf = &BSPWorld_Model::m_pWorldSurfaces[model->firstmodelsurface];

		gBSPRenderer.BindGLTexture(GL_TEXTURE0, m_pCurrentMirror->texture->GetTextureID());


		gBSPRenderer.DrawPolyFromArray(BSPWorld_Model::m_pWorldSurfaces, psurf);
		psurf->visframe = gBSPRenderer.m_iFrameCount; // For decals
	}

	GL_ShaderProgram::ResetShaderBind();
}