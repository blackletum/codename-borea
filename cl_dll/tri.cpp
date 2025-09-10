//========= Copyright � 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// Triangle rendering, if any

#include "hud.h"
#include "cl_util.h"

// Triangle rendering apis are in gEngfuncs.pTriAPI

#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
//RENDERERS START
#include "bsprenderer.h"
#include "propmanager.h"
#include "particle_engine.h"
#include "watershader.h"
#include "mirrormanager.h"

#include "StudioModelRenderer.h"
#include "GameStudioModelRenderer.h"

#include "shake.h"

#include "event_api.h"

extern CGameStudioModelRenderer g_StudioRenderer;
//RENDERERS END
#include "rain.h"
#include "com_model.h"
#include "studio_util.h"

#include "Exports.h"
#include "tri.h"

#include "glInclude.h"
#include "blur.h"

#include "svd_render.h"

extern int g_iWaterLevel;
extern Vector v_origin;

#define bound( min, num, max ) ((num) >= (min) ? ((num) < (max) ? (num) : (max)) : (min))


extern int cl_numvisedicts;
extern cl_entity_s* cl_visedicts[512];

extern client_state_s* engine_cl;

float r_blend = 0.0f;

#define GLARE_FALLOFF	19000.0f

/*
=================
R_DrawSpriteQuad
=================
*/
static void R_DrawSpriteQuad(mspriteframe_t* frame, Vector org, Vector v_right, Vector v_up, float scale)
{
	Vector	point;


	glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 1.0f);
		VectorMA(org, frame->down * scale, v_up, point);
		VectorMA(point, frame->left * scale, v_right, point);
		glVertex3fv(point);
		glTexCoord2f(0.0f, 0.0f);
		VectorMA(org, frame->up * scale, v_up, point);
		VectorMA(point, frame->left * scale, v_right, point);
		glVertex3fv(point);
		glTexCoord2f(1.0f, 0.0f);
		VectorMA(org, frame->up * scale, v_up, point);
		VectorMA(point, frame->right * scale, v_right, point);
		glVertex3fv(point);
		glTexCoord2f(1.0f, 1.0f);
		VectorMA(org, frame->down * scale, v_up, point);
		VectorMA(point, frame->right * scale, v_right, point);
		glVertex3fv(point);
	glEnd();
}

void R_SpriteColor(colorVec* pColor, cl_entity_t* pEntity, int alpha)
{
	int a;

	if (pEntity->curstate.rendermode == kRenderGlow || pEntity->curstate.rendermode == kRenderTransAdd)
		a = std::clamp(alpha, 0, 255); // some entities > 255 wtf?
	else
		a = 256;

	if (pEntity->curstate.rendercolor.r != 0 || pEntity->curstate.rendercolor.g != 0 || pEntity->curstate.rendercolor.b != 0)
	{
		pColor->r = (pEntity->curstate.rendercolor.r * a) >> 8;
		pColor->g = (pEntity->curstate.rendercolor.g * a) >> 8;
		pColor->b = (pEntity->curstate.rendercolor.b * a) >> 8;
	}
	else
	{
		pColor->r = (255 * a) >> 8;
		pColor->g = (255 * a) >> 8;
		pColor->b = (255 * a) >> 8;
	}
}

mspriteframe_t* R_GetSpriteFrame(const model_t* pModel, int frame, float yaw)
{
	msprite_t* psprite;
	mspritegroup_t* pspritegroup;
	mspriteframe_t* pspriteframe = NULL;
	float* pintervals, fullinterval;
	int i, numframes;
	float          targettime;

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
	//else if (psprite->frames[frame].type == FRAME_ANGLED)
	//{
	//	int angleframe = (int)(rint((gBSPRenderer.m_RefDef.viewangles[1] - yaw + 45.0f) / 360 * 8) - 4) & 7;
	//
	//	// e.g. doom-style sprite monsters
	//	pspritegroup = (mspritegroup_t*)psprite->frames[frame].frameptr;
	//	pspriteframe = pspritegroup->frames[angleframe];
	//}

	return pspriteframe;
}

void R_GetSpriteFrames(const model_t* pModel, int &framecount)
{
	msprite_t* psprite;
	mspritegroup_t* pspritegroup;
	mspriteframe_t* pspriteframe = NULL;
	float* pintervals, fullinterval;
	int i, numframes;
	float          targettime;

	assert(pModel != NULL);
	psprite = (msprite_t*)pModel->cache.data;

	if (!psprite)
	{
		framecount = 0;
		return;
	}

	framecount = psprite->numframes;
}

/*
================
R_CullSpriteModel

Cull sprite model by bbox
================
*/
static qboolean R_CullSpriteModel(cl_entity_t* e, Vector origin)
{
	Vector	sprite_mins, sprite_maxs;
	float	scale = 1.0f;

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

static qboolean R_SpriteHasLightmap(cl_entity_t* e, int texFormat)
{
	if (!gEngfuncs.pfnGetCvarFloat("r_sprite_lighting"))
		return false;

	if (texFormat != SPR_ALPHTEST)
		return false;

	//if (e->curstate.effects & EF_FULLBRIGHT))
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

/*
================
R_GlowSightDistance

Set sprite brightness factor
================
*/
static float R_SpriteGlowBlend(Vector origin, int rendermode, int renderfx, float* pscale)
{
	float	dist, brightness;
	Vector	glowDist;
	pmtrace_t tr;

	VectorSubtract(origin, gBSPRenderer.m_RefDef.vieworg, glowDist);
	dist = glowDist.Length();

	gEngfuncs.pEventAPI->EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(gBSPRenderer.m_RefDef.vieworg, origin, gEngfuncs.pfnGetCvarFloat("r_traceglow") ? PM_GLASS_IGNORE : (PM_GLASS_IGNORE | PM_STUDIO_IGNORE), -2, &tr);

	if ((1.0f - tr.fraction) * dist > 8.0f)
		return 0.0f;

	if (renderfx == kRenderFxNoDissipation)
		return 1.0f;

	brightness = GLARE_FALLOFF / (dist * dist);
	brightness = bound(0.05f, brightness, 1.0f);
	*pscale *= dist * (1.0f / 200.0f);

	return brightness;
}

/*
================
R_SpriteOccluded

Do occlusion test for glow-sprites
================
*/
static qboolean R_SpriteOccluded(cl_entity_t* e, Vector origin, float* pscale)
{
	if (e->curstate.rendermode == kRenderGlow)
	{
		float	blend;
		Vector	v;
		Vector temporigin = origin;
		gEngfuncs.pTriAPI->WorldToScreen(v, temporigin);

		if (v[0] < gBSPRenderer.m_RefDef.viewport[0] || v[0] > gBSPRenderer.m_RefDef.viewport[0] + gBSPRenderer.m_RefDef.viewport[2])
			return true; // do scissor
		if (v[1] < gBSPRenderer.m_RefDef.viewport[1] || v[1] > gBSPRenderer.m_RefDef.viewport[1] + gBSPRenderer.m_RefDef.viewport[3])
			return true; // do scissor

		blend = R_SpriteGlowBlend(origin, e->curstate.rendermode, e->curstate.renderfx, pscale);
		r_blend *= blend;

		if (blend <= 0.01f)
			return true; // faded
	}
	else
	{
		if (R_CullSpriteModel(e, origin))
			return true;
	}

	return false;
}

extern void SinCos_(float radians, float* sine, float* cosine);

void R_DrawSpriteModel(cl_entity_t* e)
{
	mspriteframe_t* frame = NULL, * oldframe = NULL;
	msprite_t* psprite;
	model_t* model;
	int		i, type;
	float		angle, dot, sr, cr;
	float		lerp = 1.0f, ilerp, scale;
	Vector		v_forward, v_right, v_up;
	Vector		origin, color, color2;

	model = e->model;
	psprite = (msprite_t*)model->cache.data;
	VectorCopy(e->origin, origin);	// set render origin

	r_blend = 1.0f;

	model_t* beamsprite = IEngineStudio.GetModelByIndex(e->curstate.movetype);
	if (beamsprite)
		return; //hacky way to not draw beams

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
			else VectorCopy(parent->origin, origin);
		}
	}

	scale = e->curstate.scale;
	if (!scale) scale = 1.0f;

	if (R_SpriteOccluded(e, origin, &scale))
		return; // sprite culled

	//if (e->curstate.rendermode == kRenderGlow || e->curstate.rendermode == kRenderTransAdd)
	//	R_AllowFog(false);

	// select properly rendermode
	switch (e->curstate.rendermode)
	{
	case kRenderTransAlpha:
		glDepthMask(GL_FALSE);
		// fallthrough
	case kRenderTransColor:
	case kRenderTransTexture:
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		break;
	case kRenderGlow:
		glDisable(GL_DEPTH_TEST);
		// fallthrough
	case kRenderTransAdd:
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glDepthMask(GL_FALSE);
		break;
	case kRenderNormal:
	default:
		glDisable(GL_BLEND);
		break;
	}

	// all sprites can have color
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glEnable(GL_ALPHA_TEST);

	// NOTE: never pass sprites with rendercolor '0 0 0' it's a stupid Valve Hammer Editor bug
	if (e->curstate.rendercolor.r || e->curstate.rendercolor.g || e->curstate.rendercolor.b)
	{
		color[0] = (float)e->curstate.rendercolor.r * (1.0f / 255.0f);
		color[1] = (float)e->curstate.rendercolor.g * (1.0f / 255.0f);
		color[2] = (float)e->curstate.rendercolor.b * (1.0f / 255.0f);
	}
	else
	{
		color[0] = 1.0f;
		color[1] = 1.0f;
		color[2] = 1.0f;
	}

	if (R_SpriteHasLightmap(e, psprite->texFormat))
	{
		Vector lightColor(0, 0, 0);
		g_StudioRenderer.StudioRecursiveLightPoint(nullptr, engine_cl->worldmodel->nodes, origin, origin - 8192, lightColor, false, true);;
		// FIXME: collect light from dlights?
		color2[0] = (float)lightColor.x * (1.0f / 255.0f);
		color2[1] = (float)lightColor.y * (1.0f / 255.0f);
		color2[2] = (float)lightColor.z * (1.0f / 255.0f);
		// NOTE: sprites with 'lightmap' looks ugly when alpha func is GL_GREATER 0.0
		// NOTE: make them easier to see with 0.3333, was 0.5 in original
		glAlphaFunc(GL_GREATER, 1.0f / 3.0f);
	}

	//if (R_SpriteAllowLerping(e, psprite))
	//	lerp = R_GetSpriteFrameInterpolant(e, &oldframe, &frame);
	//else 
		frame = oldframe = R_GetSpriteFrame(model, e->curstate.frame, e->angles[YAW]);

	type = psprite->type;

	// automatically roll parallel sprites if requested
	if (e->angles[ROLL] != 0.0f && type == SPR_VP_PARALLEL)
		type = SPR_VP_PARALLEL_ORIENTED;

	switch (type)
	{
	case SPR_ORIENTED:
		AngleVectors(e->angles, v_forward, v_right, v_up);
		VectorScale(v_forward, 0.01f, v_forward);	// to avoid z-fighting
		VectorSubtract(origin, v_forward, origin);
		break;
	case SPR_FACING_UPRIGHT:
		v_right = Vector(origin[1] - gBSPRenderer.m_RefDef.vieworg[1], -(origin[0] - gBSPRenderer.m_RefDef.vieworg[0]), 0.0f);
		v_up = Vector(0.0f, 0.0f, 1.0f);
		VectorNormalize(v_right);
		break;
	case SPR_VP_PARALLEL_UPRIGHT:
		dot = gBSPRenderer.m_RefDef.forward[2];
		if ((dot > 0.999848f) || (dot < -0.999848f))	// cos(1 degree) = 0.999848
			return; // invisible
		v_up = Vector(0.0f, 0.0f, 1.0f);
		v_right = Vector(gBSPRenderer.m_RefDef.forward[1], -gBSPRenderer.m_RefDef.forward[0], 0.0f);
		VectorNormalize(v_right);
		break;
	case SPR_VP_PARALLEL_ORIENTED:
		angle = e->angles[ROLL] * ((M_PI * 2) / 360.0f);
		SinCos_(angle, &sr, &cr);
		for (i = 0; i < 3; i++)
		{
			v_right[i] = (gBSPRenderer.m_RefDef.right[i] * cr + gBSPRenderer.m_RefDef.up[i] * sr);
			v_up[i] = gBSPRenderer.m_RefDef.right[i] * -sr + gBSPRenderer.m_RefDef.up[i] * cr;
		}
		break;
	case SPR_VP_PARALLEL: // normal sprite
	default:
		VectorCopy(gBSPRenderer.m_RefDef.right, v_right);
		VectorCopy(gBSPRenderer.m_RefDef.up, v_up);
		break;
	}

	if (oldframe == frame)
	{
		// draw the single non-lerped frame
		glColor4f(color[0], color[1], color[2], r_blend);
		glBindTexture(GL_TEXTURE_2D, frame->gl_texturenum);
		R_DrawSpriteQuad(frame, origin, v_right, v_up, scale);
	}
	else
	{
		// draw two combined lerped frames
		lerp = bound(0.0f, lerp, 1.0f);
		ilerp = 1.0f - lerp;

		if (ilerp != 0.0f)
		{
			glColor4f(color[0], color[1], color[2], r_blend* ilerp);
			glBindTexture(GL_TEXTURE_2D, oldframe->gl_texturenum);
			R_DrawSpriteQuad(oldframe, origin, v_right, v_up, scale);
		}

		if (lerp != 0.0f)
		{
			glColor4f(color[0], color[1], color[2], r_blend * lerp);
			glBindTexture(GL_TEXTURE_2D, frame->gl_texturenum);
			R_DrawSpriteQuad(frame, origin, v_right, v_up, scale);
		}
	}

	// draw the sprite 'lightmap' :-)
	if (R_SpriteHasLightmap(e, psprite->texFormat))
	{
		if (!gEngfuncs.pfnGetCvarFloat("r_lightmap"))
			glEnable(GL_BLEND);
		else glDisable(GL_BLEND);
		glDepthFunc(GL_EQUAL);
		glDisable(GL_ALPHA_TEST);
		glBlendFunc(GL_ZERO, GL_SRC_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		glColor4f(color2[0], color2[1], color2[2], r_blend);
		glBindTexture(GL_TEXTURE_2D, 0);
		R_DrawSpriteQuad(frame, origin, v_right, v_up, scale);
		glAlphaFunc(GL_GREATER, 0.0f);
		glDepthFunc(GL_LEQUAL);
		glDisable(GL_BLEND);
	}


	glDisable(GL_ALPHA_TEST);
	glDepthMask(GL_TRUE);

	//if (e->curstate.rendermode == kRenderGlow || e->curstate.rendermode == kRenderTransAdd)
	//	R_AllowFog(true);

	if (e->curstate.rendermode != kRenderNormal)
	{
		glDisable(GL_BLEND);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glEnable(GL_DEPTH_TEST);
	}
}

Vector R_GetAttachmentPoint(int entity, int attachment)
{
	auto pEntity = gEngfuncs.GetEntityByIndex(entity);

	if (attachment)
		return pEntity->attachment[attachment - 1];

	return pEntity->origin;
}

void R_DrawTempEntities(bool bBrushes)
{
	for (int i = 0; i < cl_numvisedicts; i++)
	{
		if (!bBrushes)
		{
			if (cl_visedicts[i]->model->type == mod_studio)
			{
				g_StudioRenderer.m_pCurrentEntity = cl_visedicts[i];
				g_StudioRenderer.StudioDrawModel(STUDIO_RENDER);
			}
			else if (cl_visedicts[i]->model->type == mod_sprite)
			{
				R_DrawSpriteModel(cl_visedicts[i]);
			}
		}
		else if (cl_visedicts[i]->model->type == mod_brush)
		{
			gBSPRenderer.DrawBrushModel(cl_visedicts[i]);
		}
	}
}

extern ref_params_t* r_refdef;

int V_FadeAlpha()
{
	bool fade_in = (engine_cl->sf.fadeFlags & FFADE_IN);
	bool fade_out = (engine_cl->sf.fadeFlags & FFADE_OUT);
	bool fade_modulate = (engine_cl->sf.fadeFlags & FFADE_MODULATE);
	bool fade_stayout = (engine_cl->sf.fadeFlags & FFADE_STAYOUT);
	bool fade_longfade = (engine_cl->sf.fadeFlags & FFADE_LONGFADE);

	float time = engine_cl->time;

	int result = 0;
	float fadetime = 0;
	int fadealpha = 0;

	int alpha;

	if (time > engine_cl->sf.fadeReset && time > engine_cl->sf.fadeEnd)
	{
		if (!engine_cl->sf.fadeFlags & FFADE_STAYOUT)
			return 0;
	}

	if (engine_cl->sf.fadeFlags & FFADE_STAYOUT)
	{
		alpha = engine_cl->sf.fadealpha;
		if ((engine_cl->sf.fadeFlags & FFADE_OUT) && engine_cl->sf.fadeTotalEnd > time)
		{
			alpha += engine_cl->sf.fadeSpeed * (engine_cl->sf.fadeTotalEnd - time);
		}
		else
		{
			engine_cl->sf.fadeEnd = time + 0.1;
		}
	}
	else
	{
		alpha = engine_cl->sf.fadeSpeed * (engine_cl->sf.fadeEnd - time);
		if (engine_cl->sf.fadeFlags & FFADE_OUT)
		{
			alpha += engine_cl->sf.fadealpha;
		}
	}
	alpha = bound(0, alpha, engine_cl->sf.fadealpha);

	return alpha;
}

void R_PolyBlend()
{
	int alpha = V_FadeAlpha();
	if (!alpha)
		return;

	byte color[4];
	int glx = r_refdef->viewport[0];
	int gly = r_refdef->viewport[1];
	int glwidth = r_refdef->viewport[2];
	int glheight = r_refdef->viewport[3];

	//GL_DisableMultitexture();
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);
	if ((engine_cl->sf.fadeFlags & FFADE_MODULATE) != 0)
	{
		glBlendFunc(GL_ZERO, GL_SRC_COLOR);
		color[3] = -1;
		color[0] = color[1] = color[2] = (alpha * (engine_cl->sf.fader - 255) - 511) >> 8;
	}
	else
	{
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		color[3] = alpha;
		color[0] = color[1] = color[2] = engine_cl->sf.fadeb;
	}

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, glwidth, glheight, 0, -99999.0, 99999.0);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glDisable(GL_CULL_FACE);
	glColor4ubv(color);
	glBegin(GL_QUADS);
	glVertex2f(0, 0);
	glVertex2f(0, glheight);
	glVertex2f(glwidth, glheight);
	glVertex2f(glwidth, 0);
	glEnd();
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_ALPHA_TEST);
}

/*
=================
HUD_DrawNormalTriangles

Non-transparent triangles-- add them here
=================
*/
void DLLEXPORT HUD_DrawNormalTriangles()
{
    extern int restore_numleafs;
    if (restore_numleafs)
        gBSPRenderer.m_pWorld->numleafs = restore_numleafs;


    glColor4f(1, 1, 1, 1);
    glBlendFunc(GL_ONE, GL_ONE);
    glDisable(GL_BLEND);

    R_DrawNormalTriangles();

    R_DrawTransparentTriangles();

    gHUD.m_Spectator.DrawOverview();

	R_PolyBlend();

#ifdef JPH_DEBUG_RENDERER
    //PhysDebug_Draw();
#endif

    r_refdef->onlyClientDraw = 0; //for sound

    gBSPRenderer.m_pWorld->numleafs = 0; //so engine's r_visframecount doesnt infestate into the world
}

#if defined( _TFC )
void RunEventList();
#endif

/*
=================
HUD_DrawTransparentTriangles

Render any triangles with transparent rendermode needs here
=================
*/

void DLLEXPORT HUD_DrawTransparentTriangles( void )
{
    //empty, cant rely on this because it doesnt get called under certain circumstances.
}

// Draw Blood

void DrawBloodOverlay()
{
    if (gHUD.m_Health.m_iHealth < 30) {
        gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd); //additive
        gEngfuncs.pTriAPI->Color4f(1, 1, 1, 1); //set 

        //calculate opacity
        float scale = (30 - gHUD.m_Health.m_iHealth) / 30.0f;
        if (gHUD.m_Health.m_iHealth != 0)
            gEngfuncs.pTriAPI->Brightness(scale);
        else
            gEngfuncs.pTriAPI->Brightness(1);

        //gEngfuncs.Con_Printf("scale :  %f health : %i", scale, gHUD.m_Health.m_iHealth);

        gEngfuncs.pTriAPI->SpriteTexture((struct model_s*)
           GetSpritePointer(SPR_Load("sprites/damagehud.spr")), 4);
        gEngfuncs.pTriAPI->CullFace(TRI_NONE); //no culling
        gEngfuncs.pTriAPI->Begin(TRI_QUADS); //start our quad

        //top left
        gEngfuncs.pTriAPI->TexCoord2f(0.0f, 1.0f);
        gEngfuncs.pTriAPI->Vertex3f(0, 0, 0);

        //bottom left
        gEngfuncs.pTriAPI->TexCoord2f(0.0f, 0.0f);
        gEngfuncs.pTriAPI->Vertex3f(0, ScreenHeight, 0);

        //bottom right
        gEngfuncs.pTriAPI->TexCoord2f(1.0f, 0.0f);
        gEngfuncs.pTriAPI->Vertex3f(ScreenWidth, ScreenHeight, 0);

        //top right
        gEngfuncs.pTriAPI->TexCoord2f(1.0f, 1.0f);
        gEngfuncs.pTriAPI->Vertex3f(ScreenWidth, 0, 0);

        gEngfuncs.pTriAPI->End(); //end our list of vertexes
        gEngfuncs.pTriAPI->RenderMode(kRenderNormal); //return to normal
    }

    if (gHUD.isSlowmo)
    {
        gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd); //additive
        gEngfuncs.pTriAPI->Color4f(1, 1, 1, 1); //set 

        //calculate opacity
        gEngfuncs.pTriAPI->Brightness(gHUD.slowmoStrength);

        //gEngfuncs.Con_Printf("scale :  %f health : %i", scale, gHUD.m_Health.m_iHealth);

        gEngfuncs.pTriAPI->SpriteTexture((struct model_s*)
           GetSpritePointer(SPR_Load("sprites/slowmo.spr")), 4);
        gEngfuncs.pTriAPI->CullFace(TRI_NONE); //no culling
        gEngfuncs.pTriAPI->Begin(TRI_QUADS); //start our quad

        //top left
        gEngfuncs.pTriAPI->TexCoord2f(0.0f, 1.0f);
        gEngfuncs.pTriAPI->Vertex3f(0, 0, 0);

        //bottom left
        gEngfuncs.pTriAPI->TexCoord2f(0.0f, 0.0f);
        gEngfuncs.pTriAPI->Vertex3f(0, ScreenHeight, 0);

        //bottom right
        gEngfuncs.pTriAPI->TexCoord2f(1.0f, 0.0f);
        gEngfuncs.pTriAPI->Vertex3f(ScreenWidth, ScreenHeight, 0);

        //top right
        gEngfuncs.pTriAPI->TexCoord2f(1.0f, 1.0f);
        gEngfuncs.pTriAPI->Vertex3f(ScreenWidth, 0, 0);

        gEngfuncs.pTriAPI->End(); //end our list of vertexes
        gEngfuncs.pTriAPI->RenderMode(kRenderNormal); //return to normal
    }

    if (gHUD.slowmoUpdate < gHUD.m_flTime)
    {
        if (gHUD.slowmoStrength <= 0.5f)
            gHUD.slowmoMode = 1;
        else if (gHUD.slowmoStrength >= 1.0f)
            gHUD.slowmoMode = 2;

        if(gHUD.slowmoMode == 1)
            gHUD.slowmoStrength += 0.05f;
        else if (gHUD.slowmoMode == 2)
            gHUD.slowmoStrength -= 0.05f;

        gHUD.slowmoUpdate = gHUD.m_flTime + 0.01f;
    }

}

void HUD_DrawBloodOverlay(void)
{
    DrawBloodOverlay();
}
