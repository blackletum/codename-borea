#pragma once


#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "studio.h"
#include "entity_state.h"
#include "cl_entity.h"

#include <stdio.h>
#include <vector>

#include "filesystem_utils.h"

#include "rendererdefs.h"

#include <unordered_map>


class BSPModel_Model
{
public:
	BSPModel_Model(model_t* bspmodel);

	virtual bool IsWorld() { return false; };
};

class BSPWorld_Model
{
public:
	static void InitWorldModel(model_t* worldmdl);

	static clientmnode_t* m_pWorldNodes;
	static clientmleaf_t* m_pWorldLeafs;
	static clientmsurface_t* m_pWorldSurfaces;
	static int m_iNumWorldNodes;
	static int m_iNumWorldLeafs;
	static int m_iNumWorldSurfaces;
};

int AllocBlock(int w, int h, int* x, int* y);