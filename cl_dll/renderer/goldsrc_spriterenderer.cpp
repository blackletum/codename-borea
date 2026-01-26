
#include "PlatformHeaders.h"
#include "hud.h"
#include "cl_util.h"

// Triangle rendering apis are in gEngfuncs.pTriAPI

#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "Exports.h"
#include "tri.h"
#include "event_api.h"

// RENDERERS START
#include "renderer/bsprenderer.h"
#include "renderer/propmanager.h"
#include "renderer/particle_engine.h"
#include "renderer/watershader.h"
#include "renderer/mirrormanager.h"
#include "renderer/goldsrc_beamrenderer.h"
#include "opengl_utils/GL_StateHandler.h"
#include "opengl_utils/GL_ShaderProgram.h"
#include "opengl_utils/GL_VertexArrayObject.h"

#include <algorithm> //std::clamp
#include "client_state.h"


#include "studio.h"
#include "StudioModelRenderer.h"
#include "opengl_utils/GL_Buffers.h"

#include "goldsrc_spriterenderer.h"

#define GLARE_FALLOFF 19000.0f

CSpriteRenderer g_LegacySpriteRenderer;

extern void SinCos_(float radians, float* sine, float* cosine);
extern int CL_FxBlend(cl_entity_t* ent);


#include "glshaders/sprite_glsl.h"

struct sprite_vertex_t
{
	Vector point;  // 12 bytes
	color32 color; // x, y, z, a //4 bytes
	int padding[4];
	GL_DECLARE_ATTRIBLIST();
};

struct sprite_quad_t
{
	sprite_vertex_t vert[4];
	byte rendermode;
};

static std::vector<cl_entity_s*> m_vSpriteDrawList;
static std::unordered_map<int, std::vector<sprite_quad_t>> m_vSpriteQuadList;

static GL_BufferHandler* m_pSpriteQuadBuffer;
static GL_ShaderProgram* m_pSpriteShader;
static GL_VertexArrayObject* m_pSpriteVAO;

static cl_entity_s* m_pCurrentEntity;

GL_BEGIN_ATTRIBLIST(sprite_vertex_t)
	GL_DEFINE_ATTRIB(GL_ShaderProgram::ShaderAttribs::VertexPos, 3, GL_FLOAT, sprite_vertex_t, point)
	GL_DEFINE_NORMALIZEDATTRIB(GL_ShaderProgram::ShaderAttribs::Color, 4, GL_UNSIGNED_BYTE, sprite_vertex_t, color)
GL_END_ATTRIBLIST(sprite_vertex_t)



void CSpriteRenderer::Init()
{
	m_pSpriteVAO = new GL_VertexArrayObject();
	m_pSpriteVAO->BindVAO();

	m_pSpriteQuadBuffer = new GL_BufferHandler();
	m_pSpriteQuadBuffer->Bind(GL_BufferHandler::ArrayBuffer);
	//enough space for 8196 sprites. occupies 1 mb in vram
	m_pSpriteQuadBuffer->BufferData(GL_BufferHandler::ArrayBuffer, (sizeof(sprite_vertex_t) * 4) * 8196, nullptr, GL_BufferHandler::StaticDraw);

	m_pSpriteShader = new GL_ShaderProgram(glsl_sprite_vp, glsl_sprite_fp);
	m_pSpriteShader->Bind();
	m_pSpriteShader->Uniform1i(m_pSpriteShader->GetUniformLoc("texture0"), 0);

	m_pSpriteVAO->SetVertexAttributes(sprite_vertex_t::GetAttribLayout());

	GL_BufferHandler::ResetBufferBinding(GL_BufferHandler::ArrayBuffer);
	GL_ShaderProgram::ResetShaderBind();
	GL_VertexArrayObject::ResetVAOBinding();
}

void CSpriteRenderer::VidInit()
{

}

extern int cl_numvisedicts;
extern cl_entity_s* cl_visedicts[512];

void CSpriteRenderer::PushEntityToDraw(cl_entity_s* pEnt)
{
	m_pCurrentEntity = pEnt;
	
	if (!m_pCurrentEntity->model || m_pCurrentEntity->model->type != mod_sprite || m_pCurrentEntity->curstate.effects & FL_NOMODEL)
		return;

	model_t* beamsprite = CL_GetModelByIndex(m_pCurrentEntity->curstate.movetype);
	if (beamsprite)
		return; // hacky way to not draw beams

	m_vSpriteDrawList.push_back(m_pCurrentEntity);
}

void CSpriteRenderer::ClearDrawList()
{ 
	m_vSpriteDrawList.clear(); 
}

void CSpriteRenderer::DrawSpriteEntities()
{
	if (m_vSpriteDrawList.empty())
		return;

	//first make quad list
	for (auto ent : m_vSpriteDrawList)
	{
		QuadifySpriteEnt(ent);
	}

	//now actually paint the screen with the quads
	DrawSpriteQuads();

	m_vSpriteQuadList.clear();
}

void CSpriteRenderer::DrawSpriteQuads()
{
	static std::vector<sprite_vertex_t> verts;
	static int projviewmatrix_loc = m_pSpriteShader->GetUniformLoc("projviewmatrix");

	for (auto& entry : m_vSpriteQuadList)
	{
		for (auto& quads : entry.second)
		{
			verts.push_back(quads.vert[0]);
			verts.push_back(quads.vert[1]);
			verts.push_back(quads.vert[2]);
			verts.push_back(quads.vert[3]);
		}
	}
	m_pSpriteShader->Bind();
	m_pSpriteShader->UniformMatrix4fv(projviewmatrix_loc, 1, false, glm::value_ptr(gBSPRenderer.m_ProjectionMatrix * gBSPRenderer.m_ViewMatrix));


	m_pSpriteVAO->BindVAO();
	m_pSpriteQuadBuffer->Bind(GL_BufferHandler::ArrayBuffer);
	m_pSpriteQuadBuffer->BufferSubData(GL_BufferHandler::ArrayBuffer, 0, verts.size() * sizeof(sprite_vertex_t), verts.data());


	int offset = 0;
	int currendermode = -999;
	for (auto& entry : m_vSpriteQuadList)
	{
		gBSPRenderer.BindGLTexture(GL_TEXTURE0, entry.first);
		for (auto& quad : entry.second)
		{
			if (currendermode != quad.rendermode)
			{
				currendermode = quad.rendermode;

				// select properly rendermode
				switch (currendermode)
				{
				case kRenderTransAlpha:
					g_GlobalGLState.SetDepthWrite(false);
					g_GlobalGLState.SetDepthTest(true);
					g_GlobalGLState.SetBlend(true);
					g_GlobalGLState.SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					break;
				case kRenderTransColor:
				case kRenderTransTexture:
					g_GlobalGLState.SetBlend(true);
					g_GlobalGLState.SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					g_GlobalGLState.SetDepthTest(true);
					break;
				case kRenderGlow:
					g_GlobalGLState.SetDepthTest(false);
					g_GlobalGLState.SetBlend(true);
					g_GlobalGLState.SetBlendFunc(GL_SRC_ALPHA, GL_ONE);
					g_GlobalGLState.SetDepthWrite(false);
					break;
				case kRenderTransAdd:
					g_GlobalGLState.SetBlend(true);
					g_GlobalGLState.SetBlendFunc(GL_SRC_ALPHA, GL_ONE);
					g_GlobalGLState.SetDepthWrite(false);
					g_GlobalGLState.SetDepthTest(true);
					break;
				case kRenderNormal:
				default:
					g_GlobalGLState.SetBlend(false);
					g_GlobalGLState.SetDepthWrite(true);
					g_GlobalGLState.SetDepthTest(true);
					break;
				}
			}

			int drawcount = entry.second.size() * 4;
			glDrawArrays(GL_QUADS, offset, 4);
			offset += 4;
		}
	}


	verts.clear();

	g_GlobalGLState.SetDepthWrite(true);
	g_GlobalGLState.SetDepthTest(true);
	g_GlobalGLState.SetBlend(false);

	GL_ShaderProgram::ResetShaderBind();
	GL_VertexArrayObject::ResetVAOBinding();
}

void CSpriteRenderer::QuadifySpriteEnt(cl_entity_t* e)
{
	mspriteframe_t *frame = NULL, *oldframe = NULL;
	msprite_t* psprite;
	model_t* model;
	int i, type;
	float angle, dot, sr, cr;
	float lerp = 1.0f, ilerp, scale;
	Vector v_forward, v_right, v_up;
	Vector origin;

	model = e->model;
	psprite = (msprite_t*)model->cache.data;
	origin = e->origin; // set render origin

	r_blend = CL_FxBlend(e);

	// do movewith
	if (e->curstate.aiment > 0 && e->curstate.movetype == MOVETYPE_FOLLOW)
	{
		cl_entity_t* parent;

		parent = gEngfuncs.GetEntityByIndex(e->curstate.aiment);

		if (parent && parent->model)
		{
			if (parent->model->type == mod_studio && e->curstate.body > 0)
			{
				int num = bound(1, e->curstate.body, MAXSTUDIOATTACHMENTS);
				VectorCopy(parent->attachment[num - 1], origin);
			}
			else
				VectorCopy(parent->origin, origin);
		}
	}

	scale = e->curstate.scale;
	if (!scale)
		scale = 1.0f;

	if (SpriteOccluded(e, origin, &scale))
		return; // sprite culled

	sprite_quad_t spr_quad;
	spr_quad.rendermode = e->curstate.rendermode;

	// NOTE: never pass sprites with rendercolor '0 0 0' it's a stupid Valve Hammer Editor bug
	if (e->curstate.rendercolor.r || e->curstate.rendercolor.g || e->curstate.rendercolor.b)
	{
		spr_quad.vert[0].color.r = e->curstate.rendercolor.r;
		spr_quad.vert[0].color.g = e->curstate.rendercolor.g;
		spr_quad.vert[0].color.b = e->curstate.rendercolor.b;
	}
	else
	{
		spr_quad.vert[0].color.r = 255;
		spr_quad.vert[0].color.g = 255;
		spr_quad.vert[0].color.b = 255;
	}

	frame = oldframe = GetSpriteFrame(model, e->curstate.frame, e->angles[YAW]);

	type = psprite->type;

	// automatically roll parallel sprites if requested
	if (e->angles[ROLL] != 0.0f && type == SPR_VP_PARALLEL)
		type = SPR_VP_PARALLEL_ORIENTED;

	Vector viewforward;
	Vector viewup;
	Vector viewright;

	AngleVectors(gBSPRenderer.m_vViewAngles, &viewforward, &viewright, &viewup);

	switch (type)
	{
	case SPR_ORIENTED:
		AngleVectors(e->angles, &v_forward, &v_right, &v_up);
		VectorScale(v_forward, 0.01f, v_forward); // to avoid z-fighting
		VectorSubtract(origin, v_forward, origin);
		break;
	case SPR_FACING_UPRIGHT:
		v_right = Vector(origin[1] - gBSPRenderer.m_vRenderOrigin[1], -(origin[0] - gBSPRenderer.m_vRenderOrigin[0]), 0.0f);
		v_up = Vector(0.0f, 0.0f, 1.0f);
		VectorNormalize(v_right);
		break;
	case SPR_VP_PARALLEL_UPRIGHT:
		dot = viewforward.z;
		if ((dot > 0.999848f) || (dot < -0.999848f)) // cos(1 degree) = 0.999848
			return;									 // invisible
		v_up = Vector(0.0f, 0.0f, 1.0f);
		v_right = Vector(viewforward.y, -viewforward.x, 0.0f);
		VectorNormalize(v_right);
		break;
	case SPR_VP_PARALLEL_ORIENTED:
		angle = e->angles[ROLL] * ((M_PI * 2) / 360.0f);
		SinCos_(angle, &sr, &cr);
		for (i = 0; i < 3; i++)
		{
			v_right[i] = (viewright[i] * cr + viewup[i] * sr);
			v_up[i] = viewright[i] * -sr + viewup[i] * cr;
		}
		break;
	case SPR_VP_PARALLEL: // normal sprite
	default:
		VectorCopy(viewright, v_right);
		VectorCopy(viewup, v_up);
		break;
	}

	if (oldframe == frame)
	{
		// draw the single non-lerped frame
		spr_quad.vert[0].color.a = r_blend;

		MakeSpriteQuad(frame, origin, v_right, v_up, scale, &spr_quad);
	}
	else
	{
		// draw two combined lerped frames
		lerp = bound(0.0f, lerp, 1.0f);
		ilerp = 1.0f - lerp;

		ilerp *= 255;

		if (ilerp != 0.0f)
		{
			spr_quad.vert[0].color.a = (r_blend * ilerp);

			MakeSpriteQuad(oldframe, origin, v_right, v_up, scale, &spr_quad);
		}

		if (lerp != 0.0f)
		{
			spr_quad.vert[0].color.a = r_blend * lerp;

			MakeSpriteQuad(frame, origin, v_right, v_up, scale, &spr_quad);
		}
	}
}

bool CSpriteRenderer::SpriteOccluded(cl_entity_t* e, Vector origin, float* pscale)
{
	if (e->curstate.rendermode == kRenderGlow)
	{
		float blend;
		Vector v;
		Vector temporigin = origin;
		v = gBSPRenderer.TriWorldToScreen(temporigin);

		if (v[0] < gBSPRenderer.m_RefParams.viewport[0] || v[0] > gBSPRenderer.m_RefParams.viewport[0] + gBSPRenderer.m_RefParams.viewport[2])
			return true; // do scissor
		if (v[1] < gBSPRenderer.m_RefParams.viewport[1] || v[1] > gBSPRenderer.m_RefParams.viewport[1] + gBSPRenderer.m_RefParams.viewport[3])
			return true; // do scissor

		blend = SpriteGlowBlend(origin, e->curstate.rendermode, e->curstate.renderfx, pscale);
		r_blend *= blend;

		if (blend <= 0.01f)
			return true; // faded
	}
	else
	{
		if (CullSpriteModel(e, origin))
			return true;
	}

	return false;
}

float CSpriteRenderer::SpriteGlowBlend(Vector origin, int rendermode, int renderfx, float* pscale)
{
	float dist, brightness;
	Vector glowDist;
	pmtrace_t tr;
	static cvar_t* r_traceglow = gEngfuncs.pfnGetCvarPointer("r_traceglow");

	glowDist = origin - gBSPRenderer.m_RefParams.vieworg;
	dist = glowDist.Length();

	EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(gBSPRenderer.m_RefParams.vieworg, origin, r_traceglow->value ? PM_GLASS_IGNORE : (PM_GLASS_IGNORE | PM_STUDIO_IGNORE), -2, &tr);

	if ((1.0f - tr.fraction) * dist > 8.0f)
		return 0.0f;

	if (renderfx == kRenderFxNoDissipation)
		return 1.0f;

	brightness = GLARE_FALLOFF / (dist * dist);
	brightness = bound(0.05f, brightness, 1.0f);
	*pscale *= dist * (1.0f / 200.0f);

	return brightness;
}
bool CSpriteRenderer::SpriteHasLightmap(cl_entity_t* e, int texFormat)
{
	if (!gEngfuncs.pfnGetCvarFloat("r_sprite_lighting"))
		return false;

	if (texFormat != SPR_ALPHTEST)
		return false;

	// if (e->curstate.effects & EF_FULLBRIGHT))
	//	return false;

	if (e->curstate.renderamt <= 127)
		return false;

	switch (e->curstate.rendermode)
	{
	case kRenderNormal:
	case kRenderTransAlpha:
	case kRenderTransTexture:
		break;
	default:
		return false;
	}

	return true;
}

void CSpriteRenderer::MakeSpriteQuad(mspriteframe_t* frame, Vector org, Vector v_right, Vector v_up, float scale, sprite_quad_t* spr_quad)
{
	Vector point;

	spr_quad->vert[3] = spr_quad->vert[2] = spr_quad->vert[1] = spr_quad->vert[0];

	VectorMA(org, frame->down * scale, v_up, spr_quad->vert[0].point);
	VectorMA(spr_quad->vert[0].point, frame->left * scale, v_right, spr_quad->vert[0].point);


	VectorMA(org, frame->up * scale, v_up, spr_quad->vert[1].point);
	VectorMA(spr_quad->vert[1].point, frame->left * scale, v_right, spr_quad->vert[1].point);


	VectorMA(org, frame->up * scale, v_up, spr_quad->vert[2].point);
	VectorMA(spr_quad->vert[2].point, frame->right * scale, v_right, spr_quad->vert[2].point);


	VectorMA(org, frame->down * scale, v_up, spr_quad->vert[3].point);
	VectorMA(spr_quad->vert[3].point, frame->right * scale, v_right, spr_quad->vert[3].point);

	m_vSpriteQuadList[frame->gl_texturenum].push_back(*spr_quad);

}
void CSpriteRenderer::GetSpriteFrames(const model_t* pModel, int& framecount)
{
	msprite_t* psprite;
	mspritegroup_t* pspritegroup;
	mspriteframe_t* pspriteframe = NULL;
	float *pintervals, fullinterval;
	int i, numframes;
	float targettime;

	assert(pModel != NULL);
	psprite = (msprite_t*)pModel->cache.data;

	if (!psprite)
	{
		framecount = 0;
		return;
	}

	framecount = psprite->numframes;
}
bool CSpriteRenderer::CullSpriteModel(cl_entity_t* e, Vector origin)
{
	Vector sprite_mins, sprite_maxs;
	float scale = 1.0f;

	if (!e->model->cache.data)
		return true;

	if (e->curstate.scale > 0.0f)
		scale = e->curstate.scale;

	// scale original bbox (no rotation for sprites)
	VectorScale(e->model->mins, scale, sprite_mins);
	VectorScale(e->model->maxs, scale, sprite_maxs);

	VectorAdd(sprite_mins, origin, sprite_mins);
	VectorAdd(sprite_maxs, origin, sprite_maxs);

	return gHUD.viewFrustum.CullBox(sprite_mins, sprite_maxs);
}
mspriteframe_t* CSpriteRenderer::GetSpriteFrame(const model_t* pModel, int frame, float yaw)
{
	msprite_t* psprite;
	mspritegroup_t* pspritegroup;
	mspriteframe_t* pspriteframe = NULL;
	float *pintervals, fullinterval;
	int i, numframes;
	float targettime;

	assert(pModel != NULL);
	psprite = (msprite_t*)pModel->cache.data;

	if (frame < 0)
	{
		frame = 0;
	}
	else if (frame >= psprite->numframes)
	{
		if (frame > psprite->numframes)
			gEngfuncs.Con_Printf("%s: no such frame %d (%s)\n", __func__, frame, pModel->name);
		frame = psprite->numframes - 1;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else if (psprite->frames[frame].type == SPR_GROUP)
	{
		pspritegroup = (mspritegroup_t*)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes - 1];

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by zero
		targettime = engine_cl->time - ((int)(engine_cl->time / fullinterval)) * fullinterval;

		for (i = 0; i < (numframes - 1); i++)
		{
			if (pintervals[i] > targettime)
				break;
		}
		pspriteframe = pspritegroup->frames[i];
	}
	// else if (psprite->frames[frame].type == FRAME_ANGLED)
	//{
	//	int angleframe = (int)(rint((gBSPRenderer.m_RefDef.viewangles[1] - yaw + 45.0f) / 360 * 8) - 4) & 7;
	//
	//	// e.g. doom-style sprite monsters
	//	pspritegroup = (mspritegroup_t*)psprite->frames[frame].frameptr;
	//	pspriteframe = pspritegroup->frames[angleframe];
	// }

	return pspriteframe;
}