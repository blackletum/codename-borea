/*
Trinity Rendering Engine - Copyright Andrew Lucas 2009-2012

The Trinity Engine is free software, distributed in the hope th-
at it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU Lesser General Public License for more det-
ails.

Particle Engine
Written by Andrew Lucas
*/

#if !defined(PARTICLE_ENGINE_H)
#define PARTICLE_ENGINE_H
#if defined(_WIN32)
#pragma once
#endif

#include "PlatformHeaders.h"
#include "pm_defs.h"
#include "cl_entity.h"
#include "ref_params.h"
#include "dlight.h"
#include "parsemsg.h"
#include "cvardef.h"
#include "textureloader.h"
#include "rendererdefs.h"
#include "opengl_utils/GL_VertexArrayObject.h"

#include <unordered_map>

//==============================
//		PARTICLE ENGINE DEFS
//
//==============================
#define SYSTEM_SHAPE_POINT 0
#define SYSTEM_SHAPE_BOX 1
#define SYSTEM_SHAPE_PLANE_ABOVE_PLAYER 2
#define SYSTEM_SHAPE_BOX_AROUND_PLAYER 3

#define SYSTEM_DISPLAY_NORMAL 0
#define SYSTEM_DISPLAY_PARALELL 1
#define SYSTEM_DISPLAY_PLANAR 2
#define SYSTEM_DISPLAY_TRACER 3

#define SYSTEM_RENDERMODE_ADDITIVE 0
#define SYSTEM_RENDERMODE_ALPHABLEND 1
#define SYSTEM_RENDERMODE_INTENSITY 2

#define PARTICLE_COLLISION_NONE 0
#define PARTICLE_COLLISION_DIE 1
#define PARTICLE_COLLISION_BOUNCE 2
#define PARTICLE_COLLISION_DECAL 3
#define PARTICLE_COLLISION_STUCK 4
#define PARTICLE_COLLISION_NEW_SYSTEM 5
#define PARTICLE_COLLISION_RAIN 6

#define PARTICLE_WIND_NONE 0
#define PARTICLE_WIND_LINEAR 1
#define PARTICLE_WIND_SINE 2

#define PARTICLE_LIGHTCHECK_NONE 0
#define PARTICLE_LIGHTCHECK_NORMAL 1
#define PARTICLE_LIGHTCHECK_SCOLOR 2
#define PARTICLE_LIGHTCHECK_MIXP 3


struct ParticlePairHash
{
	template <class T1, class T2>
	std::size_t operator()(const std::pair<T1, T2>& p) const
	{
		auto h1 = std::hash<T1>{}(p.first);
		auto h2 = std::hash<T2>{}(p.second);
		return h1 ^ (h2 << 1);
	}
};

struct ParticleVertex
{
	Vector pos;
	float uv[2];
	color32 color;
	GL_DECLARE_ATTRIBLIST();
};

struct ParticleQuad
{
	ParticleVertex vert[4];
};

class GL_BufferHandler;
class GL_ShaderProgram;
class GL_VertexArrayObject;
struct cl_dlight_t;

//========================================
//			PARTICLE ENGINE STRUCTS
//
//========================================
struct particle_system_t
{
	//id points to a server entity
	//>= 0 means this system needs to follow this server entity. 
	// if entity not found in our dictionary then dont render, just run logic
	unsigned int id;

	byte shapetype;
	bool randomdir;

	Vector origin;
	Vector dir;

	float minvel;
	float maxvel;
	float maxofs;

	float skyheight;

	float spawntime;
	float fadeintime;
	float fadeoutdelay;
	float velocitydamp;
	float stuckdie;
	float tracerdist;

	float maxheight;

	float windx;
	float windy;
	float windvar;
	float windmult;
	float windmultvar;
	byte windtype;

	float maxlife;
	float maxlifevar;
	float systemsize;

	Vector primarycolor;
	Vector secondarycolor;
	float transitiondelay;
	float transitiontime;
	float transitionvar;

	float rotationvar;
	float rotationvel;
	float rotationdamp;
	float rotationdampdelay;

	float rotxvar;
	float rotxvel;
	float rotxdamp;
	float rotxdampdelay;

	float rotyvar;
	float rotyvel;
	float rotydamp;
	float rotydampdelay;

	float scale;
	float scalevar;
	float scaledampdelay;
	float scaledampfactor;
	float veldampdelay;
	float gravity;
	float particlefreq;
	float impactdamp;
	float mainalpha;

	unsigned short startparticles;
	unsigned short maxparticles;
	unsigned short maxparticlevar;

	byte lightcheck;
	byte collision;
	bool colwater;
	byte displaytype;
	byte rendermode;
	unsigned short numspawns;

	int fadedistfar;
	int fadedistnear;

	unsigned short numframes;
	unsigned short framesizex;
	unsigned short framesizey;
	unsigned short framerate;
	unsigned short randomframe;

	char create[64];
	char deathcreate[64];
	char watercreate[64];

	particle_system_t* createsystem;
	particle_system_t* watersystem;
	particle_system_t* parentsystem;

	cl_texture_t* texture;
	clientmleaf_t* leaf;

	particle_system_t* next;
	particle_system_t* prev;

	struct cl_particle_t* particleheader;
};

struct cl_particle_t
{
	Vector velocity;
	Vector origin;
	Vector color;
	Vector scolor;
	Vector lastspawn;
	Vector normal;

	float spawntime;
	float life;
	float scale;
	float alpha;

	float fadeoutdelay;

	float scaledampdelay;
	float secondarydelay;
	float secondarytime;

	float rotationvel;
	float rotation;

	float rotx;
	float rotxvel;

	float roty;
	float rotyvel;

	float windxvel;
	float windyvel;
	float windmult;

	float texcoords[4][2];

	int frame;

	particle_system_t* pSystem;

	cl_particle_t* next;
	cl_particle_t* prev;

	byte pad[4];
};

/*
====================
CParticleEngine

====================
*/
class CParticleEngine
{
public:
	void Init(void);
	void VidInit(void);
	void Shutdown(void);

	void CreateCluster(const char* szPath, Vector origin, Vector dir, int iId);
	particle_system_t* CreateSystem(char* szPath, Vector origin, Vector dir, int iId, particle_system_t* parent = NULL);

	void RemoveSystem(int iId);

	particle_system_t* AllocSystem(void);
	cl_particle_t* AllocParticle(particle_system_t* pSystem);

	void Update(void);
	void DrawParticles(void);
	void CullSystems(void);
	void UpdateSystems(void);

	Vector LightForParticle(cl_particle_t* pParticle);
	bool CheckLightBBox(cl_particle_t* pParticle, cl_dlight_t* pLight);

	void EnvironmentCreateFirst(particle_system_t* pSystem);

	void CreateParticle(particle_system_t* pSystem, float* flOrigin = NULL, float* flNormal = NULL);
	bool UpdateParticle(cl_particle_t* pParticle);
	void GetParticleQuad(cl_particle_t* pParticle, float flUp, float flRight, std::vector<ParticleQuad>& quadlist);

	void DrawQuadList(std::unordered_map<std::pair<GLuint, int>, std::vector<ParticleQuad>, ParticlePairHash>& particlebatch, particle_system_t* psystem);

	int MsgCreateSystem(const char* pszName, int iSize, void* pbuf);

public:

	particle_system_t* m_pSystemHeader;

	cvar_t* m_pCvarDrawParticles;
	cvar_t* m_pCvarParticleDebug;
	cvar_t* m_pCvarGravity;

	float m_flLastDraw;
	float m_flFrameTime;

	int m_iNumParticles;

	int m_iNumFreedParticles;
	int m_iNumCreatedParticles;

	int m_iNumFreedSystems;
	int m_iNumCreatedSystems;

	Vector m_vForward;
	Vector m_vRight;
	Vector m_vUp;

	Vector m_vRRight;
	Vector m_vRUp;
};
extern CParticleEngine gParticleEngine;
#endif