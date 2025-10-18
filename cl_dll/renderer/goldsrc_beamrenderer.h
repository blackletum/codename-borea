#pragma once

#include "PlatformHeaders.h"
#include "hud.h"

// reverse engineered beam code from goldsrc
class CGoldSrc_BeamRenderer
{
public:
	void R_DrawBeams(float frametime);

	void AddBeamEnt(cl_entity_t* beament) { m_BeamEnt_List.push_back(beament); }

	BEAM* AllocateTempBeam() 
	{ 
		auto& ptr = m_BeamTempEnt_List.emplace_back(std::make_unique<BEAM>());
		return ptr.get();
	}

	std::vector<std::unique_ptr<BEAM>> &GetTempBeamList()
	{
		return m_BeamTempEnt_List;
	}

	void Reset()
	{
		m_BeamTempEnt_List.clear();
		m_BeamEnt_List.clear();
		m_vDummyParticleList.clear();
	}

	void NewFrame() { m_BeamEnt_List.clear(); }

	void SetBeamAttributes(BEAM* pbeam, float r, float g, float b, float framerate, float startFrame);

	void R_BeamSetup(BEAM* pbeam, Vector start, Vector end, int modelIndex, float life, float width, float amplitude, float brightness, float speed);

	qboolean R_BeamCull(const Vector start, const Vector end, qboolean pvsOnly);

private:
	void R_BeamDrawCustomEntity(cl_entity_t* ent);

	void R_BeamDraw(BEAM* pbeam, float frametime);

	void R_BeamComputePerpendicular(const Vector vecBeamDelta, Vector& pPerp);

	void R_BeamComputeNormal(const Vector vStartPos, const Vector vNextPos, Vector& pNormal);

	qboolean R_BeamComputePoint(int beamEnt, Vector& pt);

	particle_t* CL_AllocParticleFast()
	{
		auto &ptr_ = m_vDummyParticleList.emplace_back(std::make_unique<particle_t>());
		return ptr_.get();
	}

	void Delete_particle(particle_t* part)
	{
		int i = 0;
		for (auto& ptr : m_vDummyParticleList)
		{
			if (ptr.get() == part)
			{
				m_vDummyParticleList.erase(m_vDummyParticleList.begin() + i);
				break;
			}
			i++;
		}
	}


	qboolean R_BeamRecomputeEndpoints(BEAM* pbeam);

	void CL_RunBeamLogic(BEAM* tempbeam, int listIndex); // only for temp beams


	// opengl rendering funcs

	void R_DrawSegs(Vector source, Vector delta, float width, float scale, float freq, float speed, int segments, int flags);
	void R_DrawTorus(Vector source, Vector delta, float width, float scale, float freq, float speed, int segments);
	void R_DrawDisk(Vector source, Vector delta, float width, float scale, float freq, float speed, int segments);
	void R_DrawCylinder(Vector source, Vector delta, float width, float scale, float freq, float speed, int segments);
	void R_DrawRing(Vector source, Vector delta, float width, float amplitude, float freq, float speed, int segments);
	void R_DrawBeamFollow(BEAM* pbeam);

	//


	std::vector<std::unique_ptr<BEAM>> m_BeamTempEnt_List;
	std::vector<cl_entity_t*> m_BeamEnt_List;
	std::vector<std::unique_ptr<particle_t>> m_vDummyParticleList;
};

extern CGoldSrc_BeamRenderer g_BeamRenderer;