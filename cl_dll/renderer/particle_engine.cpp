/*
Trinity Rendering Engine - Copyright Andrew Lucas 2009-2012
Spirinity Rendering Engine - Copyright FranticDreamer 2020-2021

The Trinity Engine is free software, distributed in the hope th-
at it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU Lesser General Public License for more det-
ails.

Particle Engine
Written by Andrew Lucas
*/

#include <cstdlib>
#include <cmath>
#include "hud.h"
#include "cl_util.h"

#include "const.h"
#include "studio.h"
#include "entity_state.h"
#include "triangleapi.h"
#include "event_api.h"
#include "pm_defs.h"

#include <string.h>
#include <memory.h>

#include "propmanager.h"
#include "particle_engine.h"
#include "bsprenderer.h"

#include "r_efx.h"
#include "r_studioint.h"
#include "studio_util.h"
#include "event_api.h"
#include "event_args.h"

#include "StudioModelRenderer.h"
#include "opengl_utils/GL_ShaderProgram.h"
#include "opengl_utils/GL_StateHandler.h"
#include "opengl_utils/GL_Mesh.h"
#include "goldsrc_spriterenderer.h"

CParticleEngine gParticleEngine;


//===========================================
// GLSL SHADER START
//
//===========================================
#include "glshaders/particle_glsl.h"

//===========================================
// GLSL SHADER END
//
//===========================================

GL_BEGIN_ATTRIBLIST(ParticleVertex)
GL_DEFINE_ATTRIB(GL_ShaderProgram::ShaderAttribs::VertexPos, 3, GL_FLOAT, pos)
GL_DEFINE_ATTRIB(GL_ShaderProgram::ShaderAttribs::TexCoord, 2, GL_FLOAT, uv)
GL_DEFINE_NORMALIZEDATTRIB(GL_ShaderProgram::ShaderAttribs::Color, 4, GL_UNSIGNED_BYTE, color)
GL_END_ATTRIBLIST()

static GL_Mesh* s_pParticleBuffer;
static GL_ShaderProgram s_ParticleShader(glsl_particle_vp, glsl_particle_fp);

/*
====================
Init

====================
*/
void CParticleEngine::Init() 
{
	m_pCvarDrawParticles = gEngfuncs.pfnRegisterVariable("r_particles", "1", FCVAR_ARCHIVE);
	m_pCvarParticleDebug = gEngfuncs.pfnRegisterVariable("r_particles_debug", "0", 0);
	m_pCvarGravity = gEngfuncs.pfnGetCvarPointer("sv_gravity");

	s_ParticleShader.Bind();
	s_ParticleShader.Uniform1i(s_ParticleShader.GetUniformLoc("texture0"), 0);

	s_pParticleBuffer = new GL_Mesh(4 * 100000, ParticleVertex::GetAttribLayout(), false);
	s_pParticleBuffer->SetDrawMode(GL_QUADS);
	
	GL_ShaderProgram::ResetShaderBind();
};

/*
====================
Shutdown

====================
*/
void CParticleEngine::Shutdown(void)
{
	VidInit();
}

/*
====================
VidInit

====================
*/
void CParticleEngine::VidInit()
{
	if (m_pSystemHeader)
	{
		particle_system_t* next = m_pSystemHeader;
		while (next)
		{
			particle_system_t* pfree = next;
			next = pfree->next;

			cl_particle_t* pparticle = pfree->particleheader;
			while (pparticle)
			{
				cl_particle_t* pfreeparticle = pparticle;
				pparticle = pfreeparticle->next;

				m_iNumFreedParticles++;
				delete[] pfreeparticle;
			}

			m_iNumFreedSystems++;
			delete[] pfree;
		}

		m_pSystemHeader = nullptr;
	}
};

/*
====================
AllocSystem

====================
*/
particle_system_t* CParticleEngine::AllocSystem()
{
	// Allocate memory
	particle_system_t* pSystem = new particle_system_t;
	memset(pSystem, 0, sizeof(particle_system_t));

	// Add system into pointer array
	if (m_pSystemHeader)
	{
		m_pSystemHeader->prev = pSystem;
		pSystem->next = m_pSystemHeader;
	}

	m_iNumCreatedSystems++;
	m_pSystemHeader = pSystem;
	return pSystem;
}

/*
====================
AllocParticle

====================
*/
cl_particle_t* CParticleEngine::AllocParticle(particle_system_t* pSystem)
{
	// Allocate memory
	cl_particle_t* pParticle = new cl_particle_t;
	memset(pParticle, 0, sizeof(cl_particle_t));

	// Add system into pointer array
	if (pSystem->particleheader)
	{
		pSystem->particleheader->prev = pParticle;
		pParticle->next = pSystem->particleheader;
	}

	m_iNumCreatedParticles++;
	pSystem->particleheader = pParticle;
	return pParticle;
}

/*
====================
CreateCluster

====================
*/
void CParticleEngine::CreateCluster(const char* szPath, Vector origin, Vector dir, int iId)
{
	char szFilePath[64];
	strcpy(szFilePath, "/scripts/particles/");
	strcat(szFilePath, szPath);

	std::vector<std::byte> pFile = FileSystem_LoadFileIntoBuffer(szFilePath, FileContentFormat::Text);

	if (pFile.empty())
	{
		gEngfuncs.Con_Printf("Could not load particle cluster file: %s!\n", szPath);
		return;
	}

	char* pToken = (char*)pFile.data();
	while (1)
	{
		char szField[256];

		pToken = gEngfuncs.COM_ParseFile(pToken, szField);

		if (!pToken)
			break;

		CreateSystem(szField, origin, dir, iId);
	}
}

/*
====================
CreateSystem

====================
*/
particle_system_t* CParticleEngine::CreateSystem(char* szPath, Vector origin, Vector dir, int iId, particle_system_t* parent)
{
	if (!strlen(szPath))
		return nullptr;

	char szFilePath[64];
 	strcpy(szFilePath, "/scripts/particles/");
	strcat(szFilePath, szPath);

	std::vector<std::byte> pFile = FileSystem_LoadFileIntoBuffer(szFilePath, FileContentFormat::Text);

	if (pFile.empty())
	{
		gEngfuncs.Con_Printf("Could not load particle definitions file: %s!\n", szPath);
		return nullptr;
	}

	particle_system_t* pSystem = AllocSystem();

	if (!pSystem)
	{
		gEngfuncs.Con_Printf("Warning! Exceeded max number of particle systems!\n");
		return nullptr;
	}

	// Fill in default values
	pSystem->id = iId;
	pSystem->mainalpha = 1;
	pSystem->spawntime = engine_cl->time;
	VectorCopy(dir, pSystem->dir);

	char* pToken = (char*)pFile.data();
	while (1)
	{
		char szField[32];
		pToken = gEngfuncs.COM_ParseFile(pToken, szField);

		if (!pToken)
			break;

		char szValue[32];
		pToken = gEngfuncs.COM_ParseFile(pToken, szValue);

		if (!pToken)
			break;

		if (!strcmp(szField, "systemshape"))
			pSystem->shapetype = static_cast<byte>(atoi(szValue));
		else if (!strcmp(szField, "minvel"))
			pSystem->minvel = atof(szValue);
		else if (!strcmp(szField, "maxvel"))
			pSystem->maxvel = atof(szValue);
		else if (!strcmp(szField, "maxofs"))
			pSystem->maxofs = atof(szValue);
		else if (!strcmp(szField, "fadein"))
			pSystem->fadeintime = atof(szValue);
		else if (!strcmp(szField, "fadedelay"))
			pSystem->fadeoutdelay = atof(szValue);
		else if (!strcmp(szField, "mainalpha"))
			pSystem->mainalpha = atof(szValue);
		else if (!strcmp(szField, "veldamp"))
			pSystem->velocitydamp = atof(szValue);
		else if (!strcmp(szField, "veldampdelay"))
			pSystem->veldampdelay = atof(szValue);
		else if (!strcmp(szField, "life"))
			pSystem->maxlife = atof(szValue);
		else if (!strcmp(szField, "lifevar"))
			pSystem->maxlifevar = atof(szValue);
		else if (!strcmp(szField, "pcolr"))
			pSystem->primarycolor.x = (float)atoi(szValue) / 255;
		else if (!strcmp(szField, "pcolg"))
			pSystem->primarycolor.y = (float)atoi(szValue) / 255;
		else if (!strcmp(szField, "pcolb"))
			pSystem->primarycolor.z = (float)atoi(szValue) / 255;
		else if (!strcmp(szField, "scolr"))
			pSystem->secondarycolor.x = (float)atoi(szValue) / 255;
		else if (!strcmp(szField, "scolg"))
			pSystem->secondarycolor.y = (float)atoi(szValue) / 255;
		else if (!strcmp(szField, "scolb"))
			pSystem->secondarycolor.z = (float)atoi(szValue) / 255;
		else if (!strcmp(szField, "ctransd"))
			pSystem->transitiondelay = atof(szValue);
		else if (!strcmp(szField, "ctranst"))
			pSystem->transitiontime = atof(szValue);
		else if (!strcmp(szField, "ctransv"))
			pSystem->transitionvar = atof(szValue);
		else if (!strcmp(szField, "scale"))
			pSystem->scale = atof(szValue);
		else if (!strcmp(szField, "scalevar"))
			pSystem->scalevar = atof(szValue);
		else if (!strcmp(szField, "scaledampdelay"))
			pSystem->scaledampdelay = atof(szValue);
		else if (!strcmp(szField, "scaledampfactor"))
			pSystem->scaledampfactor = atof(szValue);
		else if (!strcmp(szField, "gravity"))
			pSystem->gravity = atof(szValue);
		else if (!strcmp(szField, "systemsize"))
			pSystem->systemsize = atoi(szValue);
		else if (!strcmp(szField, "maxparticles"))
			pSystem->maxparticles = static_cast<unsigned short>(atoi(szValue));
		else if (!strcmp(szField, "intensity"))
			pSystem->particlefreq = atof(szValue);
		else if (!strcmp(szField, "startparticles"))
			pSystem->startparticles = static_cast<unsigned short>(atoi(szValue));
		else if (!strcmp(szField, "maxparticlevar"))
			pSystem->maxparticlevar = static_cast<unsigned short>(atoi(szValue));
		else if (!strcmp(szField, "lightmaps"))
			pSystem->lightcheck = static_cast<byte>(atoi(szValue));
		else if (!strcmp(szField, "collision"))
			pSystem->collision = static_cast<byte>(atoi(szValue));
		else if (!strcmp(szField, "colwater"))
			pSystem->colwater = static_cast<bool>(atoi(szValue));
		else if (!strcmp(szField, "rendermode"))
			pSystem->rendermode = static_cast<byte>(atoi(szValue));
		else if (!strcmp(szField, "display"))
			pSystem->displaytype = static_cast<byte>(atoi(szValue));
		else if (!strcmp(szField, "impactdamp"))
			pSystem->impactdamp = atof(szValue);
		else if (!strcmp(szField, "rotationvar"))
			pSystem->rotationvar = atof(szValue);
		else if (!strcmp(szField, "rotationvel"))
			pSystem->rotationvel = atof(szValue);
		else if (!strcmp(szField, "rotationdamp"))
			pSystem->rotationdamp = atof(szValue);
		else if (!strcmp(szField, "rotationdampdelay"))
			pSystem->rotationdampdelay = atof(szValue);
		else if (!strcmp(szField, "rotxvar"))
			pSystem->rotxvar = atof(szValue);
		else if (!strcmp(szField, "rotxvel"))
			pSystem->rotxvel = atof(szValue);
		else if (!strcmp(szField, "rotxdamp"))
			pSystem->rotxdamp = atof(szValue);
		else if (!strcmp(szField, "rotxdampdelay"))
			pSystem->rotxdampdelay = atof(szValue);
		else if (!strcmp(szField, "rotyvar"))
			pSystem->rotyvar = atof(szValue);
		else if (!strcmp(szField, "rotyvel"))
			pSystem->rotyvel = atof(szValue);
		else if (!strcmp(szField, "rotydamp"))
			pSystem->rotydamp = atof(szValue);
		else if (!strcmp(szField, "rotydampdelay"))
			pSystem->rotydampdelay = atof(szValue);
		else if (!strcmp(szField, "randomdir"))
			pSystem->randomdir = static_cast<bool>(atoi(szValue));
		else if (!strcmp(szField, "create"))
			strcpy(pSystem->create, szValue);
		else if (!strcmp(szField, "deathcreate"))
			strcpy(pSystem->deathcreate, szValue);
		else if (!strcmp(szField, "watercreate"))
			strcpy(pSystem->watercreate, szValue);
		else if (!strcmp(szField, "windx"))
			pSystem->windx = atof(szValue);
		else if (!strcmp(szField, "windy"))
			pSystem->windy = atof(szValue);
		else if (!strcmp(szField, "windvar"))
			pSystem->windvar = atof(szValue);
		else if (!strcmp(szField, "windtype"))
			pSystem->windtype = atoi(szValue);
		else if (!strcmp(szField, "windmult"))
			pSystem->windmult = atof(szValue);
		else if (!strcmp(szField, "windmultvar"))
			pSystem->windmultvar = atof(szValue);
		else if (!strcmp(szField, "stuckdie"))
			pSystem->stuckdie = atof(szValue);
		else if (!strcmp(szField, "maxheight"))
			pSystem->maxheight = atof(szValue);
		else if (!strcmp(szField, "tracerdist"))
			pSystem->tracerdist = atof(szValue);
		else if (!strcmp(szField, "fadedistnear"))
			pSystem->fadedistnear = atoi(szValue);
		else if (!strcmp(szField, "fadedistfar"))
			pSystem->fadedistfar = atoi(szValue);
		else if (!strcmp(szField, "numframes"))
			pSystem->numframes = static_cast<unsigned short>(atoi(szValue));
		else if (!strcmp(szField, "framesizex"))
			pSystem->framesizex = static_cast<unsigned short>(atoi(szValue));
		else if (!strcmp(szField, "framesizey"))
			pSystem->framesizey = static_cast<unsigned short>(atoi(szValue));
		else if (!strcmp(szField, "framerate"))
			pSystem->framerate = static_cast<unsigned short>(atoi(szValue));
		else if (!strcmp(szField, "texture"))
		{
			int iOriginalBind;
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &iOriginalBind);

			char szTexPath[256];
			strcpy(szTexPath, "gfx/textures/particles/");
			strcat(szTexPath, szValue);
			strcat(szTexPath, ".dds");

			pSystem->texture = gTextureLoader.LoadTexture(szTexPath);

			if (!pSystem->texture)
			{
				// Remove system
				if (pSystem->next)
				{
					m_pSystemHeader = pSystem->next;
					m_pSystemHeader->prev = nullptr;
				}
				delete[] pSystem;

				return nullptr;
			}

			glBindTexture(GL_TEXTURE_2D, pSystem->texture->iIndex);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, iOriginalBind);
		}
		else
			gEngfuncs.Con_Printf("Warning! Unknown field: %s\n", szField);
	}

	if (pSystem->shapetype != SYSTEM_SHAPE_PLANE_ABOVE_PLAYER)
	{
		if (!parent)
		{
			VectorCopy(origin, pSystem->origin);

			if(BSPWorld_Model::m_pWorldNodes)
				pSystem->leaf = Mod_PointInLeaf(pSystem->origin + Vector(0, 0, 8));
		}
		else
		{
			VectorCopy(origin + Vector(0, 0, 8), pSystem->origin);
			pSystem->leaf = parent->leaf;
		}
	}
	else
	{
		pmtrace_t tr;
		EV_SetTraceHull(2);
		gEngfuncs.pEventAPI->EV_PlayerTrace(origin, origin + Vector(0, 0, 160000), PM_STUDIO_IGNORE, -1, &tr);

		if (tr.fraction == 1.0)
		{
			// Remove system
			if (pSystem->next)
			{
				m_pSystemHeader = pSystem->next;
				m_pSystemHeader->prev = nullptr;
			}
			delete[] pSystem;

			return nullptr;
		}

		pSystem->skyheight = tr.endpos.z;
	}

	if (pSystem->collision != PARTICLE_COLLISION_DECAL)
	{
		if (pSystem->create[0] != 0)
			pSystem->createsystem = CreateSystem(pSystem->create, pSystem->origin, pSystem->dir, 0, pSystem);

		if (!pSystem->createsystem)
			memset(pSystem->create, 0, sizeof(pSystem->create));
	}

	if (pSystem->watercreate[0] != 0)
		pSystem->watersystem = CreateSystem(pSystem->watercreate, pSystem->origin, pSystem->dir, 0, pSystem);

	if (!pSystem->watersystem)
		memset(pSystem->watercreate, 0, sizeof(pSystem->watercreate));

	if (parent)
	{
		// Child systems cannot spawn on their own
		pSystem->parentsystem = parent;
		pSystem->maxparticles = NULL;
		pSystem->particlefreq = NULL;
	}
	else if (engine_cl->worldmodel)
	{
		if ((pSystem->shapetype != SYSTEM_SHAPE_PLANE_ABOVE_PLAYER) && (pSystem->shapetype != SYSTEM_SHAPE_BOX_AROUND_PLAYER))
		{
			// create all starting particles
			for (int i = 0; i < pSystem->startparticles; i++)
				CreateParticle(pSystem);
		}
		else
		{
			// Create particles at random heights
			EnvironmentCreateFirst(pSystem);
		}
	}

	return pSystem;
}

/*
====================
EnvironmentCreateFirst

====================
*/
void CParticleEngine::EnvironmentCreateFirst(particle_system_t* pSystem)
{
	Vector vOrigin;
	int iNumParticles = pSystem->particlefreq * 4;
	Vector vPlayer = gEngfuncs.GetLocalPlayer()->origin;

	// Spawn particles inbetween the view origin and maxheight
	for (int i = 0; i < iNumParticles; i++)
	{
		if (pSystem->shapetype == SYSTEM_SHAPE_PLANE_ABOVE_PLAYER)
		{
			vOrigin[0] = vPlayer[0] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);
			vOrigin[1] = vPlayer[1] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);

			if (pSystem->maxheight)
			{
				vOrigin[2] = vPlayer[2] + pSystem->maxheight;

				if (vOrigin[2] > pSystem->skyheight)
					vOrigin[2] = pSystem->skyheight;
			}
			else
			{
				vOrigin[2] = pSystem->skyheight;
			}

			vOrigin[2] = gEngfuncs.pfnRandomFloat(vPlayer[2], vOrigin[2]);

			pmtrace_t pmtrace;
			EV_SetTraceHull(2);
			gEngfuncs.pEventAPI->EV_PlayerTrace(vOrigin, Vector(vOrigin[0], vOrigin[1], pSystem->skyheight - 8), PM_STUDIO_IGNORE, -1, &pmtrace);

			if (pmtrace.allsolid || pmtrace.fraction != 1.0)
				continue;
		}
		else if (pSystem->shapetype == SYSTEM_SHAPE_BOX_AROUND_PLAYER)
		{
			if (gEngfuncs.GetLocalPlayer())
			{
				Vector vPlayer = gEngfuncs.GetLocalPlayer()->origin;
				Vector vSpeed = gBSPRenderer.m_RefParams.simvel;

				vOrigin[0] = vPlayer[0] + vSpeed[0] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);
				vOrigin[1] = vPlayer[1] + vSpeed[1] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);
				vOrigin[2] = vPlayer[2] + vSpeed[2] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);
			}
		}

		CreateParticle(pSystem, vOrigin);
	}
}

/*
====================
CreateParticle

====================
*/
void CParticleEngine::CreateParticle(particle_system_t* pSystem, float* flOrigin, float* flNormal)
{
	Vector vBaseOrigin;
	Vector vForward, vUp, vRight;

	Vector vecOrg;

	if (pSystem->collision == PARTICLE_COLLISION_RAIN)
	{
		pmtrace_t tr;
		Vector vPlayer = gEngfuncs.GetLocalPlayer()->origin;
		Vector vSpeed = gBSPRenderer.m_RefParams.simvel;

		vecOrg[0] = vPlayer[0] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);
		vecOrg[1] = vPlayer[1] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);

		if (pSystem->maxheight)
		{
			vecOrg = vPlayer[2] + pSystem->maxheight;

			if (vecOrg[2] > pSystem->skyheight)
				vecOrg[2] = pSystem->skyheight;
		}
		else
		{
			vecOrg[2] = pSystem->skyheight;
		}

		gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction(false, true);

		// Store off the old count
		EV_PushPMStates();

		EV_SetTraceHull(2);
		gEngfuncs.pEventAPI->EV_PlayerTrace(vecOrg, (vecOrg + Vector(0, 0, 8192)), PM_STUDIO_IGNORE, -1, &tr);

		EV_PopPMStates();
		
		if (gEngfuncs.PM_PointContents(tr.endpos, nullptr) != CONTENTS_SKY)
			return;

		const char* name = EV_TraceTexture(0, vecOrg, vecOrg + Vector(0, 0, 8192));

		if (!name)
			return;

		if (strcmp(name, "sky"))
			return;

		//perform a trace upwards and see if we hit the sky
		//msurface_t* surf = SurfaceAtPoint(engine_cl->worldmodel, engine_cl->worldmodel->nodes, tr.endpos, tr.endpos + Vector(0, 0, 1024));
		//if (!surf)
		//	return;
		
		//if (!(surf->flags & SURF_DRAWSKY))
		//	return;//didnt hit a sky
	}



	cl_particle_t* pParticle = AllocParticle(pSystem);

	if (!pParticle)
		return;

	pParticle->pSystem = pSystem;
	pParticle->spawntime = engine_cl->time;
	pParticle->frame = -1;

	if (pSystem->shapetype == SYSTEM_SHAPE_PLANE_ABOVE_PLAYER || pSystem->shapetype == SYSTEM_SHAPE_BOX_AROUND_PLAYER)
	{
		vForward[0] = 0;
		vForward[1] = 0;
		vForward[2] = -1;
	}
	else if (pSystem->randomdir)
	{
		vForward[0] = gEngfuncs.pfnRandomFloat(-1, 1);
		vForward[1] = gEngfuncs.pfnRandomFloat(-1, 1);
		vForward[2] = gEngfuncs.pfnRandomFloat(-1, 1);
	}
	else if (flOrigin && flNormal)
	{
		vForward[0] = flNormal[0];
		vForward[1] = flNormal[1];
		vForward[2] = flNormal[2];
	}
	else
	{
		vForward[0] = pSystem->dir[0];
		vForward[1] = pSystem->dir[1];
		vForward[2] = pSystem->dir[2];
	}

	if (flNormal)
	{
		pParticle->normal[0] = flNormal[0];
		pParticle->normal[1] = flNormal[1];
		pParticle->normal[2] = flNormal[2];
	}
	else
	{
		pParticle->normal[0] = pSystem->dir[0];
		pParticle->normal[1] = pSystem->dir[1];
		pParticle->normal[2] = pSystem->dir[2];
	}

	VectorClear(vUp);
	VectorClear(vRight);

	if (vForward != Vector(0, 0, 0))
	{
		gBSPRenderer.GetUpRight(vForward, vUp, vRight);
		VectorMA(pParticle->velocity, gEngfuncs.pfnRandomFloat(pSystem->minvel, pSystem->maxvel), vForward, pParticle->velocity);
		VectorMA(pParticle->velocity, gEngfuncs.pfnRandomFloat(-pSystem->maxofs, pSystem->maxofs), vRight, pParticle->velocity);
		VectorMA(pParticle->velocity, gEngfuncs.pfnRandomFloat(-pSystem->maxofs, pSystem->maxofs), vUp, pParticle->velocity);
	}
	else
		pParticle->velocity = Vector(0, 0, 0);

	if (pSystem->maxlife == -1)
		pParticle->life = pSystem->maxlife;
	else
		pParticle->life = engine_cl->time + pSystem->maxlife + gEngfuncs.pfnRandomFloat(-pSystem->maxlifevar, pSystem->maxlifevar);

	pParticle->scale = pSystem->scale + gEngfuncs.pfnRandomFloat(-pSystem->scalevar, pSystem->scalevar);
	pParticle->rotationvel = pSystem->rotationvel + gEngfuncs.pfnRandomFloat(-pSystem->rotationvar, pSystem->rotationvar);
	pParticle->rotxvel = pSystem->rotxvel + gEngfuncs.pfnRandomFloat(-pSystem->rotxvar, pSystem->rotxvar);
	pParticle->rotyvel = pSystem->rotyvel + gEngfuncs.pfnRandomFloat(-pSystem->rotyvar, pSystem->rotyvar);

	if (flOrigin)
	{
		VectorCopy(flOrigin, vBaseOrigin);

		if (flNormal)
			VectorMA(vBaseOrigin, 0.1, Vector(flNormal[0], flNormal[1], flNormal[2]), vBaseOrigin);
	}
	else
	{
		VectorCopy(pSystem->origin, vBaseOrigin);
	}

	if (pSystem->shapetype == SYSTEM_SHAPE_POINT)
	{
		VectorCopy(vBaseOrigin, pParticle->origin);
	}
	else if (pSystem->shapetype == SYSTEM_SHAPE_BOX)
	{
		pParticle->origin[0] = vBaseOrigin[0] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);
		pParticle->origin[1] = vBaseOrigin[1] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);
		pParticle->origin[2] = vBaseOrigin[2] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);
	}
	else if (pSystem->shapetype == SYSTEM_SHAPE_BOX_AROUND_PLAYER)
	{
		Vector vPlayer = gEngfuncs.GetLocalPlayer()->origin;
		Vector vSpeed = gBSPRenderer.m_RefParams.simvel;
		pParticle->origin[0] = vPlayer[0] + vSpeed[0] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);
		pParticle->origin[1] = vPlayer[1] + vSpeed[1] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);
		pParticle->origin[2] = vPlayer[2] + vSpeed[2] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);

		//gEngfuncs.Con_Printf("idk if this works \n");
	}
	else if (pSystem->shapetype == SYSTEM_SHAPE_PLANE_ABOVE_PLAYER)
	{
		if (!flOrigin)
		{
			Vector vPlayer = gEngfuncs.GetLocalPlayer()->origin;
			pParticle->origin[0] = vPlayer[0] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);
			pParticle->origin[1] = vPlayer[1] + gEngfuncs.pfnRandomLong(-pSystem->systemsize, pSystem->systemsize);

			if (pSystem->maxheight)
			{
				pParticle->origin[2] = vPlayer[2] + pSystem->maxheight;

				if (pParticle->origin[2] > pSystem->skyheight)
					pParticle->origin[2] = pSystem->skyheight;
			}
			else
			{
				pParticle->origin[2] = pSystem->skyheight;
			}
		}
		else
		{
			VectorCopy(flOrigin, pParticle->origin);
		}
	}

	if (pParticle->rotationvel)
		pParticle->rotation = gEngfuncs.pfnRandomFloat(0, 360);

	if (pParticle->rotxvel)
		pParticle->rotx = gEngfuncs.pfnRandomFloat(0, 360);

	if (pParticle->rotyvel)
		pParticle->roty = gEngfuncs.pfnRandomFloat(0, 360);

	if (!pSystem->fadeintime)
		pParticle->alpha = 1;

	if (pSystem->fadeoutdelay)
		pParticle->fadeoutdelay = pSystem->fadeoutdelay;

	if (pSystem->scaledampdelay)
		pParticle->scaledampdelay = engine_cl->time + pSystem->scaledampdelay + gEngfuncs.pfnRandomFloat(-pSystem->scalevar, pSystem->scalevar);

	if (pSystem->transitiondelay && pSystem->transitiontime)
	{
		pParticle->secondarydelay = engine_cl->time + pSystem->transitiondelay + gEngfuncs.pfnRandomFloat(-pSystem->transitionvar, pSystem->transitionvar);
		pParticle->secondarytime = pSystem->transitiontime + gEngfuncs.pfnRandomFloat(-pSystem->transitionvar, pSystem->transitionvar);
	}

	if (pSystem->windtype)
	{
		pParticle->windmult = pSystem->windmult + gEngfuncs.pfnRandomFloat(-pSystem->windmultvar, pSystem->windmultvar);
		pParticle->windxvel = pSystem->windx + gEngfuncs.pfnRandomFloat(-pSystem->windvar, pSystem->windvar);
		pParticle->windyvel = pSystem->windy + gEngfuncs.pfnRandomFloat(-pSystem->windvar, pSystem->windvar);
	}

	if (!pSystem->numframes)
	{
		pParticle->texcoords[0][0] = 0;
		pParticle->texcoords[0][1] = 0;
		pParticle->texcoords[1][0] = 1;
		pParticle->texcoords[1][1] = 0;
		pParticle->texcoords[2][0] = 1;
		pParticle->texcoords[2][1] = 1;
		pParticle->texcoords[3][0] = 0;
		pParticle->texcoords[3][1] = 1;
	}
	else
	{
		// Calculate these only once
		float flFractionWidth = (float)pSystem->framesizex / (float)pSystem->texture->iWidth;
		float flFractionHeight = (float)pSystem->framesizey / (float)pSystem->texture->iHeight;

		// Calculate top left coordinate
		pParticle->texcoords[0][0] = flFractionWidth;
		pParticle->texcoords[0][1] = 0;

		// Calculate top right coordinate
		pParticle->texcoords[1][0] = 0;
		pParticle->texcoords[1][1] = 0;

		// Calculate bottom right coordinate
		pParticle->texcoords[2][0] = 0;
		pParticle->texcoords[2][1] = flFractionHeight;

		// Calculate bottom left coordinate
		pParticle->texcoords[3][0] = flFractionWidth;
		pParticle->texcoords[3][1] = flFractionHeight;
	}

	VectorCopy(pSystem->primarycolor, pParticle->color);
	VectorCopy(pSystem->secondarycolor, pParticle->scolor);
	VectorCopy(pParticle->origin, pParticle->lastspawn);

	for (int i = 0; i < 3; i++)
	{
		if (pParticle->scolor[i] == -1)
			pParticle->scolor[i] = gEngfuncs.pfnRandomFloat(0, 1);
	}

	if (pSystem->lightcheck == PARTICLE_LIGHTCHECK_NONE)
	{
		pParticle->color = pSystem->primarycolor;
		return;
	}

	if (pSystem->lightcheck == PARTICLE_LIGHTCHECK_NORMAL)
	{
		pParticle->color = LightForParticle(pParticle);
	}
	else if (pSystem->lightcheck == PARTICLE_LIGHTCHECK_SCOLOR)
	{
		pParticle->scolor = LightForParticle(pParticle);
	}
	else if (pSystem->lightcheck == PARTICLE_LIGHTCHECK_MIXP)
	{
		pParticle->color = LightForParticle(pParticle);
		pParticle->color.x = pParticle->color.x * pSystem->primarycolor.x;
		pParticle->color.y = pParticle->color.y * pSystem->primarycolor.y;
		pParticle->color.z = pParticle->color.z * pSystem->primarycolor.z;
	}
}

/*
====================
Update

====================
*/
void CParticleEngine::Update()
{
	// moved to imgui_manager.cpp
	/*
	if(m_pCvarParticleDebug->value)
	{
		gEngfuncs.Con_Printf("Created Particles: %i, Freed Particles %i, Active Particles: %i\nCreated Systems: %i, Freed Systems: %i, Active Systems: %i\n\n",
			m_iNumCreatedParticles, m_iNumFreedParticles,m_iNumCreatedParticles-m_iNumFreedParticles, m_iNumCreatedSystems, m_iNumFreedSystems, m_iNumCreatedSystems-m_iNumFreedSystems);
	}
	*/

	if (!m_pCvarDrawParticles->value)
		return;

	m_flFrameTime = engine_cl->time - m_flLastDraw;
	m_flLastDraw = engine_cl->time;

	if (m_flFrameTime > 1)
		m_flFrameTime = 1;

	if (m_flFrameTime <= 0)
		return;

	// No systems to check on
	if (!m_pSystemHeader)
		return;

	UpdateSystems();

	// Update all particles
	particle_system_t* psystem = m_pSystemHeader;
	while (psystem)
	{
		cl_particle_t* pparticle = psystem->particleheader;
		while (pparticle)
		{
			if (!UpdateParticle(pparticle))
			{
				cl_particle_t* pfree = pparticle;
				pparticle = pfree->next;

				if (!pfree->prev)
				{
					psystem->particleheader = pparticle;
					if (pparticle)
						pparticle->prev = nullptr;
				}
				else
				{
					pfree->prev->next = pparticle;
					if (pparticle)
						pparticle->prev = pfree->prev;
				}

				m_iNumFreedParticles++;
				delete[] pfree;
				continue;
			}
			cl_particle_t* pnext = pparticle->next;
			pparticle = pnext;
		}

		particle_system_t* pnext = psystem->next;
		psystem = pnext;
	}
}

/*
====================
UpdateSystems

====================
*/
void CParticleEngine::UpdateSystems()
{
	float flTime = engine_cl->time;

	// check if any systems are available for removal
	particle_system_t* next = m_pSystemHeader;
	while (next)
	{
		if (next->maxparticles != 0)
		{
			particle_system_t* pnext = next->next;
			next = pnext;
			continue;
		}

		if (next->parentsystem)
		{
			particle_system_t* pnext = next->next;
			next = pnext;
			continue;
		}

		// Has related particles
		if (next->particleheader)
		{
			particle_system_t* pnext = next->next;
			next = pnext;
			continue;
		}

		// Unparent these and let the engine handle them
		if (next->createsystem)
			next->createsystem->parentsystem = nullptr;

		if (next->watersystem)
			next->watersystem->parentsystem = nullptr;

		particle_system_t* pfree = next;
		next = pfree->next;

		if (!pfree->prev)
		{
			m_pSystemHeader = next;
			if (next)
				next->prev = nullptr;
		}
		else
		{
			pfree->prev->next = next;
			if (next)
				next->prev = pfree->prev;
		}

		// Delete from memory
		m_iNumFreedSystems++;
		delete[] pfree;
	}

	// Update systems
	next = m_pSystemHeader;
	while (next)
	{
		// Parented systems cannot spawn particles themselves
		if (next->parentsystem)
		{
			particle_system_t* pnext = next->next;
			next = pnext;
			continue;
		}

		float flLife = engine_cl->time - next->spawntime;
		float flFreq = 1 / (float)next->particlefreq;
		int iTimesSpawn = flLife / flFreq;

		if (iTimesSpawn <= next->numspawns)
		{
			particle_system_t* pnext = next->next;
			next = pnext;
			continue;
		}

		int iNumSpawn = iTimesSpawn - next->numspawns;

		// cap if finite
		if (next->maxparticles != -1)
		{
			if (next->maxparticles < iNumSpawn)
				iNumSpawn = next->maxparticles;
		}

		if (next->maxparticlevar)
		{
			// Calculate variation
			int iNewAmount = iNumSpawn + abs((sin(flTime) / 2.4492) * next->maxparticlevar);

			// Create new particles
			for (int j = 0; j < iNewAmount; j++)
				CreateParticle(next);

			// Add to counter
			next->numspawns += iNumSpawn;

			// don't take off for infinite systems
			if (next->maxparticles != -1)
				next->maxparticles -= iNumSpawn;
		}
		else
		{
			// Create new particles
			for (int j = 0; j < iNumSpawn; j++)
				CreateParticle(next);

			// Add to counter
			next->numspawns += iNumSpawn;

			// don't take off for infinite systems
			if (next->maxparticles != -1)
				next->maxparticles -= iNumSpawn;
		}

		particle_system_t* pnext = next->next;
		next = pnext;
	}
}

/*
====================
CheckLightBBox

====================
*/
bool CParticleEngine::CheckLightBBox(cl_particle_t* pParticle, cl_dlight_t* pLight)
{
	if (pParticle->origin[0] > (pLight->origin[0] - pLight->radius) && pParticle->origin[1] > (pLight->origin[1] - pLight->radius) && pParticle->origin[2] > (pLight->origin[2] - pLight->radius) && pParticle->origin[0] < (pLight->origin[0] + pLight->radius) && pParticle->origin[1] < (pLight->origin[1] + pLight->radius) && pParticle->origin[2] < (pLight->origin[2] + pLight->radius))
		return false;

	return true;
}

/*
====================
LightForParticle

====================
*/
Vector CParticleEngine::LightForParticle(cl_particle_t* pParticle)
{
	float flRad;
	float flDist;
	float flAtten;
	float flCos;

	Vector vDir;
	Vector vNorm;
	Vector vForward;

	float flTime = engine_cl->time;
	Vector vEndPos = pParticle->origin - Vector(0, 0, 8964);
	Vector vColor = Vector(0, 0, 0);

	if(BSPWorld_Model::m_pWorldNodes)
		g_StudioRenderer.StudioRecursiveLightPoint(nullptr, BSPWorld_Model::m_pWorldNodes, pParticle->origin, vEndPos, vColor);

	for (auto& pLight : gBSPRenderer.m_pDynLights)
	{
		if (pLight->die < flTime || !pLight->radius)
			continue;

		if (pLight->cone_size)
		{
			if (pLight->frustum.CullBox(pParticle->origin, pParticle->origin))
				continue;

			Vector vAngles = pLight->angles;
			AngleVectors(vAngles, &vForward, nullptr, nullptr);
		}
		else
		{
			if (CheckLightBBox(pParticle, pLight.get()))
				continue;
		}

		flRad = pLight->radius * pLight->radius;
		vDir = pParticle->origin - pLight->origin;
		flDist = DotProduct(vDir, vDir);
		flAtten = (flDist / flRad - 1) * -1;

		if (pLight->cone_size)
		{
			vDir = vDir.Normalize();
			flCos = cos((pLight->cone_size * 2) * 0.3 * (M_PI2 / 360));
			flDist = DotProduct(Vector(flDist, flDist, flDist), vForward);

			if (flDist < 0 || flDist < flCos)
				continue;

			flAtten *= (flDist - flCos) / (1.0 - flCos);
		}

		if (flAtten <= 0)
			continue;

		VectorMA(vColor, flAtten, pLight->color, vColor);
	}
	return vColor;
}

/*
====================
UpdateParticle

====================
*/
bool CParticleEngine::UpdateParticle(cl_particle_t* pParticle)
{
	pmtrace_t pmtrace;
	bool bColWater = false;

	float flTime = engine_cl->time;
	Vector vFinalVelocity = pParticle->velocity;
	particle_system_t* pSystem = pParticle->pSystem;

	//
	// Check if the particle is ready to die
	//
	if (pParticle->life != -1)
	{
		if (pParticle->life <= flTime)
		{
			if (pSystem->deathcreate[0] != 0)
				CreateSystem(pSystem->deathcreate, pParticle->origin, pParticle->velocity.Normalize(), 0);

			return false; // remove
		}
	}

	//
	// Damp velocity
	//
	if (pSystem->velocitydamp && (pParticle->spawntime + pSystem->veldampdelay) < flTime)
		VectorScale(vFinalVelocity, (1.0 - pSystem->velocitydamp * m_flFrameTime), vFinalVelocity);

	//
	// Add gravity before collision test
	//
	vFinalVelocity.z -= m_pCvarGravity->value * pSystem->gravity * m_flFrameTime;

	//
	// Add in wind on either axes
	//
	if (pSystem->windtype)
	{
		if (pParticle->windxvel)
		{
			if (pSystem->windtype == PARTICLE_WIND_LINEAR)
				vFinalVelocity.x += pParticle->windxvel * m_flFrameTime;
			else
				vFinalVelocity.x += sin((flTime * pParticle->windmult)) * pParticle->windxvel * m_flFrameTime;
		}
		if (pParticle->windyvel)
		{
			if (pSystem->windtype == PARTICLE_WIND_LINEAR)
				vFinalVelocity.y += pParticle->windyvel * m_flFrameTime;
			else
				vFinalVelocity.y += sin((flTime * pParticle->windmult)) * pParticle->windyvel * m_flFrameTime;
		}
	}

	//
	// Calculate rotation on all axes
	//
	if (pSystem->rotationvel)
	{
		if (pSystem->rotationdamp && pParticle->rotationvel)
		{
			if ((pSystem->rotationdampdelay + pParticle->spawntime) < flTime)
				pParticle->rotationvel = pParticle->rotationvel * (1.0 - pSystem->rotationdamp);
		}

		pParticle->rotation += pParticle->rotationvel * m_flFrameTime;

		if (pParticle->rotation < 0)
			pParticle->rotation += 360;
		if (pParticle->rotation > 360)
			pParticle->rotation -= 360;
	}
	if (pSystem->rotxvel)
	{
		if (pSystem->rotxdamp && pParticle->rotxvel)
		{
			if ((pSystem->rotxdampdelay + pParticle->spawntime) < flTime)
				pParticle->rotxvel = pParticle->rotxvel * (1.0 - pSystem->rotxdamp);
		}

		pParticle->rotx += pParticle->rotxvel * m_flFrameTime;

		if (pParticle->rotx < 0)
			pParticle->rotx += 360;
		if (pParticle->rotx > 360)
			pParticle->rotx -= 360;
	}
	if (pSystem->rotyvel)
	{
		if (pSystem->rotydamp && pParticle->rotyvel)
		{
			if ((pSystem->rotydampdelay + pParticle->spawntime) < flTime)
				pParticle->rotyvel = pParticle->rotyvel * (1.0 - pSystem->rotydamp);
		}

		pParticle->roty += pParticle->rotyvel * m_flFrameTime;

		if (pParticle->roty < 0)
			pParticle->roty += 360;
		if (pParticle->roty > 360)
			pParticle->roty -= 360;
	}

	//
	// Collision detection
	//
	if (pSystem->collision)
	{
		EV_SetTraceHull(2);
		gEngfuncs.pEventAPI->EV_PlayerTrace(pParticle->origin, (pParticle->origin + vFinalVelocity * m_flFrameTime), PM_WORLD_ONLY, -1, &pmtrace);

		if (pmtrace.allsolid)
			return false; // Probably spawned inside a solid

		if (pSystem->colwater)
		{
			if (gEngfuncs.PM_PointContents(pParticle->origin + vFinalVelocity * m_flFrameTime, nullptr) == CONTENTS_WATER)
			{
				pmtrace.endpos = pParticle->origin + vFinalVelocity * m_flFrameTime;
				int iEntity = gEngfuncs.PM_WaterEntity(pParticle->origin + vFinalVelocity * m_flFrameTime);

				if (iEntity)
				{
					cl_entity_t* pEntity = gEngfuncs.GetEntityByIndex(iEntity);
					pmtrace.endpos.z = pEntity->model->maxs.z + 0.001;
				}

				pmtrace.plane.normal = Vector(0, 0, 1);
				pmtrace.fraction = 0;
				bColWater = true;
			}
		}

		if (pmtrace.fraction != 1.0)
		{
			if (pSystem->collision == PARTICLE_COLLISION_STUCK)
			{
				if (gEngfuncs.PM_PointContents(pmtrace.endpos, nullptr) == CONTENTS_SKY)
					return false;

				if (pParticle->life == -1 && pSystem->stuckdie)
				{
					pParticle->life = engine_cl->time + pSystem->stuckdie;
					pParticle->fadeoutdelay = engine_cl->time - pParticle->spawntime;
				}
				VectorMA(pParticle->origin, pmtrace.fraction * m_flFrameTime, vFinalVelocity, pParticle->origin);

				pParticle->rotationvel = NULL;
				pParticle->rotxvel = NULL;
				pParticle->rotyvel = NULL;

				VectorClear(pParticle->velocity);
				VectorClear(vFinalVelocity);
			}
			else if (pSystem->collision == PARTICLE_COLLISION_BOUNCE)
			{
				float fProj = DotProduct(vFinalVelocity, pmtrace.plane.normal);

				VectorMA(vFinalVelocity, -fProj * 2, pmtrace.plane.normal, pParticle->velocity);
				VectorScale(pParticle->velocity, pSystem->impactdamp, pParticle->velocity);
				VectorScale(vFinalVelocity, pmtrace.fraction, vFinalVelocity);

				if (pParticle->rotationvel)
					pParticle->rotationvel *= -fProj * 2 * pSystem->impactdamp * m_flFrameTime;

				if (pParticle->rotxvel)
					pParticle->rotxvel *= -fProj * 2 * pSystem->impactdamp * m_flFrameTime;

				if (pParticle->rotyvel)
					pParticle->rotyvel *= -fProj * 2 * pSystem->impactdamp * m_flFrameTime;
			}
			else if (pSystem->collision == PARTICLE_COLLISION_DECAL)
			{
				gBSPRenderer.CreateDecal(pmtrace.endpos, pmtrace.plane.normal, pSystem->create);
				return false;
			}
			else if (pSystem->collision == PARTICLE_COLLISION_NEW_SYSTEM || pSystem->collision == PARTICLE_COLLISION_RAIN)
			{
				if (bColWater && pSystem->watercreate[0] != 0)
				{
					for (int i = 0; i < pSystem->watersystem->startparticles; i++)
						CreateParticle(pSystem->watersystem, pmtrace.endpos, pmtrace.plane.normal);
				}
				if (pSystem->deathcreate[0] != 0)
				{
					// gEngfuncs.Con_Printf("CALLED!\n");
					CreateSystem(pSystem->deathcreate, pParticle->origin, pParticle->velocity.Normalize(), 0);
				}
				if (gEngfuncs.PM_PointContents(pmtrace.endpos, nullptr) != CONTENTS_SKY && pSystem->create[0] != 0)
				{
					for (int i = 0; i < pSystem->createsystem->startparticles; i++)
						CreateParticle(pSystem->createsystem, pmtrace.endpos, pmtrace.plane.normal);
				}
				return false;
			}
			else
			{
				// kill it
				return false;
			}
		}
		else
		{
			VectorCopy(vFinalVelocity, pParticle->velocity);
		}
	}
	else
	{
		VectorCopy(vFinalVelocity, pParticle->velocity);
	}

	//
	// Add in the final velocity
	//
	VectorMA(pParticle->origin, m_flFrameTime, vFinalVelocity, pParticle->origin);

	//
	// Always reset to 1.0
	//
	pParticle->alpha = 1.0;

	//
	// Fading in
	//
	if (pSystem->fadeintime)
	{
		if ((pParticle->spawntime + pSystem->fadeintime) > flTime)
		{
			float flFadeTime = pParticle->spawntime + pSystem->fadeintime;
			float flTimeToFade = flFadeTime - flTime;

			pParticle->alpha = 1.0 - (flTimeToFade / pSystem->fadeintime);
		}
	}

	//
	// Fade out
	//
	if (pParticle->fadeoutdelay)
	{
		if ((pParticle->fadeoutdelay + pParticle->spawntime) < flTime)
		{
			float flTimeToDeath = pParticle->life - flTime;
			float flFadeTime = pParticle->fadeoutdelay + pParticle->spawntime;
			float flFadeFrac = pParticle->life - flFadeTime;

			pParticle->alpha = flTimeToDeath / flFadeFrac;
		}
	}

	//
	// Minimum and maximum distance fading
	//
	if (pSystem->fadedistfar && pSystem->fadedistnear)
	{
		float flDist = (pParticle->origin - gBSPRenderer.m_vRenderOrigin).Length();
		float flAlpha = 1.0 - ((pSystem->fadedistfar - flDist) / (pSystem->fadedistfar - pSystem->fadedistnear));

		if (flAlpha < 0)
			flAlpha = 0;
		if (flAlpha > 1)
			flAlpha = 1;

		pParticle->alpha *= flAlpha;
	}

	//
	// Dampen scale
	//
	if (pSystem->scaledampfactor && (pParticle->scaledampdelay < flTime))
		pParticle->scale = pParticle->scale - m_flFrameTime * pSystem->scaledampfactor;

	if (pParticle->scale <= 0)
		return false;

	//
	// Check if lighting is required
	//
	// salsa: NO, JESUS, only do it once on creation
	// if (pSystem->lightcheck != PARTICLE_LIGHTCHECK_NONE)
	//{
	//	if (pSystem->lightcheck == PARTICLE_LIGHTCHECK_NORMAL)
	//	{
	//		pParticle->color = LightForParticle(pParticle);
	//	}
	//	else if (pSystem->lightcheck == PARTICLE_LIGHTCHECK_SCOLOR)
	//	{
	//		pParticle->scolor = LightForParticle(pParticle);
	//	}
	//	else if (pSystem->lightcheck == PARTICLE_LIGHTCHECK_MIXP)
	//	{
	//		pParticle->color = LightForParticle(pParticle);
	//		pParticle->color.x = pParticle->color.x * pSystem->primarycolor.x;
	//		pParticle->color.y = pParticle->color.y * pSystem->primarycolor.y;
	//		pParticle->color.z = pParticle->color.z * pSystem->primarycolor.z;
	//	}
	//}

	//
	// See if we need to blend colors
	//
	if (pSystem->lightcheck != PARTICLE_LIGHTCHECK_NORMAL)
	{
		if ((pParticle->secondarydelay < flTime) && (flTime < (pParticle->secondarydelay + pParticle->secondarytime)))
		{
			float flTimeFull = (pParticle->secondarydelay + pParticle->secondarytime) - flTime;
			float flColFrac = flTimeFull / pParticle->secondarytime;

			pParticle->color[0] = pParticle->scolor[0] * (1.0 - flColFrac) + pSystem->primarycolor[0] * flColFrac;
			pParticle->color[1] = pParticle->scolor[1] * (1.0 - flColFrac) + pSystem->primarycolor[1] * flColFrac;
			pParticle->color[2] = pParticle->scolor[2] * (1.0 - flColFrac) + pSystem->primarycolor[2] * flColFrac;
		}
	}

	//
	// Spawn tracer particles
	//
	if (pSystem->tracerdist)
	{
		Vector vDistance;
		vDistance = pParticle->origin - pParticle->lastspawn;

		if (vDistance.Length() > pSystem->tracerdist)
		{
			Vector vDirection = pParticle->origin - pParticle->lastspawn;
			int iNumTraces = vDistance.Length() / pSystem->tracerdist;

			for (int i = 0; i < iNumTraces; i++)
			{
				float flFraction = (i + 1) / (float)iNumTraces;
				Vector vOrigin = pParticle->lastspawn + vDirection * flFraction;
				CreateParticle(pSystem->createsystem, vOrigin, pParticle->velocity.Normalize());
			}

			VectorCopy(pParticle->origin, pParticle->lastspawn);
		}
	}

	//
	// Calculate texcoords
	//
	if (pSystem->numframes)
	{
		// Get desired frame
		int iFrame = ((int)((flTime - pParticle->spawntime) * pSystem->framerate));
		iFrame = (iFrame % pSystem->numframes);
		if (iFrame > pSystem->numframes)
			iFrame = pSystem->numframes;

		// Check if we actually have to set the frame
		if (iFrame != pParticle->frame)
		{
			cl_texture_t* pTexture = pSystem->texture;

			int iNumFramesX = pTexture->iWidth / pSystem->framesizex;
			int iNumFramesY = pTexture->iHeight / pSystem->framesizey;

			int iColumn = iFrame % iNumFramesX;
			int iRow = (iFrame / iNumFramesX) % iNumFramesY;

			// Calculate these only once
			float flFractionWidth = (float)pSystem->framesizex / (float)pTexture->iWidth;
			float flFractionHeight = (float)pSystem->framesizey / (float)pTexture->iHeight;

			// Calculate top left coordinate
			pParticle->texcoords[0][0] = (iColumn + 1) * flFractionWidth;
			pParticle->texcoords[0][1] = iRow * flFractionHeight;

			// Calculate top right coordinate
			pParticle->texcoords[1][0] = iColumn * flFractionWidth;
			pParticle->texcoords[1][1] = iRow * flFractionHeight;

			// Calculate bottom right coordinate
			pParticle->texcoords[2][0] = iColumn * flFractionWidth;
			pParticle->texcoords[2][1] = (iRow + 1) * flFractionHeight;

			// Calculate bottom left coordinate
			pParticle->texcoords[3][0] = (iColumn + 1) * flFractionWidth;
			pParticle->texcoords[3][1] = (iRow + 1) * flFractionHeight;

			// Fill in current frame
			pParticle->frame = iFrame;
		}
	}

	// All went well, particle is still active
	return true;
}

/*
====================
RenderParticle

====================
*/
void CParticleEngine::GetParticleQuad(cl_particle_t* pParticle, float flUp, float flRight, std::vector<ParticleQuad>& quadlist)
{
	float flDot;
	Vector vTemp;
	Vector vPoint;
	Vector vDir;
	Vector vAngles;

	if (pParticle->alpha == 0)
		return;

	/*
	if (pParticle->pSystem->shapetype == SYSTEM_SHAPE_BOX_AROUND_PLAYER)
	{
		pParticle->velocity[0] = gHUD.pparams->simvel[0];
		pParticle->velocity[1] = gHUD.pparams->simvel[1];
	}
	*/

	vDir = pParticle->origin - gBSPRenderer.m_vRenderOrigin;
	if (gHUD.m_pFogSettings.active)
	{
		if (vDir.Length() > gHUD.m_pFogSettings.end)
			return;
	}

	vDir = vDir.Normalize();
	flDot = DotProduct(vDir, m_vForward);

	// z clipped
	if (flDot < 0)
		return;

	cl_texture_t* pTexture = pParticle->pSystem->texture;

	if (pParticle->pSystem->displaytype == SYSTEM_DISPLAY_PLANAR)
	{
		gBSPRenderer.GetUpRight(pParticle->normal, m_vRUp, m_vRRight);
	}
	else if (pParticle->rotation || pParticle->rotx || pParticle->roty)
	{
		vAngles = gBSPRenderer.m_vViewAngles;

		if (pParticle->rotx)
			vAngles[0] = pParticle->rotx;
		if (pParticle->roty)
			vAngles[1] = pParticle->roty;
		if (pParticle->rotation)
			vAngles[2] = pParticle->rotation;

		AngleVectors(vAngles, nullptr, &m_vRRight, &m_vRUp);
	}

	ParticleQuad quad;

	if (pParticle->pSystem->displaytype == SYSTEM_DISPLAY_PARALELL)
	{

		vPoint = pParticle->origin + m_vRUp * flUp * pParticle->scale * 2;
		vPoint = vPoint + m_vRRight * flRight * (-pParticle->scale);
		quad.vert[0].pos = vPoint;

		vPoint = pParticle->origin + m_vRUp * flUp * pParticle->scale * 2;
		vPoint = vPoint + m_vRRight * flRight * pParticle->scale;
		quad.vert[1].pos = vPoint;

		vPoint = pParticle->origin + m_vRRight * flRight * pParticle->scale;
		quad.vert[2].pos = vPoint;

		vPoint = pParticle->origin + m_vRRight * flRight * (-pParticle->scale);
		quad.vert[3].pos = vPoint;
	}
	else
	{
		vPoint = pParticle->origin + m_vRUp * flUp * pParticle->scale;
		vPoint = vPoint + m_vRRight * flRight * (-pParticle->scale);
		quad.vert[0].pos = vPoint;

		vPoint = pParticle->origin + m_vRUp * flUp * pParticle->scale;
		vPoint = vPoint + m_vRRight * flRight * pParticle->scale;
		quad.vert[1].pos = vPoint;

		vPoint = pParticle->origin + m_vRUp * flUp * (-pParticle->scale);
		vPoint = vPoint + m_vRRight * flRight * pParticle->scale;
		quad.vert[2].pos = vPoint;

		vPoint = pParticle->origin + m_vRUp * flUp * (-pParticle->scale);
		vPoint = vPoint + m_vRRight * flRight * (-pParticle->scale);
		quad.vert[3].pos = vPoint;
	}

	memcpy(quad.vert[0].uv, pParticle->texcoords[0], sizeof(float) * 2);
	memcpy(quad.vert[1].uv, pParticle->texcoords[1], sizeof(float) * 2);
	memcpy(quad.vert[2].uv, pParticle->texcoords[2], sizeof(float) * 2);
	memcpy(quad.vert[3].uv, pParticle->texcoords[3], sizeof(float) * 2);

	quad.vert[0].color.r = std::clamp(pParticle->color.x * 255.f, 0.f, 255.f);
	quad.vert[0].color.g = std::clamp(pParticle->color.y * 255.f, 0.f, 255.f);
	quad.vert[0].color.b = std::clamp(pParticle->color.z * 255.f, 0.f, 255.f);
	quad.vert[0].color.a = (pParticle->alpha * pParticle->pSystem->mainalpha) * 255.f;
	if (pParticle->pSystem->displaytype == SYSTEM_RENDERMODE_ADDITIVE)
	{
		quad.vert[0].color.r *= (pParticle->alpha * pParticle->pSystem->mainalpha);
		quad.vert[0].color.g *= (pParticle->alpha * pParticle->pSystem->mainalpha);
		quad.vert[0].color.b *= (pParticle->alpha * pParticle->pSystem->mainalpha);
	}


	quad.vert[3].color = quad.vert[2].color = quad.vert[1].color = quad.vert[0].color;


	quadlist.push_back(quad);

	m_iNumParticles++;
}

/*
====================
DrawParticles

====================
*/
void CParticleEngine::DrawParticles()
{
	if (!m_pCvarDrawParticles->value)
		return;

	AngleVectors(gBSPRenderer.m_vViewAngles, &m_vForward, &m_vRight, &m_vUp);

	float flUp;
	float flRight;

	std::unordered_map<std::pair<GLuint, int>, std::vector<ParticleQuad>, ParticlePairHash> particlebatch;

	particle_system_t* psystem = m_pSystemHeader;
	while (psystem)
	{
		// Check if there's any particles at all
		if (!psystem->particleheader)
		{
			particle_system_t* pnext = psystem->next;
			psystem = pnext;
			continue;
		}

		// Check if it's in VIS
		if (psystem->leaf)
		{
			if (psystem->leaf->visframe != gBSPRenderer.m_pViewLeaf->visframe)
			{
				particle_system_t* pnext = psystem->next;
				psystem = pnext;
				continue;
			}
		}
		else
		{
			if (psystem->shapetype != SYSTEM_SHAPE_PLANE_ABOVE_PLAYER)
			{
				if (BSPWorld_Model::m_pWorldNodes)
					psystem->leaf = Mod_PointInLeaf(psystem->origin + Vector(0, 0, 8));

				if (psystem->leaf->visframe != gBSPRenderer.m_pViewLeaf->visframe)
				{
					particle_system_t* pnext = psystem->next;
					psystem = pnext;
					continue;
				}
			}

		}

		// Calculate up and right scalers
		if (psystem->numframes)
		{
			if (psystem->framesizex > psystem->framesizey)
			{
				flUp = (float)psystem->framesizey / (float)psystem->framesizex;
				flRight = 1.0;
			}
			else
			{
				flRight = (float)psystem->framesizex / (float)psystem->framesizey;
				flUp = 1.0;
			}
		}
		else
		{
			if (psystem->texture->iWidth > psystem->texture->iHeight)
			{
				flUp = (float)psystem->texture->iHeight / (float)psystem->texture->iWidth;
				flRight = 1.0;
			}
			else
			{
				flRight = (float)psystem->texture->iWidth / (float)psystem->texture->iHeight;
				flUp = 1.0;
			}
		}

		if (psystem->displaytype == SYSTEM_DISPLAY_PARALELL)
		{
			VectorCopy(m_vRight, m_vRRight);
			VectorClear(m_vRUp);
			m_vRUp[2] = 1;
		}
		else if (!psystem->rotationvel && !psystem->rotxvel && !psystem->rotyvel)
		{
			VectorCopy(m_vRight, m_vRRight);
			VectorCopy(m_vUp, m_vRUp);
		}

		std::vector<ParticleQuad> quadlist;

		// Render all particles tied to this system
		cl_particle_t* pparticle = psystem->particleheader;
		while (pparticle)
		{

			GetParticleQuad(pparticle, flUp, flRight, quadlist);
			cl_particle_t* pnext = pparticle->next;
			pparticle = pnext;
		}

		// append them to the correct batch
		std::pair<GLuint, int> particlepair = std::pair<GLuint, int>(psystem->texture->iIndex, psystem->rendermode);
		auto& batch = particlebatch[particlepair];
		batch.insert(batch.end(), std::begin(quadlist), std::end(quadlist));


		particle_system_t* pnext = psystem->next;
		psystem = pnext;
	}

	if (particlebatch.empty())
		return;

	s_ParticleShader.Bind();

	DrawQuadList(particlebatch, psystem);

	GL_ShaderProgram::ResetShaderBind();
}

void CParticleEngine::DrawQuadList(std::unordered_map<std::pair<GLuint, int>, std::vector<ParticleQuad>, ParticlePairHash>& particlebatch, particle_system_t* psystem)
{
	g_GlobalGLState.SetBlend(true);
	g_GlobalGLState.SetDepthWrite(false);
	g_GlobalGLState.SetCullFace(false);

	static int projviewmatrixloc = s_ParticleShader.GetUniformLoc("projviewmatrix");
	
	s_ParticleShader.UniformMatrix4fv(projviewmatrixloc, 1, GL_FALSE, glm::value_ptr(gBSPRenderer.m_ProjectionMatrix * gBSPRenderer.m_ViewMatrix));

	std::vector<ParticleVertex> verts;
	for (auto batch : particlebatch)
	{
		if (batch.second.empty())
			continue;
		for (auto quad : batch.second)
		{
			verts.push_back(quad.vert[0]);
			verts.push_back(quad.vert[1]);
			verts.push_back(quad.vert[2]);
			verts.push_back(quad.vert[3]);
		}
	}
	s_pParticleBuffer->BindMesh();

	s_pParticleBuffer->ModifyMesh(verts.data(), verts.size(), 0);

	int offset = 0;
	int currendermode = -1;
	GLuint curtexture = 0;
	for (auto batch : particlebatch)
	{
		if (batch.second.empty())
			continue;
		if (currendermode != batch.first.second)
		{
			currendermode = batch.first.second;
			switch (batch.first.second)
			{
				case SYSTEM_RENDERMODE_ADDITIVE:
				{
					g_GlobalGLState.SetBlendFunc(GL_SRC_ALPHA, GL_ONE);
					break;
				}
				case SYSTEM_RENDERMODE_ALPHABLEND:
				{
					g_GlobalGLState.SetBlendFunc(GL_ONE, GL_ONE);
					break;
				}
				default:
				{
					g_GlobalGLState.SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					break;
				}
			}
		}

		if (curtexture != batch.first.first)
		{
			curtexture = batch.first.first;
			gBSPRenderer.BindGLTexture(GL_TEXTURE0, batch.first.first);
		}

		s_pParticleBuffer->DrawArrays(offset, batch.second.size() * 4);
		offset += batch.second.size() * 4;
	}
		
	g_GlobalGLState.SetBlend(false);
	g_GlobalGLState.SetDepthWrite(true);
	g_GlobalGLState.SetCullFace(true);
}

/*
====================
RemoveSystem

====================
*/
void CParticleEngine::RemoveSystem(int iId)
{
	if (!m_pSystemHeader)
		return;

	if (!iId)
		return;

	particle_system_t* psystem = m_pSystemHeader;
	while (psystem)
	{
		if (psystem->id != iId)
		{
			particle_system_t* pnext = psystem->next;
			psystem = pnext;
			continue;
		}

		// Remove all related particles
		cl_particle_t* pparticle = psystem->particleheader;
		while (pparticle)
		{
			cl_particle_t* pfree = pparticle;
			pparticle = pfree->next;

			m_iNumFreedParticles++;
			delete[] pfree;
		}

		// Unlink this
		if (psystem->createsystem)
			psystem->createsystem->parentsystem = nullptr;

		// Unlink this
		if (psystem->watersystem)
			psystem->watersystem->parentsystem = nullptr;

		if (!psystem->prev)
		{
			m_pSystemHeader = psystem->next;
			if (psystem->next)
				psystem->next->prev = nullptr;
		}
		else
		{
			psystem->prev->next = psystem->next;
			if (psystem->next)
				psystem->next->prev = psystem->prev;
		}

		m_iNumFreedSystems++;
		delete[] psystem;
		break;
	}
}

/*
====================
MsgCreateSystem

====================
*/
int CParticleEngine::MsgCreateSystem(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	Vector pos;
	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();

	Vector ang;
	ang.x = READ_COORD();
	ang.y = READ_COORD();
	ang.z = READ_COORD();

	int iType = READ_BYTE();
	char* szPath = READ_STRING();
	int iId = READ_SHORT();

	if (iType == 2)
		RemoveSystem(iId);
	else if (iType == 1)
		CreateCluster(szPath, pos, ang, iId);
	else
		CreateSystem(szPath, pos, ang, iId);

	return 1;
}
