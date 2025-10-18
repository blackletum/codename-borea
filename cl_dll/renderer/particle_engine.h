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

#include <unordered_map>


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
};

struct ParticleQuad
{
	ParticleVertex vert[4];
};

class GL_BufferHandler;
class GL_ShaderProgram;
class GL_VertexArrayObject;

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
	GL_BufferHandler* m_pQuadBuffer;
	GL_ShaderProgram *m_ParticleShader;
	GL_VertexArrayObject* m_pParticleVAO;

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