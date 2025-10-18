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
#include "bsprenderer.h"

#include "r_efx.h"
#include "r_studioint.h"
#include "studio_util.h"

#include "textureloader.h"
#include "particle_engine.h"

#include "StudioModelRenderer.h"

#include "customentity.h"

#include "Exports.h"

#include "goldsrc_beamrenderer.h"

#include "opengl_utils/GL_StateHandler.h"
#include "opengl_utils/GL_ShaderProgram.h"

std::vector<std::unique_ptr<particle_t>> m_DummyParticles;

#define NOISE_DIVISIONS 64 // don't touch - many tripmines cause the crash when it equal 128

typedef struct
{
	Vector pos;
	float texcoord; // Y texture coordinate
	float width;
} beamseg_t;

extern void SinCos_(float radians, float* sine, float* cosine);

/*
==============================================================

FRACTAL NOISE

==============================================================
*/
static float rgNoise[NOISE_DIVISIONS + 1]; // global noise array


// freq2 += step * 0.1;
// Fractal noise generator, power of 2 wavelength
static void FracNoise(float* noise, int divs)
{
	int div2;

	div2 = divs >> 1;
	if (divs < 2)
		return;

	// noise is normalized to +/- scale
	noise[div2] = (noise[0] + noise[divs]) * 0.5f + divs * gEngfuncs.pfnRandomFloat(-0.125f, 0.125f);

	if (div2 > 1)
	{
		FracNoise(&noise[div2], div2);
		FracNoise(noise, div2);
	}
}

static void SineNoise(float* noise, int divs)
{
	float freq = 0;
	float step = M_PI / (float)divs;
	int i;

	for (i = 0; i < divs; i++)
	{
		noise[i] = sin(freq);
		freq += step;
	}
}

/*
==============
R_BeamGetEntity

extract entity number from index
handle user entities
==============
*/
cl_entity_t* R_BeamGetEntity(int index)
{
	if (index < 0)
		return HUD_GetUserEntity(BEAMENT_ENTITY(-index));
	return gEngfuncs.GetEntityByIndex(BEAMENT_ENTITY(index));
}

CGoldSrc_BeamRenderer g_BeamRenderer;


extern int CL_FxBlend(cl_entity_t* ent);

void CGoldSrc_BeamRenderer::R_BeamComputePerpendicular(const Vector vecBeamDelta, Vector& pPerp)
{
	// direction in worldspace of the center of the beam
	Vector vecBeamCenter;

	vecBeamCenter = vecBeamDelta, vecBeamCenter;
	CrossProduct(gBSPRenderer.m_RefParams.forward, vecBeamCenter, pPerp);
	pPerp = pPerp.Normalize();
}

void CGoldSrc_BeamRenderer::R_BeamComputeNormal(const Vector vStartPos, const Vector vNextPos, Vector& pNormal)
{
	// vTangentY = line vector for beam
	Vector vTangentY, vDirToBeam;

	vTangentY = (vStartPos - vNextPos);

	// vDirToBeam = vector from viewer origin to beam
	vDirToBeam = (vStartPos - gBSPRenderer.m_vRenderOrigin);

	// get a vector that is perpendicular to us and perpendicular to the beam.
	// this is used to fatten the beam.
	CrossProduct(vTangentY, vDirToBeam, pNormal);
	pNormal = pNormal.Normalize();
}

void CGoldSrc_BeamRenderer::SetBeamAttributes(BEAM* pbeam, float r, float g, float b, float framerate, float startFrame)
{
	pbeam->frameRate = framerate;
	pbeam->frame = startFrame;
	pbeam->r = r;
	pbeam->g = g;
	pbeam->b = b;
}


/*
==============
R_BeamComputePoint

compute attachment point for beam
==============
*/
qboolean CGoldSrc_BeamRenderer::R_BeamComputePoint(int beamEnt, Vector& pt)
{
	cl_entity_t* ent;
	int attach;

	ent = R_BeamGetEntity(beamEnt);

	if (beamEnt < 0)
		attach = BEAMENT_ATTACHMENT(-beamEnt);
	else
		attach = BEAMENT_ATTACHMENT(beamEnt);

	if (!ent)
	{
		gEngfuncs.Con_DPrintf("R_BeamComputePoint: invalid entity %i\n", BEAMENT_ENTITY(beamEnt));
		pt = Vector(0, 0, 0);
		return false;
	}

	// get attachment
	if (attach > 0)
	{
		g_StudioRenderer.m_pCurrentEntity = ent;
		g_StudioRenderer.m_pStudioHeader = (studiohdr_t*)IEngineStudio.Mod_Extradata(ent->model);
		g_StudioRenderer.StudioSetUpTransform(0);
		g_StudioRenderer.StudioSetupBones();
		g_StudioRenderer.StudioCalcAttachments();
		pt = ent->attachment[attach - 1];
	}
	else if ((ent->index - 1) == engine_cl->playernum)
		pt = engine_cl->simorg;
	else
		pt = ent->origin;

	return true;
}


qboolean CGoldSrc_BeamRenderer::R_BeamRecomputeEndpoints(BEAM* pbeam)
{
	if (pbeam->flags & FBEAM_STARTENTITY)
	{
		cl_entity_t* start = R_BeamGetEntity(pbeam->startEntity);

		if (R_BeamComputePoint(pbeam->startEntity, pbeam->source))
		{
			if (!pbeam->pFollowModel)
				pbeam->pFollowModel = start->model;
			pbeam->flags |= FBEAM_STARTVISIBLE;
		}
		else if (!(pbeam->flags & FBEAM_FOREVER))
		{
			pbeam->flags &= ~FBEAM_STARTENTITY;
		}
	}

	if (pbeam->flags & FBEAM_ENDENTITY)
	{
		cl_entity_t* end = R_BeamGetEntity(pbeam->endEntity);

		if (R_BeamComputePoint(pbeam->endEntity, pbeam->target))
		{
			if (!pbeam->pFollowModel)
				pbeam->pFollowModel = end->model;
			pbeam->flags |= FBEAM_ENDVISIBLE;
		}
		else if (!(pbeam->flags & FBEAM_FOREVER))
		{
			pbeam->flags &= ~FBEAM_ENDENTITY;
			pbeam->die = engine_cl->time;
			return false;
		}
		else
		{
			return false;
		}
	}

	if ((pbeam->flags & FBEAM_STARTENTITY) && !(pbeam->flags & FBEAM_STARTVISIBLE))
		return false;
	return true;
}

/*
==============
R_BeamCull

Cull the beam by bbox
==============
*/
qboolean CGoldSrc_BeamRenderer::R_BeamCull(const Vector start, const Vector end, qboolean pvsOnly)
{
	Vector mins, maxs;
	int i;

	for (i = 0; i < 3; i++)
	{
		if (start[i] < end[i])
		{
			mins[i] = start[i];
			maxs[i] = end[i];
		}
		else
		{
			mins[i] = end[i];
			maxs[i] = start[i];
		}

		// don't let it be zero sized
		if (mins[i] == maxs[i])
			maxs[i] += 1.0f;
	}

	// check bbox
	if (!gHUD.viewFrustum.CullBox(mins, maxs))
	{
		// beam is visible
		return false;
	}

	// beam is culled
	return true;
}

void CGoldSrc_BeamRenderer::R_BeamSetup(BEAM* pbeam, Vector start, Vector end, int modelIndex, float life, float width, float amplitude, float brightness, float speed)
{
	model_t* sprite = CL_GetModelByIndex(modelIndex);

	if (!sprite)
		return;

	pbeam->type = BEAM_POINTS;
	pbeam->modelIndex = modelIndex;
	pbeam->frame = 0;
	pbeam->frameRate = 0;
	pbeam->frameCount = sprite->numframes;

	pbeam->source = start;
	pbeam->target = end;
	pbeam->delta = end - start;

	pbeam->freq = speed * engine_cl->time;
	pbeam->die = life + engine_cl->time;
	pbeam->amplitude = amplitude;
	pbeam->brightness = brightness;
	pbeam->width = width;
	pbeam->speed = speed;

	if (amplitude >= 0.50f)
		pbeam->segments = pbeam->delta.Length() * 0.25f + 3.0f; // one per 4 pixels
	else
		pbeam->segments = pbeam->delta.Length() * 0.075f + 3.0f; // one per 16 pixels

	pbeam->pFollowModel = NULL;
	pbeam->flags = 0;
}

extern int ModelFrameCount(model_t* model); // engine_hookfuncs.cpp

void CGoldSrc_BeamRenderer::R_DrawBeams(float frametime)
{
	if (m_BeamEnt_List.empty() && m_BeamTempEnt_List.empty())
		return;

	GL_ShaderProgram::ResetShaderBind();

	for (auto beament : m_BeamEnt_List)
	{
		// draw it
		R_BeamDrawCustomEntity(beament);
	}

	for (int i = m_BeamTempEnt_List.size() - 1; i >= 0; i--)
	{
		BEAM* tbeam = m_BeamTempEnt_List[i].get();
		R_BeamDraw(tbeam, engine_cl->time - engine_cl->oldtime);

		CL_RunBeamLogic(tbeam, i);
	}
}

/*
==============
R_BeamDrawCustomEntity

initialize beam from server entity
==============
*/
void CGoldSrc_BeamRenderer::R_BeamDrawCustomEntity(cl_entity_t* ent)
{
	BEAM beam;
	float amp = ent->curstate.body / 100.0f;
	float blend = CL_FxBlend(ent) / 255.0f;
	float r, g, b;
	int beamFlags;

	r = ent->curstate.rendercolor.r / 255.0f;
	g = ent->curstate.rendercolor.g / 255.0f;
	b = ent->curstate.rendercolor.b / 255.0f;

	R_BeamSetup(&beam, ent->origin, ent->curstate.angles, ent->curstate.modelindex, 0, ent->curstate.scale, amp, blend, ent->curstate.animtime);
	SetBeamAttributes(&beam, r, g, b, ent->curstate.framerate, ent->curstate.frame);
	beam.pFollowModel = NULL;

	switch (ent->curstate.rendermode & 0x0F)
	{
	case BEAM_ENTPOINT:
		beam.type = TE_BEAMPOINTS;
		if (ent->curstate.sequence)
		{
			beam.flags |= FBEAM_STARTENTITY;
			beam.startEntity = ent->curstate.sequence;
		}
		if (ent->curstate.skin)
		{
			beam.flags |= FBEAM_ENDENTITY;
			beam.endEntity = ent->curstate.skin;
		}
		break;
	case BEAM_ENTS:
		beam.type = TE_BEAMPOINTS;
		beam.flags |= (FBEAM_STARTENTITY | FBEAM_ENDENTITY);
		beam.startEntity = ent->curstate.sequence;
		beam.endEntity = ent->curstate.skin;
		break;
	case BEAM_HOSE:
		beam.type = TE_BEAMHOSE;
		break;
	case BEAM_POINTS:
		// already set up
		break;
	}

	beamFlags = (ent->curstate.rendermode & 0xF0);

	if (beamFlags & BEAM_FSINE)
		beam.flags |= FBEAM_SINENOISE;

	if (beamFlags & BEAM_FSOLID)
		beam.flags |= FBEAM_SOLID;

	if (beamFlags & BEAM_FSHADEIN)
		beam.flags |= FBEAM_SHADEIN;

	if (beamFlags & BEAM_FSHADEOUT)
		beam.flags |= FBEAM_SHADEOUT;

	// draw it
	R_BeamDraw(&beam, engine_cl->time - engine_cl->oldtime);
}

void CGoldSrc_BeamRenderer::R_BeamDraw(BEAM* pbeam, float frametime)
{
	if (pbeam->modelIndex < 0)
	{
		pbeam->die = engine_cl->time;
		return;
	}
	model_t* beamsprite = CL_GetModelByIndex(pbeam->modelIndex);
	if (!beamsprite)
		return;

	pbeam->freq += frametime;

	// generate fractal noise
	rgNoise[0] = 0;
	rgNoise[NOISE_DIVISIONS] = 0;

	if (pbeam->amplitude != 0 && frametime != 0.0f)
	{
		if (pbeam->flags & FBEAM_SINENOISE)
			SineNoise(rgNoise, NOISE_DIVISIONS);
		else
			FracNoise(rgNoise, NOISE_DIVISIONS);
	}

	Vector delta(0, 0, 0);

	if (pbeam->flags & (FBEAM_STARTENTITY | FBEAM_ENDENTITY))
	{
		// makes sure attachment[0] + attachment[1] are valid
		if (!R_BeamRecomputeEndpoints(pbeam))
		{
			pbeam->flags &= ~FBEAM_ISACTIVE; // force to ignore
			return;
		}

		// compute segments from the new endpoints
		delta = pbeam->target - pbeam->source;
		pbeam->delta = Vector(0, 0, 0);

		if (delta.Length() > 0.0000001f)
			pbeam->delta = delta;

		if (pbeam->amplitude >= 0.50f)
			pbeam->segments = pbeam->delta.Length() * 0.25f + 3.0f; // one per 4 pixels
		else
			pbeam->segments = pbeam->delta.Length() * 0.075f + 3.0f; // one per 16 pixels
	}


	if (pbeam->type == TE_BEAMPOINTS && R_BeamCull(pbeam->source, pbeam->target, 0))
	{
		pbeam->flags &= ~FBEAM_ISACTIVE;
		return;
	}

	// don't draw really short or inactive beams
	if (!(pbeam->flags & FBEAM_ISACTIVE) || pbeam->delta.Length() < 0.1f)
	{
		// return;
	}

	if (pbeam->flags & (FBEAM_FADEIN | FBEAM_FADEOUT))
	{
		// update life cycle
		pbeam->t = pbeam->freq + (pbeam->die - engine_cl->time);
		if (pbeam->t != 0.0f)
			pbeam->t = 1.0f - pbeam->freq / pbeam->t;
	}

	if (pbeam->type == TE_BEAMHOSE)
	{
		float flDot;

		VectorSubtract(pbeam->target, pbeam->source, delta);
		VectorNormalize(delta);

		flDot = DotProduct(delta, gBSPRenderer.m_RefParams.forward);

		// abort if the player's looking along it away from the source
		if (flDot > 0)
		{
			return;
		}
		else
		{
			float flFade = pow(flDot, 10);
			Vector localDir, vecProjection, tmp;
			float flDistance;

			// fade the beam if the player's not looking at the source
			VectorSubtract(gBSPRenderer.m_vRenderOrigin, pbeam->source, localDir);
			flDot = DotProduct(delta, localDir);
			VectorScale(delta, flDot, vecProjection);
			VectorSubtract(localDir, vecProjection, tmp);
			flDistance = tmp.Length();

			if (flDistance > 30)
			{
				flDistance = 1.0f - ((flDistance - 30.0f) / 64.0f);
				if (flDistance <= 0)
					flFade = 0;
				else
					flFade *= pow(flDistance, 3);
			}

			if (flFade < (1.0f / 255.0f))
				return;

			// FIXME: needs to be testing
			pbeam->brightness *= flFade;
		}
	}

	if (pbeam->flags & FBEAM_SOLID)
	{
		g_GlobalGLState.SetBlend(false);
		g_GlobalGLState.SetDepthWrite(true);
		glShadeModel(GL_FLAT);
	}
	else
	{
		g_GlobalGLState.SetBlendFunc(GL_ONE, GL_ONE);
		g_GlobalGLState.SetBlend(true);
		g_GlobalGLState.SetDepthWrite(false);
		glShadeModel(GL_SMOOTH);
	}

	if (!gEngfuncs.pTriAPI->SpriteTexture(beamsprite, (int)(pbeam->frame + pbeam->frameRate * engine_cl->time) % pbeam->frameCount))
	{
		pbeam->flags &= ~FBEAM_ISACTIVE;
		return;
	}

	if (pbeam->type == TE_BEAMFOLLOW)
	{
		cl_entity_t* pStart;

		// XASH SPECIFIC: get brightness from head entity
		pStart = R_BeamGetEntity(pbeam->startEntity);
		if (pStart && pStart->curstate.rendermode != kRenderNormal)
			pbeam->brightness = CL_FxBlend(pStart) / 255.0f;
	}

	if ((pbeam->flags & FBEAM_FADEIN))
		gEngfuncs.pTriAPI->Color4f(pbeam->r, pbeam->g, pbeam->b, pbeam->t * pbeam->brightness);
	else if (pbeam->flags & FBEAM_FADEOUT)
		gEngfuncs.pTriAPI->Color4f(pbeam->r, pbeam->g, pbeam->b, (1.0f - pbeam->t) * pbeam->brightness);
	else
		gEngfuncs.pTriAPI->Color4f(pbeam->r, pbeam->g, pbeam->b, pbeam->brightness);

	// time to render !!

	switch (pbeam->type)
	{
	case TE_BEAMTORUS:
		glBegin(GL_TRIANGLE_STRIP);
			R_DrawTorus(pbeam->source, pbeam->delta, pbeam->width, pbeam->amplitude, pbeam->freq, pbeam->speed, pbeam->segments);
		glEnd();
		break;
	case TE_BEAMDISK:
		glBegin(GL_TRIANGLE_STRIP);
			R_DrawDisk(pbeam->source, pbeam->delta, pbeam->width, pbeam->amplitude, pbeam->freq, pbeam->speed, pbeam->segments);
		glEnd();
		break;
	case TE_BEAMCYLINDER:
		glBegin(GL_TRIANGLE_STRIP);
			R_DrawCylinder(pbeam->source, pbeam->delta, pbeam->width, pbeam->amplitude, pbeam->freq, pbeam->speed, pbeam->segments);
		glEnd();
		break;
	case TE_BEAMPOINTS:
	case TE_BEAMHOSE:
		glBegin(GL_TRIANGLE_STRIP);
			R_DrawSegs(pbeam->source, pbeam->delta, pbeam->width, pbeam->amplitude, pbeam->freq, pbeam->speed, pbeam->segments, pbeam->flags);
		glEnd();
		break;
	case TE_BEAMFOLLOW:
		glBegin(GL_QUADS);
			R_DrawBeamFollow(pbeam);
		glEnd();
		break;
	case TE_BEAMRING:
		glBegin(GL_TRIANGLE_STRIP);
			R_DrawRing(pbeam->source, pbeam->delta, pbeam->width, pbeam->amplitude, pbeam->freq, pbeam->speed, pbeam->segments);
		glEnd();
		break;
	}

	if (!(pbeam->flags & FBEAM_SOLID))
	{
		g_GlobalGLState.SetBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
		g_GlobalGLState.SetBlend(false);
		g_GlobalGLState.SetDepthWrite(true);
		glShadeModel(GL_FLAT);
	}
}

void CGoldSrc_BeamRenderer::R_DrawSegs(Vector source, Vector delta, float width, float scale, float freq, float speed, int segments, int flags)
{
	int noiseIndex, noiseStep;
	int i, total_segs, segs_drawn;
	float div, length, fraction, factor;
	float flMaxWidth, vLast, vStep, brightness;
	Vector perp1, vLastNormal;
	beamseg_t curSeg;

	if (segments < 2)
		return;

	length = delta.Length();
	flMaxWidth = width * 0.5f;
	div = 1.0f / (segments - 1);

	if (length * div < flMaxWidth * 1.414f)
	{
		// here, we have too many segments; we could get overlap... so lets have less segments
		segments = (int)(length / (flMaxWidth * 1.414f)) + 1.0f;
		if (segments < 2)
			segments = 2;
	}

	if (segments > NOISE_DIVISIONS)
		segments = NOISE_DIVISIONS;

	div = 1.0f / (segments - 1);
	length *= 0.01f;
	vStep = length * div; // Texture length texels per space pixel

	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	vLast = fmod(freq * speed, 1);

	if (flags & FBEAM_SINENOISE)
	{
		if (segments < 16)
		{
			segments = 16;
			div = 1.0f / (segments - 1);
		}
		scale *= 100.0f;
		length = segments * 0.1f;
	}
	else
	{
		scale *= length * 2.0f;
	}

	// Iterator to resample noise waveform (it needs to be generated in powers of 2)
	noiseStep = (int)((float)(NOISE_DIVISIONS - 1) * div * 65536.0f);
	brightness = 1.0f;
	noiseIndex = 0;

	if (flags & FBEAM_SHADEIN)
		brightness = 0;

	// Choose two vectors that are perpendicular to the beam
	R_BeamComputePerpendicular(delta, perp1);

	total_segs = segments;
	segs_drawn = 0;

	// specify all the segments.
	for (i = 0; i < segments; i++)
	{
		beamseg_t nextSeg;
		Vector vPoint1, vPoint2;

		assert(noiseIndex < (NOISE_DIVISIONS << 16));

		fraction = i * div;

		VectorMA(source, fraction, delta, nextSeg.pos);

		// distort using noise
		if (scale != 0)
		{
			factor = rgNoise[noiseIndex >> 16] * scale;

			if (flags & FBEAM_SINENOISE)
			{
				float s, c;

				SinCos_(fraction * M_PI * length + freq, &s, &c);
				VectorMA(nextSeg.pos, (factor * s), gBSPRenderer.m_RefParams.up, nextSeg.pos);

				// rotate the noise along the perpendicluar axis a bit to keep the bolt from looking diagonal
				VectorMA(nextSeg.pos, (factor * c), gBSPRenderer.m_RefParams.right, nextSeg.pos);
			}
			else
			{
				VectorMA(nextSeg.pos, factor, perp1, nextSeg.pos);
			}
		}

		// specify the next segment.
		nextSeg.width = width * 2.0f;
		nextSeg.texcoord = vLast;

		if (segs_drawn > 0)
		{
			// Get a vector that is perpendicular to us and perpendicular to the beam.
			// This is used to fatten the beam.
			Vector vNormal, vAveNormal;

			R_BeamComputeNormal(curSeg.pos, nextSeg.pos, vNormal);

			if (segs_drawn > 1)
			{
				// Average this with the previous normal
				VectorAdd(vNormal, vLastNormal, vAveNormal);
				VectorScale(vAveNormal, 0.5f, vAveNormal);
				vAveNormal = vAveNormal.Normalize();
			}
			else
			{
				VectorCopy(vNormal, vAveNormal);
			}

			VectorCopy(vNormal, vLastNormal);

			// draw regular segment
			VectorMA(curSeg.pos, (curSeg.width * 0.5f), vAveNormal, vPoint1);
			VectorMA(curSeg.pos, (-curSeg.width * 0.5f), vAveNormal, vPoint2);

			glTexCoord2f(0.0f, curSeg.texcoord);
			gEngfuncs.pTriAPI->Brightness(brightness);
			glNormal3fv(vAveNormal);
			glVertex3fv(vPoint1);

			glTexCoord2f(1.0f, curSeg.texcoord);
			gEngfuncs.pTriAPI->Brightness(brightness);
			glNormal3fv(vAveNormal);
			glVertex3fv(vPoint2);
		}

		curSeg = nextSeg;
		segs_drawn++;

		if ((flags & FBEAM_SHADEIN) && (flags & FBEAM_SHADEOUT))
		{
			if (fraction < 0.5f)
				brightness = fraction;
			else
				brightness = (1.0f - fraction);
		}
		else if (flags & FBEAM_SHADEIN)
		{
			brightness = fraction;
		}
		else if (flags & FBEAM_SHADEOUT)
		{
			brightness = 1.0f - fraction;
		}

		if (segs_drawn == total_segs)
		{
			// draw the last segment
			VectorMA(curSeg.pos, (curSeg.width * 0.5f), vLastNormal, vPoint1);
			VectorMA(curSeg.pos, (-curSeg.width * 0.5f), vLastNormal, vPoint2);

			// specify the points.
			glTexCoord2f(0.0f, curSeg.texcoord);
			gEngfuncs.pTriAPI->Brightness(brightness);
			glNormal3fv(vLastNormal);
			glVertex3fv(vPoint1);

			glTexCoord2f(1.0f, curSeg.texcoord);
			gEngfuncs.pTriAPI->Brightness(brightness);
			glNormal3fv(vLastNormal);
			glVertex3fv(vPoint2);
		}

		vLast += vStep; // Advance texture scroll (v axis only)
		noiseIndex += noiseStep;
	}
}
void CGoldSrc_BeamRenderer::R_DrawTorus(Vector source, Vector delta, float width, float scale, float freq, float speed, int segments)
{
	int i, noiseIndex, noiseStep;
	float div, length, fraction, factor, vLast, vStep;
	Vector last1, last2, point, screen, screenLast, tmp, normal;

	if (segments < 2)
		return;

	if (segments > NOISE_DIVISIONS)
		segments = NOISE_DIVISIONS;

	length = delta.Length() * 0.01;
	if (length < 0.5)
		length = 0.5; // don't lose all of the noise/texture on short beams

	div = 1.0f / (segments - 1);

	vStep = length * div; // Texture length texels per space pixel

	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	vLast = fmod(freq * speed, 1);
	scale = scale * length;

	// Iterator to resample noise waveform (it needs to be generated in powers of 2)
	noiseStep = (int)((float)(NOISE_DIVISIONS - 1) * div * 65536.0f);
	noiseIndex = 0;

	for (i = 0; i < segments; i++)
	{
		float s, c;

		fraction = i * div;
		SinCos_(fraction * (M_PI * 2), &s, &c);

		point[0] = s * freq * delta[2] + source[0];
		point[1] = c * freq * delta[2] + source[1];
		point[2] = source[2];

		// distort using noise
		if (scale != 0)
		{
			if ((noiseIndex >> 16) < NOISE_DIVISIONS)
			{
				factor = rgNoise[noiseIndex >> 16] * scale;
				VectorMA(point, factor, gBSPRenderer.m_RefParams.up, point);

				// rotate the noise along the perpendicluar axis a bit to keep the bolt from looking diagonal
				factor = rgNoise[noiseIndex >> 16] * scale * cos(fraction * M_PI * 3 + freq);
				VectorMA(point, factor, gBSPRenderer.m_RefParams.right, point);
			}
		}

		// Transform point into screen space
		screen = gBSPRenderer.TriWorldToScreen(point);

		if (i != 0)
		{
			// build world-space normal to screen-space direction vector
			tmp = screen - screenLast;

			// we don't need Z, we're in screen space
			tmp[2] = 0;
			tmp = tmp.Normalize();
			VectorScale(gBSPRenderer.m_RefParams.up, -tmp[0], normal); // Build point along noraml line (normal is -y, x)
			VectorMA(normal, tmp[1], gBSPRenderer.m_RefParams.right, normal);

			// Make a wide line
			VectorMA(point, width, normal, last1);
			VectorMA(point, -width, normal, last2);

			vLast += vStep; // advance texture scroll (v axis only)
			glTexCoord2f(1, vLast);
			glVertex3fv(last2);
			glTexCoord2f(0, vLast);
			glVertex3fv(last1);
		}

		VectorCopy(screen, screenLast);
		noiseIndex += noiseStep;
	}
}
void CGoldSrc_BeamRenderer::R_DrawDisk(Vector source, Vector delta, float width, float scale, float freq, float speed, int segments)
{
	float div, length, fraction;
	float w, vLast, vStep;
	Vector point;
	int i;

	if (segments < 2)
		return;

	if (segments > NOISE_DIVISIONS) // UNDONE: Allow more segments?
		segments = NOISE_DIVISIONS;

	length = delta.Length() * 0.01f;
	if (length < 0.5f)
		length = 0.5f; // don't lose all of the noise/texture on short beams

	div = 1.0f / (segments - 1);
	vStep = length * div; // Texture length texels per space pixel

	// scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	vLast = fmod(freq * speed, 1);
	scale = scale * length;

	// clamp the beam width
	w = fmod(freq, width * 0.1f) * delta[2];

	// NOTE: we must force the degenerate triangles to be on the edge
	for (i = 0; i < segments; i++)
	{
		float s, c;

		fraction = i * div;
		point = source;

		gEngfuncs.pTriAPI->Brightness(1.0f);
		glTexCoord2f(1.0f, vLast);
		glVertex3fv(point);

		SinCos_(fraction * (M_PI * 2), &s, &c);
		point[0] = s * w + source[0];
		point[1] = c * w + source[1];
		point[2] = source[2];

		gEngfuncs.pTriAPI->Brightness(1.0f);
		glTexCoord2f(0.0f, vLast);
		glVertex3fv(point);

		vLast += vStep; // advance texture scroll (v axis only)
	}
}
void CGoldSrc_BeamRenderer::R_DrawCylinder(Vector source, Vector delta, float width, float scale, float freq, float speed, int segments)
{
	float div, length, fraction;
	float vLast, vStep;
	Vector point;
	int i;

	if (segments < 2)
		return;

	if (segments > NOISE_DIVISIONS)
		segments = NOISE_DIVISIONS;

	length = delta.Length() * 0.01f;
	if (length < 0.5f)
		length = 0.5f; // don't lose all of the noise/texture on short beams

	div = 1.0f / (segments - 1);
	vStep = length * div; // texture length texels per space pixel

	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	vLast = fmod(freq * speed, 1);
	scale = scale * length;

	for (i = 0; i < segments; i++)
	{
		float s, c;

		fraction = i * div;
		SinCos_(fraction * (M_PI * 2), &s, &c);

		point[0] = s * freq * delta[2] + source[0];
		point[1] = c * freq * delta[2] + source[1];
		point[2] = source[2] + width;

		gEngfuncs.pTriAPI->Brightness(0);
		glTexCoord2f(1, vLast);
		glVertex3fv(point);

		point[0] = s * freq * (delta[2] + width) + source[0];
		point[1] = c * freq * (delta[2] + width) + source[1];
		point[2] = source[2] - width;

		gEngfuncs.pTriAPI->Brightness(1);
		glTexCoord2f(0, vLast);
		glVertex3fv(point);

		vLast += vStep; // Advance texture scroll (v axis only)
	}
}
void CGoldSrc_BeamRenderer::R_DrawRing(Vector source, Vector delta, float width, float amplitude, float freq, float speed, int segments)
{
	int i, j, noiseIndex, noiseStep;
	float div, length, fraction, factor, vLast, vStep;
	Vector  last1, last2, point, screen, screenLast;
	Vector  tmp, normal, center, xaxis, yaxis;
	float radius, x, y, scale;

	if (segments < 2)
		return;

	VectorClear(screenLast);
	segments = segments * M_PI;

	if (segments > NOISE_DIVISIONS * 8)
		segments = NOISE_DIVISIONS * 8;

	length = delta.Length() * 0.01f * M_PI;
	if (length < 0.5f)
		length = 0.5f; // Don't lose all of the noise/texture on short beams

	div = 1.0f / (segments - 1);

	vStep = length * div / 8.0f; // texture length texels per space pixel

	// Scroll speed 3.5 -- initial texture position, scrolls 3.5/sec (1.0 is entire texture)
	vLast = fmod(freq * speed, 1.0f);
	scale = amplitude * length / 8.0f;

	// Iterator to resample noise waveform (it needs to be generated in powers of 2)
	noiseStep = (int)((float)(NOISE_DIVISIONS - 1) * div * 65536.0f) * 8;
	noiseIndex = 0;

	delta = delta * 0.5;
	center = source + delta;

	xaxis = delta;
	radius = xaxis.Length();

	// cull beamring
	// --------------------------------
	// Compute box center +/- radius
	last1 = Vector(radius, radius, scale);
	tmp = center + last1;	 // maxs
	screen = center - last1; // mins

	if (!engine_cl->worldmodel)
		return;

	// is that box in PVS && frustum?
	if (g_StudioRenderer.StudioCullBBox(screen, tmp))
	{
		return;
	}

	yaxis = Vector(xaxis[1], -xaxis[0], 0.0f).Normalize();
	yaxis = yaxis * radius;

	j = segments / 8;

	for (i = 0; i < segments + 1; i++)
	{
		fraction = i * div;
		SinCos_(fraction * (M_PI * 2), &x, &y);

		point = (x * xaxis) + (y, yaxis) + (1.0f * center);

		// distort using noise
		factor = rgNoise[(noiseIndex >> 16) & (NOISE_DIVISIONS - 1)] * scale;
		VectorMA(point, factor, gBSPRenderer.m_RefParams.up, point);

		// Rotate the noise along the perpendicluar axis a bit to keep the bolt from looking diagonal
		factor = rgNoise[(noiseIndex >> 16) & (NOISE_DIVISIONS - 1)] * scale;
		factor *= cos(fraction * M_PI * 24 + freq);
		VectorMA(point, factor, gBSPRenderer.m_RefParams.right, point);

		// Transform point into screen space
		screen = gBSPRenderer.TriWorldToScreen(point);

		if (i != 0)
		{
			// build world-space normal to screen-space direction vector
			VectorSubtract(screen, screenLast, tmp);

			// we don't need Z, we're in screen space
			tmp[2] = 0;
			VectorNormalize(tmp);

			// Build point along normal line (normal is -y, x)
			VectorScale(gBSPRenderer.m_RefParams.up, tmp[0], normal);
			VectorMA(normal, tmp[1], gBSPRenderer.m_RefParams.right, normal);

			// Make a wide line
			VectorMA(point, width, normal, last1);
			VectorMA(point, -width, normal, last2);

			vLast += vStep; // Advance texture scroll (v axis only)
			glTexCoord2f(1.0f, vLast);
			glVertex3fv(last2);
			glTexCoord2f(0.0f, vLast);
			glVertex3fv(last1);
		}

		VectorCopy(screen, screenLast);
		noiseIndex += noiseStep;
		j--;

		if (j == 0 && amplitude != 0)
		{
			j = segments / 8;
			FracNoise(rgNoise, NOISE_DIVISIONS);
		}
	}
}

void CGoldSrc_BeamRenderer::R_DrawBeamFollow(BEAM* pbeam)
{
	particle_t *pnew, *particles;
	float fraction, div, vLast, vStep;
	Vector last1, last2, tmp, screen;
	Vector delta, screenLast, normal;

	float frametime = engine_cl->time - engine_cl->oldtime;

	particles = pbeam->particles;
	pnew = NULL;

	div = 0;

	auto entity = R_BeamGetEntity(pbeam->startEntity);

	if (pbeam->flags & FBEAM_STARTENTITY && pbeam->die > engine_cl->time && entity->curstate.messagenum == engine_cl->parsecount)
	{
		if (particles)
		{
			VectorSubtract(particles->org, pbeam->source, delta);
			div = delta.Length();

			if (div >= 32)
			{
				pnew = CL_AllocParticleFast();
			}
		}
		else
		{
			pnew = CL_AllocParticleFast();
		}
	}
	else
	{
		pbeam->startEntity = 0;
	}

	if (pnew)
	{
		VectorCopy(pbeam->source, pnew->org);
		pnew->die = engine_cl->time + pbeam->amplitude;
		VectorClear(pnew->vel);

		pnew->next = particles;
		pbeam->particles = pnew;
		particles = pnew;
	}

	// nothing to draw
	if (!particles)
		return;

	if (!pnew && div != 0)
	{
		VectorCopy(pbeam->source, delta);
		screenLast = gBSPRenderer.TriWorldToScreen(pbeam->source);
		screen = gBSPRenderer.TriWorldToScreen(particles->org);
	}
	else if (particles && particles->next)
	{
		VectorCopy(particles->org, delta);
		screenLast = gBSPRenderer.TriWorldToScreen(particles->org);
		screen = gBSPRenderer.TriWorldToScreen(particles->next->org);
		particles = particles->next;
	}
	else
	{
		return;
	}

	// UNDONE: This won't work, screen and screenLast must be extrapolated here to fix the
	// first beam segment for this trail

	// build world-space normal to screen-space direction vector
	VectorSubtract(screen, screenLast, tmp);
	// we don't need Z, we're in screen space
	tmp[2] = 0;
	VectorNormalize(tmp);

	// Build point along noraml line (normal is -y, x)
	VectorScale(gBSPRenderer.m_RefParams.up, tmp[0], normal); // Build point along normal line (normal is -y, x)
	VectorMA(normal, tmp[1], gBSPRenderer.m_RefParams.right, normal);

	// Make a wide line
	VectorMA(delta, pbeam->width, normal, last1);
	VectorMA(delta, -pbeam->width, normal, last2);

	div = 1.0f / pbeam->amplitude;
	fraction = (pbeam->die - engine_cl->time) * div;

	vLast = 0.0f;
	vStep = 1.0f;

	while (particles)
	{
		gEngfuncs.pTriAPI->Brightness(fraction);
		glTexCoord2f(1, 1);
		glVertex3fv(last2);
		gEngfuncs.pTriAPI->Brightness(fraction);
		glTexCoord2f(0, 1);
		glVertex3fv(last1);

		// Transform point into screen space
		screen = gBSPRenderer.TriWorldToScreen(particles->org);
		// Build world-space normal to screen-space direction vector
		VectorSubtract(screen, screenLast, tmp);

		// we don't need Z, we're in screen space
		tmp[2] = 0;
		VectorNormalize(tmp);
		VectorScale(gBSPRenderer.m_RefParams.up, tmp[0], normal); // Build point along noraml line (normal is -y, x)
		VectorMA(normal, tmp[1], gBSPRenderer.m_RefParams.right, normal);

		// Make a wide line
		VectorMA(particles->org, pbeam->width, normal, last1);
		VectorMA(particles->org, -pbeam->width, normal, last2);

		vLast += vStep; // Advance texture scroll (v axis only)

		if (particles->next != NULL)
		{
			fraction = (particles->die - engine_cl->time) * div;
		}
		else
		{
			fraction = 0.0;
		}

		gEngfuncs.pTriAPI->Brightness(fraction);
		glTexCoord2f(0, 0);
		glVertex3fv(last1);
		gEngfuncs.pTriAPI->Brightness(fraction);
		glTexCoord2f(1, 0);
		glVertex3fv(last2);

		VectorCopy(screen, screenLast);

		particles = particles->next;
	}

	// drift popcorn trail if there is a velocity
	particles = pbeam->particles;

	while (particles)
	{
		VectorMA(particles->org, frametime, particles->vel, particles->org);
		particles = particles->next;
	}
}

void CGoldSrc_BeamRenderer::CL_RunBeamLogic(BEAM* pBeam, int listIndex)
{
	// premanent beams never die automatically
	if (pBeam->flags & FBEAM_FOREVER)
		return;

	if (pBeam->type == TE_BEAMFOLLOW && pBeam->particles)
	{
		particle_t* particle = pBeam->particles;
		while (particle)
		{
			if (particle->die <= engine_cl->time)
			{
				particle_t* todelete = particle;
				pBeam->particles = particle = particle->next;
				Delete_particle(todelete);
			}
			else
			{
				particle_t* todelete = particle;
				particle = particle->next;
			}
		}
		return;
	}

	// other beams
	if (pBeam->die > engine_cl->time)
		return;

	m_BeamTempEnt_List.erase(m_BeamTempEnt_List.begin() + listIndex);
}