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


extern int cl_numvisedicts;
extern cl_entity_s* cl_visedicts[512];

extern client_state_s* engine_cl;

Vector r_entorigin;


void R_GetSpriteAxes(cl_entity_t* pEntity, int type, Vector& forward, Vector& right, Vector& up)
{
	Vector origin, vup, vright, vpn;
	IEngineStudio.GetViewInfo(origin, vup, vright, vpn);

	if ((type != SPR_VP_PARALLEL || pEntity->angles[2] == 0) && type != SPR_VP_PARALLEL_ORIENTED)
	{
		float v, dot;
		switch (type)
		{
		case 0:
			if (vpn[2] > 0.999848 || vpn[2] < -0.999848)
				break;
			up[0] = 0.0;
			up[1] = 0.0;
			up[2] = 1.0;

			right[0] = vpn[1];
			right[1] = -vpn[0];
			right[2] = 0;

			VectorNormalize(right);
			forward[0] = -right[1];
			forward[1] = right[0];
			forward[2] = 0;
			break;
		case 1:
			v = -origin[0];
			dot = -origin[2];
			v = sqrt(v * v);
			if (dot > 0.999848 || dot < -0.999848)
				break;

			up[0] = 0;
			up[1] = 0;
			up[2] = 1;

			right[0] = -origin[1];
			right[1] = -v;
			right[2] = 0;
			VectorNormalize(right);
			forward[0] = -right[1];
			forward[1] = right[0];
			forward[2] = 0;
			break;
		case 2:
			VectorCopy(vup, up);
			VectorCopy(vright, right);
			VectorCopy(vpn, forward);
			break;
		case 3:
			AngleVectors(pEntity->angles, forward, right, up);
			break;
		default:
			gEngfuncs.Con_DPrintf("R_DrawSprite: Bad sprite type %d", type);
		}
	}
	else
	{
		float error = (180 / M_PI) * pEntity->angles[2];
		float s = sin(error);
		float c = cos(error);
		;

		for (int i = 0; i < 3; i++)
		{
			forward[i] = vpn[i];
			right[i] = vright[i] * c + vup[i] * s;
			up[i] = vright[i] * -s + vup[i] * c;
		}
	}
}

void R_SpriteColor(colorVec* pColor, cl_entity_t* pEntity, int alpha)
{
	int a;

	if (pEntity->curstate.rendermode == kRenderGlow || pEntity->curstate.rendermode == kRenderTransAdd)
		a = clamp(alpha, 0, 255); // some entities > 255 wtf?
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

mspriteframe_t* R_GetSpriteFrame(msprite_t* pSprite, int frame)
{
	mspriteframe_t* pspriteframe;

	if (!pSprite)
	{
		gEngfuncs.Con_DPrintf("Sprite:  no pSprite!!!\n");
		return NULL;
	}

	if (!pSprite->numframes)
	{
		gEngfuncs.Con_DPrintf("Sprite:  pSprite has no frames!!!\n");
		return NULL;
	}

	if ((frame >= pSprite->numframes) || (frame < 0))
	{
		gEngfuncs.Con_DPrintf("Sprite: no such frame %d\n", frame);
		frame = 0;
	}

	if (pSprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = pSprite->frames[frame].frameptr;
	}
	else
	{
		pspriteframe = NULL;
	}

	return pspriteframe;
}

void R_DrawSpriteModel(cl_entity_t* e)
{
	Vector forward, right, up, point;
	msprite_t* psprite = (msprite_t*)e->model->cache.data;
	mspriteframe_t* spriteFrame = R_GetSpriteFrame(psprite, e->curstate.frame);
	if (!spriteFrame)
	{
		gEngfuncs.Con_DPrintf("R_DrawSpriteModel:  couldn't get sprite frame for %s\n", e->model->name);
		return;
	}

	float scale = e->curstate.scale;

	if (scale <= 0)
		scale = 1;

	float r_blend = 1.0f;

	if (e->curstate.rendermode == kRenderNormal)
		r_blend = 1.0;

	colorVec color;

	R_SpriteColor(&color, e, r_blend * 255);

	if (e->curstate.rendermode == kRenderGlow)
	{
		float distance = (r_entorigin - gBSPRenderer.m_vRenderOrigin).Length() * 0.0015f;

		r_blend -= clamp(distance, 0.f, 1.0f);
	}

	r_blend *= 255.0f;

	if (!gEngfuncs.pfnGetCvarPointer("gl_spriteblend")->value && !e->curstate.rendermode)
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glColor4ub(color.r, color.g, color.b, 255);
		glDisable(GL_BLEND);
	}
	switch (e->curstate.rendermode)
	{
	case kRenderTransColor:
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ALPHA);
		glColor4ub(color.r, color.g, color.b, r_blend);
		glEnable(GL_BLEND);
		break;
	case kRenderTransAdd:
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glBlendFunc(GL_ONE, GL_ONE);
		glColor4ub(color.r, color.g, color.b, 255);
		glEnable(GL_BLEND);
		break;
	case kRenderGlow:
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glColor4ub(color.r, color.g, color.b, r_blend);
		glEnable(GL_BLEND);
		break;
	case kRenderTransAlpha:
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glColor4ub(color.r, color.g, color.b, r_blend);
		glEnable(GL_BLEND);
		break;
	}
	R_GetSpriteAxes(e, psprite->type, forward, right, up);

	glBindTexture(GL_TEXTURE_2D, spriteFrame->gl_texturenum);
	glEnable(GL_ALPHA_TEST);

	glBegin(GL_QUADS);

	glTexCoord2f(0.0, 1.0);
	VectorMA(r_entorigin, scale * spriteFrame->down, up, point);
	VectorMA(point, scale * spriteFrame->left, up, point);
	glVertex3fv(point);
	glTexCoord2f(0.0, 0.0);
	VectorMA(r_entorigin, scale * spriteFrame->up, up, point);
	VectorMA(point, scale * spriteFrame->left, right, point);
	glVertex3fv(point);
	glTexCoord2f(1.0, 0.0);
	VectorMA(r_entorigin, scale * spriteFrame->up, up, point);
	VectorMA(point, scale * spriteFrame->right, right, point);
	glVertex3fv(point);
	glTexCoord2f(1.0, 1.0);
	VectorMA(r_entorigin, scale * spriteFrame->down, up, point);
	VectorMA(point, scale * spriteFrame->right, right, point);
	glVertex3fv(point);

	glEnd();

	glDisable(GL_ALPHA_TEST);
	glDepthMask(GL_TRUE);
	if (e->curstate.rendermode || gEngfuncs.pfnGetCvarPointer("gl_spriteblend")->value)
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, 7681.0);
		glDisable(GL_BLEND);
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
				if (cl_visedicts[i]->curstate.body)
				{
					r_entorigin = R_GetAttachmentPoint(cl_visedicts[i]->curstate.skin, cl_visedicts[i]->curstate.body);
				}
				else
				{
					r_entorigin = cl_visedicts[i]->origin;
				}
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
            gEngfuncs.GetSpritePointer(SPR_Load("sprites/damagehud.spr")), 4);
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
            gEngfuncs.GetSpritePointer(SPR_Load("sprites/slowmo.spr")), 4);
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
