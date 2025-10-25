/*
Trinity Rendering Engine - Copyright Andrew Lucas 2009-2012

The Trinity Engine is free software, distributed in the hope th-
at it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU Lesser General Public License for more det-
ails.

Renderer base definitions and functions
Written by Andrew Lucas, Richard Rohac, BUzer, Laurie, Botman and Id Software
*/

#include "PlatformHeaders.h"
#include "STDIO.H"
#include "STDLIB.H"
#include "MATH.H"

#include "hud.h"
#include "cl_util.h"
#include "com_model.h"
#include <string.h>
#include "triangleapi.h"
#include "event_api.h"

#include "shake.h"

#include "rendererdefs.h"
#include "bsprenderer.h"
#include "particle_engine.h"
#include "propmanager.h"
#include "watershader.h"
#include "mirrormanager.h"

#include "postprocess.h"
#include "blur.h"
#include "goldsrc_beamrenderer.h"
#include "goldsrc_tracerrenderer.h"

#include "exports.h"

#include "StudioModelRenderer.h"

#include "goldsrc_spriterenderer.h"

#include "opengl_utils/GL_FBO.h"
#include "opengl_utils/GL_Buffers.h"
#include "opengl_utils/GL_ShaderProgram.h"
#include "opengl_utils/GL_DebugInterface.h"
#include "opengl_utils/GL_ShadowMap.h"
#include "opengl_utils/GL_StateHandler.h"

#include "BSPModel_Gen.h"


#ifndef BOX_ON_PLANE_SIDE
#define BOX_ON_PLANE_SIDE(emins, emaxs, p)                                                                 \
	(((p)->type < 3) ? (                                                                                   \
						   ((p)->dist <= (emins)[(p)->type]) ? 1                                           \
															 : (                                           \
																   ((p)->dist >= (emaxs)[(p)->type]) ? 2   \
																									 : 3)) \
					 : BoxOnPlaneSide((emins), (emaxs), (p)))
#endif

Vector g_vecFull(1.0f, 1.0f, 1.0f); // color of 3d attenuation texture

glstate_t g_savedGLState;

extern int g_iFlashLight;

int r_visframecount;
clientmleaf_t* r_oldviewleaf;

model_t* cl_sprite_dot;
model_t* cl_sprite_lightning;
model_t* cl_sprite_white;
model_t* cl_sprite_glow;
model_t* cl_sprite_muzzleflash[3];
model_t* cl_sprite_ricochet;
model_t* cl_sprite_shell;

extern std::vector<std::unique_ptr<TEMPENTITY>> gpTempEnts;

double sqrt(double x);

//==========================
//	stristr
//
//==========================
char* stristr(const char* string, const char* string2)
{
	int c, len;
	c = tolower(*string2);
	len = strlen(string2);

	while (string)
	{
		for (; *string && tolower(*string) != c; string++)
			;
		if (*string)
		{
			if (strnicmp(string, string2, len) == 0)
			{
				break;
			}
			string++;
		}
		else
		{
			return NULL;
		}
	}
	return (char*)string;
}

//==========================
//	strLower
//
//==========================
char* strLower(char* str)
{
	char* temp;

	for (temp = str; *temp; temp++)
		*temp = tolower(*temp);

	return str;
}

//==========================
// FilenameFromPath
//
//==========================
void FilenameFromPath(const char* szin, char* szout)
{
	int lastdot = 0;
	int lastbar = 0;
	int pathlength = 0;

	for (int i = 0; i < (int)strlen(szin); i++)
	{
		if (szin[i] == '/' || szin[i] == '\\')
			lastbar = i + 1;

		if (szin[i] == '.')
			lastdot = i;
	}

	for (int i = lastbar; i < (int)strlen(szin); i++)
	{
		if (i == lastdot)
			break;

		szout[pathlength] = szin[i];
		pathlength++;
	}
	szout[pathlength] = 0;
}

#define shuffle(a, b, c) (((a) << 4) | ((b) << 2) | ((c)))

inline void DotProductSub(float* result, Vector* v0, Vector* v1, float* subval)
{
	auto mmPos = _mm_loadu_ps((const float*)v0);
	auto mmPlane = _mm_loadu_ps((const float*)v1);
	auto result_ = _mm_sub_ps(_mm_dp_ps(mmPos, mmPlane, 0b01110111), _mm_load_ps1(subval));
	*result = _mm_cvtss_f32(result_);
}

/*
====================
VectorIRotate

====================
*/
void VectorIRotate(const Vector& in1, const matrix3x4_t &in2, Vector& out)
{
	out[0] = in1[0] * in2[0][0] + in1[1] * in2[1][0] + in1[2] * in2[2][0];
	out[1] = in1[0] * in2[0][1] + in1[1] * in2[1][1] + in1[2] * in2[2][1];
	out[2] = in1[0] * in2[0][2] + in1[1] * in2[1][2] + in1[2] * in2[2][2];
}

/*
====================
VectorRotate

====================
*/
void VectorRotate(const Vector& in1, const matrix3x4_t &in2, Vector& out)
{
	out[0] = in1[0] * in2[0][0] + in1[1] * in2[0][1] + in1[2] * in2[0][2];
	out[1] = in1[0] * in2[1][0] + in1[1] * in2[1][1] + in1[2] * in2[1][2];
	out[2] = in1[0] * in2[2][0] + in1[1] * in2[2][1] + in1[2] * in2[2][2];

}

/*
====================
VectorRotate

====================
*/
void VectorRotateAbs(const Vector& in1, const matrix3x4_t& in2, Vector& out)
{
	out[0] = fabs(in1[0] * in2[0][0]) + fabs(in1[1] * in2[0][1]) + fabs(in1[2] * in2[0][2]);
	out[1] = fabs(in1[0] * in2[1][0]) + fabs(in1[1] * in2[1][1]) + fabs(in1[2] * in2[1][2]);
	out[2] = fabs(in1[0] * in2[2][0]) + fabs(in1[1] * in2[2][1]) + fabs(in1[2] * in2[2][2]);
}

//==========================
//	ClampColor
//
//==========================
void ClampColor(int r, int g, int b, color24* out)
{
	if (r < 0)
		r = 0;
	if (g < 0)
		g = 0;
	if (b < 0)
		b = 0;

	if (r > 255)
		r = 255;
	if (g > 255)
		g = 255;
	if (b > 255)
		b = 255;

	out->r = (unsigned char)r;
	out->g = (unsigned char)g;
	out->b = (unsigned char)b;
}

//==========================
//	IsEntityMoved
//
//==========================
int IsEntityMoved(cl_entity_t* e)
{
	//need a better way to identify ents with scrolling textures without the use of flags

	//auto psurf = &engine_cl->worldmodel->surfaces[e->model->firstmodelsurface];
	//for (int i = 0; i < e->model->nummodelsurfaces; i++, psurf++)
	//{
	//	if (stristr(psurf->texinfo->texture->name, "scroll"))
	//	{
	//		return true;
	//	}
	//}

	if (e->angles[0] || e->angles[1] || e->angles[2] ||
		e->origin[0] || e->origin[1] || e->origin[2] ||
		e->curstate.frame > 0)
		return TRUE;
	else
		return FALSE;
}

//==========================
//	IsEntityTransparent
//
//==========================
int IsEntityTransparent(cl_entity_t* e)
{
	if (!e)
		return FALSE;

	if (e->curstate.rendermode == kRenderNormal ||
		e->curstate.rendermode == kRenderTransAlpha)
		return FALSE;
	else
		return TRUE;
}

//==========================
//	IsPitchReversed
//
//==========================
int IsPitchReversed(float pitch)
{
	int quadrant = int(pitch / 90) % 4;
	if ((quadrant == 1) || (quadrant == 2))
		return -1;

	return 1;
}


//==========================
//	MOD_PointInLeaf
//
//==========================
clientmleaf_t* Mod_PointInLeaf(Vector p)
{
	clientmnode_t* node;
	float d;
	mplane_t* plane;

	node = BSPWorld_Model::m_pWorldNodes;
	while (1)
	{
		if (node->contents < 0)
			return (clientmleaf_t*)node;
		plane = node->plane;
		d = DotProduct (p,plane->normal) - plane->dist;
		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}

	return NULL; // never reached
}
int BoxOnPlaneSide(Vector emins, Vector emaxs, mplane_t* p)
{
	float dist1, dist2;
	int sides;

	// fast axial cases
	if (p->type < 3)
	{
		if (p->dist <= emins[p->type])
			return 1;
		if (p->dist >= emaxs[p->type])
			return 2;
		return 3;
	}

	// general case
	switch (p->signbits)
	{
	case 0:
		dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
		dist2 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
		break;
	case 1:
		dist1 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
		dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
		break;
	case 2:
		dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
		dist2 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
		break;
	case 3:
		dist1 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
		dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
		break;
	case 4:
		dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
		dist2 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
		break;
	case 5:
		dist1 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
		dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
		break;
	case 6:
		dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
		dist2 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
		break;
	case 7:
		dist1 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
		dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
		break;
	default:
		dist1 = dist2 = 0;		// shut up compiler
		assert( 0 );
		break;
	}

	sides = 0;
	if (dist1 >= p->dist)
		sides = 1;
	if (dist2 < p->dist)
		sides |= 2;
	return sides;
}
/*
===============
SV_FindTouchedLeafs

===============
*/
void SV_FindTouchedLeafs(entextradata_t* ent, clientmnode_t* node)
{
	mplane_t* splitplane;
	clientmleaf_t* leaf;
	int sides;
	int leafnum;

	if (node->contents == CONTENTS_SOLID)
		return;

	// add an efrag if the node is a leaf
	if (node->contents < 0)
	{
		if (ent->num_leafs == MAX_ENT_LEAFS)
			return;

		leaf = (clientmleaf_t*)node;
		leafnum = leaf - BSPWorld_Model::m_pWorldLeafs - 1;

		ent->leafnums[ent->num_leafs] = leafnum;
		ent->num_leafs++;
		return;
	}

	// NODE_MIXED

	splitplane = node->plane;
	sides = BoxOnPlaneSide(ent->absmin, ent->absmax, splitplane);

	// recurse down the contacted sides
	if (sides & 1)
		SV_FindTouchedLeafs(ent, node->children[0]);

	if (sides & 2)
		SV_FindTouchedLeafs(ent, node->children[1]);
}

/*
===================
Mod_DecompressVis
===================
*/

byte* Mod_DecompressVis(byte* in)
{
	static byte decompressed[MAX_MAP_LEAFS / 8];
	int c;
	byte* out;
	int row;

	row = (BSPWorld_Model::m_iNumWorldLeafs + 7) >> 3;
	out = decompressed;

	if (!in)
	{ // no vis info, so make all visible
		while (row)
		{
			*out++ = 0xff;
			row--;
		}
		return decompressed;
	}

	do
	{
		if (*in)
		{
			*out++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		while (c)
		{
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);

	return decompressed;
}

byte* Mod_LeafPVS(clientmleaf_t* leaf)
{
	if (leaf == BSPWorld_Model::m_pWorldLeafs)
		return Mod_DecompressVis(NULL);

	return Mod_DecompressVis(leaf->compressed_vis);
}

/*
===============
R_MarkLeaves
===============
*/
void R_MarkLeaves(clientmleaf_t* pLeaf)
{
	static cl_entity_t* worldent = gEngfuncs.GetEntityByIndex(1);
	static byte solid[4096];
	static cvar_t* r_novis = gEngfuncs.pfnGetCvarPointer("r_novis");
	if (r_oldviewleaf == pLeaf)
	{
		if (r_novis->value == 0)
			return;

		r_visframecount++;

		goto label;
	}
	else
	{
		r_oldviewleaf = pLeaf;
		r_visframecount++;
		if (r_novis->value == 0)
		{
			// Get current vis data
			gBSPRenderer.m_pPVS = Mod_LeafPVS(pLeaf);
		}
		else
		{
		label:
			memset(solid, 255, (BSPWorld_Model::m_iNumWorldLeafs + 7) >> 3);
			gBSPRenderer.m_pPVS = solid;
		}
	}

	if (BSPWorld_Model::m_iNumWorldLeafs > 0)
	{
		for (int i = 0; i < BSPWorld_Model::m_iNumWorldLeafs; i++)
		{
			if (CHECKVISBIT(gBSPRenderer.m_pPVS, i))
			{
				clientmnode_t* node = (clientmnode_t*)&BSPWorld_Model::m_pWorldLeafs[i + 1];
				do
				{
					if (node->visframe == r_visframecount)
						break;

					node->visframe = r_visframecount;
					node = node->parent;
				} while (node);
			}
		}
	}
}

byte* ResizeArray(byte* pOriginal, int iSize, int iCount)
{
	byte* pArray = new byte[iSize * (iCount + 1)];
	memset(pArray, 0, sizeof(byte) * iSize * (iCount + 1));

	if (pOriginal && iCount)
	{
		memmove(pArray, pOriginal, iSize * iCount);
		delete[] pOriginal;
	}

	return pArray;
}


char* UTIL_VarArgs_client(const char* format, ...)
{
	va_list argptr;
	static char string[1024];

	va_start(argptr, format);
	vsprintf(string, format, argptr);
	va_end(argptr);

	return string;
}

/*
=================
HUD_PrintSpeeds

=================
*/
void HUD_PrintSpeeds(void)
{
	if (!gBSPRenderer.m_pCvarSpeeds->value)
		return;

	static float flLastTime;
	float flCurTime = engine_cl->time;
	float flFrameTime = flCurTime - flLastTime;
	flLastTime = flCurTime;

	// prevent divide by zero
	if (flFrameTime <= 0)
		flFrameTime = 1;

	if (flFrameTime > 1)
		flFrameTime = 1;

	int iFPS = 1 / flFrameTime;

	gHUD.DrawHudString(50, 20, 0, UTIL_VarArgs_client("FPS: %d\n", iFPS), 255, 255, 255);
	gHUD.DrawHudString(50, 50, 0, UTIL_VarArgs_client("%s took %f ms\n", WATER_PASS_TIME, gWaterShader.m_fRenderTime), 255, 255, 255);
	gHUD.DrawHudString(70, 70, 0, UTIL_VarArgs_client("water passes made: %d\n", gWaterShader.m_iNumPasses), 255, 255, 255);
	gHUD.DrawHudString(50, 100, 0, UTIL_VarArgs_client("%s took %f ms\n", MIRROR_PASS_TIME, gMirrorManager.m_fRenderTime), 255, 255, 255);
	gHUD.DrawHudString(70, 120, 0, UTIL_VarArgs_client("mirror passes made: %d\n", gMirrorManager.m_iNumPasses), 255, 255, 255);
	gHUD.DrawHudString(50, 150, 0, UTIL_VarArgs_client("%s took %f ms\n", SHADOWMAP_PASS_TIME, gBSPRenderer.m_fShadowGenerationTime), 255, 255, 255);
	gHUD.DrawHudString(70, 170, 0, UTIL_VarArgs_client("shadow passes made: %d\n", gBSPRenderer.m_iNumTotalShadows), 255, 255, 255);
	gHUD.DrawHudString(50, 200, 0, UTIL_VarArgs_client("%s took %f ms\n", MAINWORLDSCENE_PASS_TIME, gBSPRenderer.m_fMainWorldRenderTime), 255, 255, 255);
	gHUD.DrawHudString(70, 220, 0, UTIL_VarArgs_client("num brush verts drawn: %d\n", gBSPRenderer.m_iBSPVertsCounter), 255, 255, 255);
	gHUD.DrawHudString(50, 250, 0, UTIL_VarArgs_client("%s took %f ms\n", STUDIOMDL_PASS_TIME, g_StudioRenderer.m_fStudioMDLRenderTime), 255, 255, 255);
	gHUD.DrawHudString(70, 270, 0, UTIL_VarArgs_client("num studiomdl polys drawn: %d\n", gBSPRenderer.m_iStudioPolyCounter), 255, 255, 255);
}


ref_params_t* r_refdef; // the refdef param pointer lasts until the end of v_renderview. so reset it on hud_drawtransparenttriangles

int restore_numleafs = 0;

void R_SetupView(ref_params_t* pparams)
{
	gBSPRenderer.RendererRefDef(pparams);
	g_StudioRenderer.StudioPreFrame(pparams);
}

/*
=================
R_CalcRefDef

=================
*/
void R_CalcRefDef(ref_params_t* pparams)
{
	if (gEngfuncs.pfnGetCvarFloat("r_norefresh") != 0.0)
		return;

	if (restore_numleafs)
		engine_cl->worldmodel->numleafs = restore_numleafs;

	pparams->onlyClientDraw = 1;
	r_refdef = pparams;

	// all entities have been set up by goldsrc, get them
	gBSPRenderer.GetRenderEnts();

	SetupFlashlight(engine_cl->viewent.origin + Vector(0, 0, 8), engine_cl->viewent.angles * Vector(-1, 1, 1), engine_cl->time, gHUD.m_flTimeDelta);


	// Set up pre-frame stuff
	gBSPRenderer.SetupPreFrame(pparams);

	g_StudioRenderer.StudioPreFrame(pparams);

	// Set up basic rendering
	gBSPRenderer.RendererRefDef(pparams);

	if (!restore_numleafs)
		restore_numleafs = engine_cl->worldmodel->numleafs;

	engine_cl->worldmodel->numleafs = 0;
}

extern cvar_t* cl_first_person_uses_world_model;

void R_DrawMultiViews()
{
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(glm::value_ptr(gBSPRenderer.m_ViewMatrix * gBSPRenderer.m_ModelMatrix));

	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(glm::value_ptr(gBSPRenderer.m_ProjectionMatrix));

	// Render water shader perspectives
	gWaterShader.DrawWaterPasses(&gBSPRenderer.m_RefParams);

	// Render mirror perspectives
	gMirrorManager.DrawMirrorPasses(&gBSPRenderer.m_RefParams);

	// Render shadow maps into depth images
	gBSPRenderer.Make_ShadowMaps();
}

void R_DrawMainView()
{
	glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf(glm::value_ptr(gBSPRenderer.m_ViewMatrix * gBSPRenderer.m_ModelMatrix));
	
	glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(glm::value_ptr(gBSPRenderer.m_ProjectionMatrix));
	
	gBSPRenderer.m_bMainPass = true;
	
	// Render world
	gBSPRenderer.DrawNormalTriangles();
	
	// Render props on the list
	gPropManager.RenderProps();
	
	// Render any cables
	gPropManager.DrawCables();
	
	gBSPRenderer.m_bMainPass = false;
	
	// Render water shader
	gWaterShader.DrawWater();
	
	// Draw mirrors
	gMirrorManager.DrawMirrors();
	
	// draw studio models and whatnot
	
	gBSPRenderer.m_bMainPass = true;

	g_StudioRenderer.StudioDrawModels();

	// Render transparent entities
	gBSPRenderer.DrawTransparentTriangles();

	g_LegacySpriteRenderer.DrawSpriteEntities();

	// Render our own goldsrc beams
	g_BeamRenderer.R_DrawBeams(engine_cl->time - engine_cl->oldtime);

	// Render our own goldsrc tracers
	g_TracerRenderer.CL_DrawTracers();

	// Render particles
	gParticleEngine.DrawParticles();

	g_StudioRenderer.StudioDrawViewmodel();

	gBSPRenderer.m_bMainPass = false;

	g_BeamRenderer.NewFrame();
}

void DrawQuadDebugTest()
{
	if (!gBSPRenderer.m_pSunShadowMap)
		return;

	gBSPRenderer.m_FilterShader->Bind();
	gBSPRenderer.m_FilterShader->Uniform1i(gBSPRenderer.m_FilterShader->GetUniformLoc("gaussian_pass"), 0);
	gBSPRenderer.m_pScreenQuadVAO->BindVAO();

	gBSPRenderer.BindGLTexture(GL_TEXTURE0, gBSPRenderer.m_iEngineLightmapIndex);

	g_GlobalGLState.SetBlend(false);
	g_GlobalGLState.SetCullFace(false);
	g_GlobalGLState.SetDepthTest(false);

	glDrawArrays(GL_TRIANGLES, gBSPRenderer.quad_TopRight, 6);

	GL_ShaderProgram::ResetShaderBind();
	GL_BufferHandler::ResetBufferBinding(GL_BufferHandler::ArrayBuffer);
}

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

	// GL_DisableMultitexture();
	g_GlobalGLState.SetBlend(true);
	g_GlobalGLState.SetCullFace(false);
	g_GlobalGLState.SetDepthTest(false);
	glDisable(GL_TEXTURE_2D);
	if ((engine_cl->sf.fadeFlags & FFADE_MODULATE) != 0)
	{
		g_GlobalGLState.SetBlendFunc(GL_ZERO, GL_SRC_COLOR);
		color[3] = -1;
		color[0] = color[1] = color[2] = (alpha * (engine_cl->sf.fader - 255) - 511) >> 8;
	}
	else
	{
		g_GlobalGLState.SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
	g_GlobalGLState.SetDepthTest(true);
	g_GlobalGLState.SetCullFace(true);
	glEnable(GL_TEXTURE_2D);
}

extern cvar_t* cl_first_person_uses_world_model;

/*
=================
R_DrawNormalTriangles

=================
*/
void R_DrawNormalTriangles(void)
{
	g_StudioRenderer.m_fStudioMDLRenderTime = 0;

	R_DrawMultiViews(); //shadowmaps, water povs, mirrors, etc
	
	R_DrawMainView();

	//just for debugging certain textures
	//DrawQuadDebugTest();

	// Restore fog params
	gWaterShader.Restore();

	GL_ShaderProgram::ResetShaderBind();
	GL_VertexArrayObject::ResetVAOBinding();
	GL_BufferHandler::ResetBufferBinding(GL_BufferHandler::ArrayBuffer);

	R_PolyBlend(); // restore goldsrc's ugly screen fade code
}

void RenderersDumpInfo(void)
{
	gEngfuncs.Con_Printf("Engine Data Dump:\n");
	gEngfuncs.Con_Printf("Number of models in engine cache: %i of 512 max.\n", engine_cl->model_precache_count);
	gEngfuncs.Con_Printf("Number of entities in PVS: %i.\n", engine_cl->num_entities);
	gEngfuncs.Con_Printf("Number of tga textures cached: %i on client.\n", gTextureLoader.m_pTextures.size());
	gEngfuncs.Con_Printf("Number of lightmaps: %i of 64 max.\n", gBSPRenderer.m_iNumLightmaps);
	gEngfuncs.Con_Printf("Number of surfaces: %i.\n", gBSPRenderer.m_iTotalFaceCount);
	gEngfuncs.Con_Printf("Number of triangles: %i.\n", gBSPRenderer.m_iTotalTriCount);
	gEngfuncs.Con_Printf("Number of vertexes: %i.\n", gBSPRenderer.m_iTotalVertCount);
	gEngfuncs.Con_Printf("Number of client side entities: %i.\n", gPropManager.m_pEntities.size());
	gEngfuncs.Con_Printf("Number of detail textures: %i.\n", gBSPRenderer.m_iNumDetailTextures);
	gEngfuncs.Con_Printf("Number of OpenGL Buffers: %i.\n", GL_BufferHandler::GetNumBuffers());
	gEngfuncs.Con_Printf("Approximated total size of buffers in kb: %f.\n", GL_BufferHandler::GetTotalMemorySize() * 0.001);
	gEngfuncs.Con_Printf("Number of OpenGL FrameBuffers: %i.\n", GL_FBOHandler::GetNumFrameBuffers());
	gEngfuncs.Con_Printf("Number of OpenGL RenderBuffers: %i.\n\n\n", GL_RBOHandler::GetNumRenderBuffers());

	const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));

	if (stristr(vendor, "nvidia"))
	{
		//NOT TESTED
		int dedicated_vidmem, total_available_mem, current_available_mem, eviction_count, evicted_mem;
		glGetIntegerv(GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &dedicated_vidmem);
		glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, &total_available_mem);
		glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, &current_available_mem);
		glGetIntegerv(GL_GPU_MEMORY_INFO_EVICTION_COUNT_NVX, &eviction_count);
		glGetIntegerv(GL_GPU_MEMORY_INFO_EVICTED_MEMORY_NVX, &evicted_mem);

		gEngfuncs.Con_Printf("OpenGL meminfo: dedicated video memory: %i kb\n", dedicated_vidmem);
		gEngfuncs.Con_Printf("OpenGL meminfo: total available memory: %i kb\n", total_available_mem);
		gEngfuncs.Con_Printf("OpenGL meminfo: current available memory: %i kb\n\n", current_available_mem);

		gEngfuncs.Con_Printf("OpenGL meminfo: eviction count: %i\n", eviction_count);
		gEngfuncs.Con_Printf("OpenGL meminfo: evicted memory: %i kb\n", evicted_mem);
	}
	else if (stristr(vendor, "amd") || stristr(vendor, "ati"))
	{
		int params[4];
		int params1[4];
		int params2[4];
		glGetIntegerv(GL_VBO_FREE_MEMORY_ATI, params);
		glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, params1);
		glGetIntegerv(GL_RENDERBUFFER_FREE_MEMORY_ATI, params2);

		gEngfuncs.Con_Printf("OpenGL VBO: total memory free in the pool: %i kb\n", params[0]);
		gEngfuncs.Con_Printf("OpenGL VBO: largest available free block in the pool: %i kb\n", params[1]);
		gEngfuncs.Con_Printf("OpenGL VBO: total auxiliary memory free: %i kb\n", params[2]);
		gEngfuncs.Con_Printf("OpenGL VBO: largest auxiliary free block: %i kb\n\n\n", params[3]);

		gEngfuncs.Con_Printf("OpenGL Texture: total memory free in the pool: %i kb\n", params1[0]);
		gEngfuncs.Con_Printf("OpenGL Texture: largest available free block in the pool: %i kb\n", params1[1]);
		gEngfuncs.Con_Printf("OpenGL Texture: total auxiliary memory free: %i kb\n", params1[2]);
		gEngfuncs.Con_Printf("OpenGL Texture: largest auxiliary free block: %i kb\n\n\n", params1[3]);

		gEngfuncs.Con_Printf("OpenGL RenderBuffer: total memory free in the pool: %i kb\n", params2[0]);
		gEngfuncs.Con_Printf("OpenGL RenderBuffer: largest available free block in the pool: %i kb\n", params2[1]);
		gEngfuncs.Con_Printf("OpenGL RenderBuffer: total auxiliary memory free: %i kb\n", params2[2]);
		gEngfuncs.Con_Printf("OpenGL RenderBuffer: largest auxiliary free block: %i kb\n", params2[3]);
	}
}


//===============================
// buz: flashlight managenemt
//===============================
void SetupFlashlight(Vector origin, Vector angles, float time, float frametime)
{
	pmtrace_t tr;
	Vector fwd, right, up;

	static float add = 0;
	float addideal = 0;

	static float mult = 0;
	mult = lerp(mult, (float)g_iFlashLight, gHUD.m_flTimeDelta * 10);

	if (mult <= 0.001)
		return;

	AngleVectors(angles, &fwd, &right, &up);

	fwd = origin + (fwd * 150);

	EV_SetTraceHull(2);
	gEngfuncs.pEventAPI->EV_PlayerTrace(origin, fwd, PM_WORLD_ONLY, -1, &tr);

	if (tr.fraction < 1.0)
		addideal = (1 - tr.fraction) * 30;

	float speed = (add - addideal) * 10;

	if (speed < 0)
		speed *= -1;

	if (add < addideal)
	{
		add += frametime * speed;

		if (add > addideal)
			add = addideal;
	}
	else if (add > addideal)
	{
		add -= frametime * speed;

		if (add < addideal)
			add = addideal;
	}

	cl_dlight_t* flashlight = gBSPRenderer.CL_AllocDLight(-666);
	flashlight->origin = origin + (up * 8) + (right * 10);
	flashlight->radius = 700;
	flashlight->die = time + 0.01;
	flashlight->cone_size = 50 + add;
	flashlight->color.x = 1.0 * mult;
	flashlight->color.y = 1.0 * mult;
	flashlight->color.z = 1.0 * mult;
	flashlight->textureindex = gBSPRenderer.m_pFlashlightTextures[0]->iIndex;
	flashlight->frustum.SetFrustum(angles, flashlight->origin, flashlight->cone_size * 1.2, 700);
	flashlight->justspawned = true;
	flashlight->flags |= LIGHT_CASTSHADOWS;
	auto sm_res = gBSPRenderer.m_pCvarFlashLightDepthRes->value;
	if (flashlight->depth)
		GL_ShadowMap::DeAllocateShadowMap(flashlight->depth);

	flashlight->depth = GL_ShadowMap::AllocateShadowMap(GL_ShadowMap::_2DTexture_Storage, GL_RG16F, sm_res, sm_res, 0, GL_RG, GL_FLOAT);
	VectorCopy(angles, flashlight->angles);
}
float Q_rsqrt(float number)
{
	long i;
	float x2, y;
	const float threehalfs = 1.5F;

	x2 = number * 0.5F;
	y = number;
	i = *(long*)&y;			   // evil floating point bit level hacking
	i = 0x5f3759df - (i >> 1); // what the fuck?
	y = *(float*)&i;
	y = y * (threehalfs - (x2 * y * y)); // 1st iteration

	return y;
}

// Detect litte/big endian
byte bLittleEndian[2] = {1, 0};
unsigned short ByteToUShort(byte* byte)
{
	if (*(short*)bLittleEndian == 1)
		return (byte[0] + (byte[1] << 8));
	else
		return (byte[1] + (byte[0] << 8));
}

unsigned int ByteToUInt(byte* byte)
{
	unsigned int iValue = byte[0];
	iValue += (byte[1] << 8);
	iValue += (byte[2] << 16);
	iValue += (byte[3] << 24);

	return iValue;
}

int ByteToInt(byte* byte)
{
	int iValue = byte[0];
	iValue += (byte[1] << 8);
	iValue += (byte[2] << 16);
	iValue += (byte[3] << 24);

	return iValue;
}

/*
=================
R_Init

=================
*/
void R_Init(void)
{
	cl_sprite_dot = IEngineStudio.Mod_ForName("sprites/dot.spr", true);
	cl_sprite_lightning = IEngineStudio.Mod_ForName("sprites/lgtning.spr", true);
	cl_sprite_white = IEngineStudio.Mod_ForName("sprites/white.spr", true);
	cl_sprite_glow = IEngineStudio.Mod_ForName("sprites/animglow01.spr", true);
	cl_sprite_ricochet = IEngineStudio.Mod_ForName("sprites/richo1.spr", true);
	cl_sprite_shell = IEngineStudio.Mod_ForName("sprites/shellchrome.spr", true);

	glewInit();

	g_IGLDebug.Initialize();

	gpTempEnts.clear();

	gPropManager.Init();
	gTextureLoader.Init();
	gBSPRenderer.Init();
	gParticleEngine.Init();
	gWaterShader.Init();
	gMirrorManager.Init();
	g_LegacySpriteRenderer.Init();
	gPostProcess.Init();
	gHUD.gBloomRenderer.Init();
	gBlur.InitScreen();
}

extern viewinfo_s g_viewinfo;

/*
=================
R_VidInit

=================
*/
void R_VidInit(void)
{
	restore_numleafs = 0;

	GLint mainfbo;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &mainfbo);

	GL_FBOHandler::SetMainGameFBO(mainfbo);

	if (mainfbo < 0)
		mainfbo = 0;

	gpTempEnts.clear();

	memset(&g_viewinfo, 0, sizeof(g_viewinfo));

	gTextureLoader.VidInit();
	gWaterShader.VidInit();
	gBSPRenderer.VidInit();
	gParticleEngine.VidInit();
	gMirrorManager.VidInit();
	g_StudioRenderer.VidInit();
	gPropManager.VidInit();
	g_LegacySpriteRenderer.VidInit();
	g_TracerRenderer.CL_ClearTracerList();
	g_BeamRenderer.Reset();

	gBlur.VidInit();
	gPostProcess.Reset();
}

/*
=================
R_Shutdown

=================
*/
void R_Shutdown(void)
{
	gTextureLoader.Shutdown();
	gBSPRenderer.Shutdown();
	gPropManager.Shutdown();
	gWaterShader.Shutdown();
	gParticleEngine.Shutdown();
}