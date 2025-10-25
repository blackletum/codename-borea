
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



clientmnode_t* BSPWorld_Model::m_pWorldNodes = nullptr;
clientmleaf_t* BSPWorld_Model::m_pWorldLeafs = nullptr;
clientmsurface_t* BSPWorld_Model::m_pWorldSurfaces = nullptr;
int BSPWorld_Model::m_iNumWorldNodes = 0;
int BSPWorld_Model::m_iNumWorldLeafs = 0;
int BSPWorld_Model::m_iNumWorldSurfaces = 0;

void BSPWorld_Model::InitWorldModel(model_t* worldmdl)
{
	//this is supposed to be a studiomdl_meshgen but for bsp models,
	//may be useful if we ever need to decouple from goldsrc since it still
	//reads and has writing access to world model (or just add new info, like normals on surfaces, etc)

	//the world model holds literally all bsp geometry data, separate bsp models (like func_doors, func_rotating) have their surfaces point to worldmodel data

	m_iNumWorldNodes = worldmdl->numnodes;
	m_iNumWorldLeafs = worldmdl->numleafs;
	m_iNumWorldSurfaces = worldmdl->numsurfaces;

	if (m_pWorldNodes)
	{
		delete[] m_pWorldNodes;
		delete[] m_pWorldLeafs;
		delete[] m_pWorldSurfaces;
	}

	m_pWorldNodes = new clientmnode_t[worldmdl->numnodes];
	m_pWorldLeafs = new clientmleaf_t[worldmdl->numleafs + 1];
	m_pWorldSurfaces = new clientmsurface_t[worldmdl->numsurfaces];

	auto remapNode = [&](mnode_t* old) -> clientmnode_t*
	{
		if (!old)
			return nullptr;

		return &m_pWorldNodes[old - worldmdl->nodes];
	};

	auto remapLeaf = [&](mleaf_t* old) -> clientmleaf_t*
	{
		if (!old)
			return nullptr;

		return &m_pWorldLeafs[old - worldmdl->leafs];
	};

	auto remapSurf = [&](msurface_t* old) -> clientmsurface_t*
	{
		if (!old)
			return nullptr;

		return &m_pWorldSurfaces[old - worldmdl->surfaces];
	};

	for (int i = 0; i < worldmdl->numsurfaces; i++)
	{
		msurface_t* oldSurf = &worldmdl->surfaces[i];
		clientmsurface_t* newSurf = &m_pWorldSurfaces[i];


		newSurf->plane = oldSurf->plane;
		newSurf->flags = oldSurf->flags;
	
		newSurf->firstedge = oldSurf->firstedge;  // look up in model->surfedges[], negative numbers
		newSurf->numedges = oldSurf->numedges;   // are backwards edges

		memcpy(newSurf->texturemins, oldSurf->texturemins, sizeof(short) * 2);
		memcpy(newSurf->extents, oldSurf->extents, sizeof(short) * 2);

		newSurf->light_s = oldSurf->light_s;           // gl lightmap coordinates
		newSurf->light_t = oldSurf->light_t;
		newSurf->polys = oldSurf->polys;                     // multiple if warped
		newSurf->texturechain = nullptr;

		newSurf->texinfo = oldSurf->texinfo;

		newSurf->lightmaptexturenum = oldSurf->lightmaptexturenum;
		memcpy(newSurf->styles, oldSurf->styles, sizeof(byte) * 4);
		memcpy(newSurf->cached_light, oldSurf->cached_light, sizeof(int) * 4); // values currently used in lightmap

		const int smax = (newSurf->extents[0] >> 4) + 1;
		const int tmax = (newSurf->extents[1] >> 4) + 1;
		const int size = smax * tmax;

		color24* lightmap = oldSurf->samples;

		for (int maps = 0; maps < MAXLIGHTMAPS && newSurf->styles[maps] != 255; ++maps)
		{
			const int style = newSurf->styles[maps];

			for (int i = 0; i < size; ++i)
			{
				newSurf->samples.push_back(lightmap[i]);

			}

			lightmap += size;
		}

		m_iNumWorldSurfaces++;
	}

	for (int i = 0; i < worldmdl->numnodes; i++)
	{
		mnode_t* oldNode = &worldmdl->nodes[i];
		clientmnode_t* newNode = &m_pWorldNodes[i];

		memcpy(newNode, oldNode, sizeof(mnode_t));

		newNode->parent = remapNode(oldNode->parent);
		if (oldNode->children[0])
		{
			if (oldNode->children[0]->contents >= 0)
				newNode->children[0] = remapNode(oldNode->children[0]);
			else
				newNode->children[0] = (clientmnode_t*)remapLeaf((mleaf_t*)oldNode->children[0]);
		}
		if (oldNode->children[1])
		{
			if (oldNode->children[1]->contents >= 0)
				newNode->children[1] = remapNode(oldNode->children[1]);
			else
				newNode->children[1] = (clientmnode_t*)remapLeaf((mleaf_t*)oldNode->children[1]);
		}

		newNode->plane = oldNode->plane;

		m_iNumWorldNodes++;
	}

	for (int i = 0; i < worldmdl->numleafs + 1; i++)
	{
		mleaf_t* oldLeaf = &worldmdl->leafs[i];
		clientmleaf_t* newLeaf = &m_pWorldLeafs[i];

		memcpy(newLeaf, oldLeaf, sizeof(mleaf_t));

		newLeaf->parent = remapNode(oldLeaf->parent);

		newLeaf->compressed_vis = oldLeaf->compressed_vis;

		for (int j = 0; j < newLeaf->nummarksurfaces; j++)
		{
			newLeaf->firstmarksurface[j] = remapSurf(oldLeaf->firstmarksurface[j]);
		}
	}

	//worldmdl->nodes = m_pWorldNodes;
	//worldmdl->leafs = m_pWorldLeafs;
	//worldmdl->surfaces = m_pWorldSurfaces;


}

//int AllocBlock(int w, int h, int* x, int* y)
//{
//	int best;
//	int texnum;
//	int best2 = 0;
//	texnum = 0;
//	for (int k = 0; ; k += 128)
//	{
//		best2 = BLOCK_SIZE;
//		for (int i = 0; i < BLOCK_SIZE - w; i++)
//		{
//			best = 0;
//			int j;
//			for (j = 0; j < w; j++)
//			{
//				int* alloc = (int*)allocated + i + k + j;
//				if (*alloc >= best2)
//					break;
//				if (*alloc > best)
//					best = *alloc;
//			}
//			if (j == w)
//			{
//				*y = best2 = best;
//				*x = i;
//			}
//		}
//		if (best2 + h <= BLOCK_SIZE)
//			break;
//		++texnum;
//		//if (k + BLOCK_SIZE >= 0x2000)
//		//	Sys_Error("%s: Full", __func__);
//	}
//
//	for (int i = 0; i < w; i++)
//		allocated[texnum][i + *x] = best2 + h;
//
//	return texnum;
//}