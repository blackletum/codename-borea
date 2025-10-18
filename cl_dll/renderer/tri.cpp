//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
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

#include <algorithm> //std::clamp

#include "studio.h"
#include "../renderer/StudioModelRenderer.h"
#include "client_state.h"

#include "renderer/goldsrc_spriterenderer.h"

#include "renderer/opengl_utils/GL_Buffers.h"

#include "renderer/opengl_utils/GL_StateHandler.h"
#include "renderer/opengl_utils/GL_VertexArrayObject.h"


extern int cl_numvisedicts;
extern cl_entity_s* cl_visedicts[512];

float r_blend = 0.0f;

#define GLARE_FALLOFF 19000.0f

int CL_FxBlend(cl_entity_t* ent)
{
	int blend = 0;

	// int offset = ((int)ent->curstate.number ) * 363.0f; ida pro says its curstate.number. is this true or is the software just bullshitting me ?
	int offset = ((int)ent->index) * 363.0f;
	switch (ent->curstate.renderfx)
	{
		case kRenderFxPulseSlowWide:
			blend = ent->curstate.renderamt + 0x40 * sin(engine_cl->time * 2 + offset);
			break;
		case kRenderFxPulseFastWide:
			blend = ent->curstate.renderamt + 0x40 * sin(engine_cl->time * 8 + offset);
			break;
		case kRenderFxPulseSlow:
			blend = ent->curstate.renderamt + 0x10 * sin(engine_cl->time * 2 + offset);
			break;
		case kRenderFxPulseFast:
			blend = ent->curstate.renderamt + 0x10 * sin(engine_cl->time * 8 + offset);
			break;
		case kRenderFxFadeSlow:

			if (ent->curstate.renderamt > 0)
				ent->curstate.renderamt -= 1;
			else
				ent->curstate.renderamt = 0;

			blend = ent->curstate.renderamt;
			break;
		case kRenderFxFadeFast:

			if (ent->curstate.renderamt > 3)
				ent->curstate.renderamt -= 4;
			else
				ent->curstate.renderamt = 0;

				blend = ent->curstate.renderamt;
			break;
		case kRenderFxSolidSlow:
			if (ent->curstate.renderamt < 255)
				ent->curstate.renderamt += 1;
				else
				ent->curstate.renderamt = 255;
				blend = ent->curstate.renderamt;
			break;
		case kRenderFxSolidFast:
			if (ent->curstate.renderamt < 252)
				ent->curstate.renderamt += 4;
				else
				ent->curstate.renderamt = 255;
				blend = ent->curstate.renderamt;
			break;
		case kRenderFxStrobeSlow:
			blend = 20 * sin(engine_cl->time * 4 + offset);
			if (blend < 0)
				blend = 0;
			else
				blend = ent->curstate.renderamt;
			break;
		case kRenderFxStrobeFast:
			blend = 20 * sin(engine_cl->time * 16 + offset);
			if (blend < 0)
				blend = 0;
			else
				blend = ent->curstate.renderamt;
			break;
		case kRenderFxStrobeFaster:
			blend = 20 * sin(engine_cl->time * 36 + offset);
			if (blend < 0)
				blend = 0;
			else
				blend = ent->curstate.renderamt;
			break;
		case kRenderFxFlickerSlow:
			blend = 20 * (sin(engine_cl->time * 2) + sin(engine_cl->time * 17 + offset));
			if (blend < 0)
				blend = 0;
			else
				blend = ent->curstate.renderamt;
			break;
		case kRenderFxFlickerFast:
			blend = 20 * (sin(engine_cl->time * 16) + sin(engine_cl->time * 23 + offset));
			if (blend < 0)
				blend = 0;
			else
				blend = ent->curstate.renderamt;
			break;
		case kRenderFxHologram:
		case kRenderFxDistort:
		{
			Vector tmp = ent->origin - gBSPRenderer.m_RefParams.vieworg;
			float dist = DotProduct(tmp, gBSPRenderer.m_RefParams.forward);

			// turn off distance fade
			if (ent->curstate.renderfx == kRenderFxDistort)
				dist = 1;

			if (dist <= 0)
			{
				blend = 0;
			}
			else
			{
				ent->curstate.renderamt = 180;
				if (dist <= 100)
					blend = ent->curstate.renderamt;
				else
					blend = (int)((1.0f - (dist - 100) * (1.0f / 400.0f)) * ent->curstate.renderamt);
				blend += gEngfuncs.pfnRandomLong(-32, 31);
			}
			break;
		}
		default:
			blend = ent->curstate.renderamt;
			break;
	}

	return bound(0, blend, 255);
}

Vector R_GetAttachmentPoint(int entity, int attachment)
{
	auto pEntity = gEngfuncs.GetEntityByIndex(entity);

	if (attachment)
		return pEntity->attachment[attachment - 1];

	return pEntity->origin;
}

#define RENDERER_TIME "Renderer_Time"

extern ref_params_t* r_refdef;
/*
=================
HUD_DrawNormalTriangles

Non-transparent triangles-- add them here
salsa: anything can be rendered here, transparent or not
	this is only not called if r_norefresh is 1.
=================
*/

extern ref_params_t* r_refdef;

void DLLEXPORT HUD_DrawNormalTriangles()
{
	extern int restore_numleafs;
	if (restore_numleafs)
		engine_cl->worldmodel->numleafs = restore_numleafs;

	 // god fucking dammit developer cvar, stop messing up our RENDERER FOR CHRIST SAKE
	g_GlobalGLState.ResetStates();
	g_GlobalGLState.SetBlend(false);
	g_GlobalGLState.SetBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	g_GlobalGLState.SetAlphaTest(false);
	g_GlobalGLState.SetDepthTest(true);
	g_GlobalGLState.SetDepthWrite(true);
	glBindTexture(GL_TEXTURE_2D, 0);
	glColor4f(1, 1, 1, 1);

	R_DrawNormalTriangles();

	gHUD.m_Spectator.DrawOverview();

	GL_BufferHandler::ResetBufferBinding(GL_BufferHandler::ArrayBuffer);
	GL_BufferHandler::ResetBufferBinding(GL_BufferHandler::ElementArrayBuffer);
	GL_VertexArrayObject::ResetVAOBinding();

	r_refdef->onlyClientDraw = 0; // for sound

	engine_cl->worldmodel->numleafs = 0; // so engine's r_visframecount doesnt infestate into the world
}

/*
=================
HUD_DrawTransparentTriangles

Render any triangles with transparent rendermode needs here
salsa: does not get called if r_drawentities is zero
=================
*/
void DLLEXPORT HUD_DrawTransparentTriangles()
{
	//empty
}
