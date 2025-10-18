#include "PlatformHeaders.h"
#include "hud.h"
#include "cl_util.h"
#include "triangleapi.h"

#include "bsprenderer.h"
#include "StudioModelRenderer.h"

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <math.h>
#include <vector>

#include "goldsrc_tracerrenderer.h"
#include "opengl_utils/GL_StateHandler.h"
#include "opengl_utils/GL_ShaderProgram.h"

extern model_t* cl_sprite_dot;

// particle ramps
static int ramp1[8] = {0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61};
static int ramp2[8] = {0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66};
static int ramp3[6] = {0x6d, 0x6b, 6, 5, 4, 3};
static int gSparkRamp[9] = {0xfe, 0xfd, 0xfc, 0x6f, 0x6e, 0x6d, 0x6c, 0x67, 0x60};

static float gTracerSize[11] = {1.5f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
static color24 gTracerColors[] =
	{
		{255, 255, 255}, // White
		{255, 0, 0},	 // Red
		{0, 255, 0},	 // Green
		{0, 0, 255},	 // Blue
		{0, 0, 0},		 // Tracer default, filled in from cvars, etc.
		{255, 167, 17},	 // Yellow-orange sparks
		{255, 130, 90},	 // Yellowish streaks (garg)
		{55, 60, 144},	 // Blue egon streak
		{255, 130, 90},	 // More Yellowish streaks (garg)
		{255, 140, 90},	 // More Yellowish streaks (garg)
		{200, 130, 90},	 // More red streaks (garg)
		{255, 120, 70},	 // Darker red streaks (garg)
};

CGoldSrc_TracerRenderer g_TracerRenderer;

particle_t* CGoldSrc_TracerRenderer::AllocateTracer(const Vector org, const Vector vel, float life)
{
	auto& ptr = m_TracerTempEnt_List.emplace_back(std::make_unique<particle_t>());
	auto tracer_ptr = ptr.get();
	tracer_ptr->org = org;
	tracer_ptr->vel = vel;
	tracer_ptr->die = engine_cl->time + life;

	return tracer_ptr;
}

/*
================
CL_CullTracer

check tracer bbox
================
*/
bool CGoldSrc_TracerRenderer::CL_CullTracer(particle_t* p, const Vector start, const Vector end)
{
	Vector mins, maxs;
	int i;

	// compute the bounding box
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
		{
			maxs[i] += gTracerSize[p->type] * 2.0f;
		}
	}

	// check bbox
	return gHUD.viewFrustum.CullBox(mins, maxs);
}

void CGoldSrc_TracerRenderer::CL_DrawTracers()
{
	float scale, atten, gravity;
	Vector screenLast, screen;
	Vector start, end, delta;

	float frametime = engine_cl->time - engine_cl->oldtime;

	if (m_TracerTempEnt_List.empty())
		return; // nothing to draw?

	if (!gEngfuncs.pTriAPI->SpriteTexture(cl_sprite_dot, 0))
		return;

	GL_ShaderProgram::ResetShaderBind();

	static cvar_t* tracerred = gEngfuncs.pfnGetCvarPointer("tracerred");
	static cvar_t* tracergreen = gEngfuncs.pfnGetCvarPointer("tracergreen");
	static cvar_t* tracerblue = gEngfuncs.pfnGetCvarPointer("tracerblue");
	static cvar_t* traceralpha = gEngfuncs.pfnGetCvarPointer("tracerblue");

	color24* customColors = &gTracerColors[4];
	customColors->r = (byte)(tracerred->value * traceralpha->value * 255);
	customColors->g = (byte)(tracergreen->value * traceralpha->value * 255);
	customColors->b = (byte)(tracerblue->value * traceralpha->value * 255);

	g_GlobalGLState.SetBlend(true);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PRIMARY_COLOR);
	g_GlobalGLState.SetBlendFunc(GL_SRC_ALPHA, GL_ONE);
	g_GlobalGLState.SetAlphaTest(false);
	g_GlobalGLState.SetDepthWrite(false);

	gravity = frametime * gEngfuncs.pfnGetCvarFloat("sv_gravity"); // tr.movevars->gravity;
	scale = 1.0 - (frametime * 0.9);
	if (scale < 0.0f)
		scale = 0.0f;

	glBegin(GL_QUADS);

	for (auto &p_ : m_TracerTempEnt_List)
	{
		particle_t* p = p_.get();
		atten = (p->die - engine_cl->time);
		if (atten > 0.1f)
			atten = 0.1f;

		VectorScale(p->vel, (p->ramp * atten), delta);
		VectorAdd(p->org, delta, end);
		VectorCopy(p->org, start);

		if (!CL_CullTracer(p, start, end))
		{
			Vector verts[4], tmp2;
			Vector tmp, normal;
			color24 color;

			// Transform point into screen space
			screen = gBSPRenderer.TriWorldToScreen(start);
			screenLast = gBSPRenderer.TriWorldToScreen(end);

			// build world-space normal to screen-space direction vector
			VectorSubtract(screen, screenLast, tmp);

			// we don't need Z, we're in screen space
			tmp[2] = 0;
			VectorNormalize(tmp);

			// build point along noraml line (normal is -y, x)
			VectorScale(gBSPRenderer.m_RefParams.up, tmp[0] * gTracerSize[p->type], normal);
			VectorScale(gBSPRenderer.m_RefParams.right, -tmp[1] * gTracerSize[p->type], tmp2);
			VectorSubtract(normal, tmp2, normal);

			// compute four vertexes
			VectorSubtract(start, normal, verts[0]);
			VectorAdd(start, normal, verts[1]);
			VectorAdd(verts[0], delta, verts[2]);
			VectorAdd(verts[1], delta, verts[3]);

			if (p->color < 0 || p->color >= sizeof(gTracerColors) / sizeof(gTracerColors[0]))
			{
				p->color = TRACER_COLORINDEX_DEFAULT;
			}

			color = gTracerColors[p->color];
			glColor4ub(color.r, color.g, color.b, p->packedColor);

			glTexCoord2f(0.0f, 0.8f);
			glVertex3fv(verts[2]);
			glTexCoord2f(1.0f, 0.8f);
			glVertex3fv(verts[3]);
			glTexCoord2f(1.0f, 0.0f);
			glVertex3fv(verts[1]);
			glTexCoord2f(0.0f, 0.0f);
			glVertex3fv(verts[0]);
		}

		// evaluate position
		VectorMA(p->org, frametime, p->vel, p->org);

		if (p->type == pt_grav)
		{
			p->vel[0] *= scale;
			p->vel[1] *= scale;
			p->vel[2] -= gravity;

			p->packedColor = 255 * (p->die - engine_cl->time) * 2;
			if (p->packedColor > 255)
				p->packedColor = 255;
		}
		else if (p->type == pt_slowgrav)
		{
			p->vel[2] = gravity * 0.05f;
		}
	}
	glEnd();

	CL_RunTracerLogic();

	g_GlobalGLState.SetBlend(false);
	g_GlobalGLState.SetBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	g_GlobalGLState.SetAlphaTest(false);
	g_GlobalGLState.SetDepthWrite(true);
}

void CGoldSrc_TracerRenderer::CL_RunTracerLogic()
{
	float frametime = engine_cl->time - engine_cl->oldtime;

	if (!frametime)
		return;

	float time3 = 15.0f * frametime;
	float time2 = 10.0f * frametime;
	float time1 = 5.0f * frametime;
	float dvel = 4.0f * frametime;
	float grav = frametime * gEngfuncs.pfnGetCvarFloat("sv_gravity") * 0.05f;

	for (int i = m_TracerTempEnt_List.size() - 1; i >= 0;)
	{
		auto p = m_TracerTempEnt_List[i].get();

		if (p->die < engine_cl->time)
		{
			m_TracerTempEnt_List.erase(m_TracerTempEnt_List.begin() + i);
			i--;
			continue;
		}
		i--;

		if (p->type != pt_clientcustom)
		{
			// update position.
			VectorMA(p->org, frametime, p->vel, p->org);
		}

		switch (p->type)
		{
		case pt_static:
			break;
		case pt_fire:
			p->ramp += time1;
			if (p->ramp >= 6.0f)
				p->die = -1.0f;
			else
				p->color = ramp3[(int)p->ramp];
			p->vel[2] += grav;
			break;
		case pt_explode:
			p->ramp += time2;
			if (p->ramp >= 8.0f)
				p->die = -1.0f;
			else
				p->color = ramp1[(int)p->ramp];
			VectorMA(p->vel, dvel, p->vel, p->vel);
			p->vel[2] -= grav;
			break;
		case pt_explode2:
			p->ramp += time3;
			if (p->ramp >= 8.0f)
				p->die = -1.0f;
			else
				p->color = ramp2[(int)p->ramp];
			VectorMA(p->vel, -frametime, p->vel, p->vel);
			p->vel[2] -= grav;
			break;
		case pt_blob:
			if (p->packedColor == 255)
			{
				// normal blob explosion
				VectorMA(p->vel, dvel, p->vel, p->vel);
				p->vel[2] -= grav;
				break;
			}
			// intentionally fallthrough
		case pt_blob2:
			if (p->packedColor == 255)
			{
				// normal blob explosion
				p->vel[0] -= p->vel[0] * dvel;
				p->vel[1] -= p->vel[1] * dvel;
				p->vel[2] -= grav;
			}
			else
			{
				p->ramp += time2;
				if (p->ramp >= 9.0f)
					p->ramp = 0.0f;
				p->color = gSparkRamp[(int)p->ramp];
				VectorMA(p->vel, -frametime * 0.5f, p->vel, p->vel);
				p->type = gEngfuncs.pfnRandomLong(0, 3) ? pt_blob : pt_blob2;
				p->vel[2] -= grav * 5.0f;
			}
			break;
		case pt_grav:
			p->vel[2] -= grav * 20.0f;
			break;
		case pt_slowgrav:
			p->vel[2] -= grav;
			break;
		case pt_vox_grav:
			p->vel[2] -= grav * 8.0f;
			break;
		case pt_vox_slowgrav:
			p->vel[2] -= grav * 4.0f;
			break;
		case pt_clientcustom:
			if (p->callback)
				p->callback(p, frametime);
			break;
		}
	}
}