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

typedef struct clientmnode_t
{
	// common with leaf
	int contents; // 0, to differentiate from leafs
	int visframe; // node needs to be traversed if current

	float minmaxs[6]; // for bounding box culling

	struct mnode_s* parent;

	// node specific
	mplane_t* plane;
	clientmnode_t* children[2];

	unsigned short firstsurface;
	unsigned short numsurfaces;
};

typedef struct clientmleaf_s
{
	// common with node
	int contents; // wil be a negative contents number
	int visframe; // node needs to be traversed if current

	float minmaxs[6]; // for bounding box culling

	struct mnode_s* parent;

	// leaf specific
	byte* compressed_vis;
	struct efrag_s* efrags;

	msurface_t** firstmarksurface;
	int nummarksurfaces;
	int key; // BSP sequence number for leaf's contents
	byte ambient_sound_level[NUM_AMBIENTS];
} clientmleaf_t;

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

	static mnode_t* m_pWorldNodes;
	static mleaf_t* m_pWorldLeafs;
	static msurface_t* m_pWorldSurfaces;
	static int m_iNumWorldNodes;
	static int m_iNumWorldLeafs;
	static int m_iNumWorldSurfaces;
};

