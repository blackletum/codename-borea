
#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "studio.h"
#include "entity_state.h"
#include "cl_entity.h"

#include "event_api.h"
#include "pmtrace.h"

#include <stdio.h>
#include <vector>
#include <string.h>
#include <memory.h>
#include <math.h>
#include <array>
#include <limits>

#include "filesystem_utils.h"

#include "rendererdefs.h"
#include "textureloader.h"

#include "studio_util.h"
#include "r_studioint.h"
#include "opengl_utils/GL_Buffers.h"
#include "opengl_utils/GL_ShaderProgram.h"
#include "opengl_utils/GL_VertexArrayObject.h"

#include "BSPModel_Gen.h"

#include <unordered_map>



//
//
// BSP Geometry decoupled from goldsrc, so that in the future extra info can be loaded and applied onto bsp geometry (surface normal textures, external lighting data, etc)
//
//





BSPModel_Model::BSPModel_Model(model_t* bspmodel)
{

}



mnode_t* BSPWorld_Model::m_pWorldNodes = nullptr;
mleaf_t* BSPWorld_Model::m_pWorldLeafs = nullptr;
msurface_t* BSPWorld_Model::m_pWorldSurfaces = nullptr;
int BSPWorld_Model::m_iNumWorldNodes = 0;
int BSPWorld_Model::m_iNumWorldLeafs = 0;
int BSPWorld_Model::m_iNumWorldSurfaces = 0;

void BSPWorld_Model::InitWorldModel(model_t* worldmdl)
{
	//this is supposed to be a studiomdl_meshgen but for bsp models,
	//may be useful if we ever need to decouple from goldsrc since it still
	//reads and has writing access to world model (or just add new info, like normals on surfaces, etc)

	//the world model holds literally all bsp geometry data, separate bsp models (like func_doors, func_rotating) have their surfaces point to worldmodel data


	//salsatobias: not used for now.

	m_iNumWorldNodes = worldmdl->numnodes;
	m_iNumWorldLeafs = worldmdl->numleafs;
	m_iNumWorldSurfaces = worldmdl->numsurfaces;

	if (m_pWorldNodes)
	{
		delete[] m_pWorldNodes;
		delete[] m_pWorldLeafs;
		delete[] m_pWorldSurfaces;
	}

	return;

	m_pWorldNodes = new mnode_t[worldmdl->numnodes];
	m_pWorldLeafs = new mleaf_t[worldmdl->numleafs + 1];
	m_pWorldSurfaces = new msurface_t[worldmdl->numsurfaces];

	auto remapNode = [&](mnode_t* old) -> mnode_t*
	{
		if (!old)
			return nullptr;

		return &m_pWorldNodes[old - worldmdl->nodes];
	};

	auto remapLeaf = [&](mleaf_t* old) -> mleaf_t*
	{
		if (!old)
			return nullptr;

		return &m_pWorldLeafs[old - worldmdl->leafs];
	};

	auto remapSurf = [&](msurface_t* old) -> msurface_t*
	{
		if (!old)
			return nullptr;

		return &m_pWorldSurfaces[old - worldmdl->surfaces];
	};

	for (int i = 0; i < worldmdl->numsurfaces; i++)
	{
		msurface_t* oldSurf = &worldmdl->surfaces[i];
		msurface_t* newSurf = &m_pWorldSurfaces[i];

		*newSurf = *oldSurf;

		m_iNumWorldSurfaces++;
	}

	for (int i = 0; i < worldmdl->numnodes; i++)
	{
		mnode_t* oldNode = &worldmdl->nodes[i];
		mnode_t* newNode = &m_pWorldNodes[i];

		*newNode = *oldNode;

		newNode->parent = remapNode(oldNode->parent);
		if (oldNode->children[0])
		{
			if (oldNode->children[0]->contents >= 0)
				newNode->children[0] = remapNode(oldNode->children[0]);
			else
				newNode->children[0] = (mnode_t*)remapLeaf((mleaf_t*)oldNode->children[0]);
		}
		if (oldNode->children[1])
		{
			if (oldNode->children[1]->contents >= 0)
				newNode->children[1] = remapNode(oldNode->children[1]);
			else
				newNode->children[1] = (mnode_t*)remapLeaf((mleaf_t*)oldNode->children[1]);
		}

		newNode->plane = oldNode->plane;

		m_iNumWorldNodes++;
	}

	for (int i = 0; i < worldmdl->numleafs + 1; i++)
	{
		mleaf_t* oldLeaf = &worldmdl->leafs[i];
		mleaf_t* newLeaf = &m_pWorldLeafs[i];

		*newLeaf = *oldLeaf;

		newLeaf->parent = remapNode(oldLeaf->parent);

		newLeaf->compressed_vis = oldLeaf->compressed_vis;

		for (int j = 0; j < newLeaf->nummarksurfaces; j++)
		{
			newLeaf->firstmarksurface[j] = remapSurf(oldLeaf->firstmarksurface[j]);
		}
	}

	worldmdl->nodes = m_pWorldNodes;
	worldmdl->leafs = m_pWorldLeafs;
	worldmdl->surfaces = m_pWorldSurfaces;


}