#pragma once

#include "PlatformHeaders.h"
#include "hud.h"
#include "cl_util.h"

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <math.h>
#include <vector>

#define TRACER_COLORINDEX_DEFAULT 4

// reverse engineered beam code from goldsrc
class CGoldSrc_TracerRenderer
{
public:
	particle_t* AllocateTracer(const Vector org, const Vector vel, float life);

	void CL_RunTracerLogic();

	void CL_DrawTracers();

	void CL_ClearTracerList() { m_TracerTempEnt_List.clear(); }

private:
	bool CL_CullTracer(particle_t* p, const Vector start, const Vector end);

	std::vector<std::unique_ptr<particle_t>> m_TracerTempEnt_List;
};

extern CGoldSrc_TracerRenderer g_TracerRenderer;