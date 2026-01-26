/*
Trinity Rendering Engine - Copyright Andrew Lucas 2009-2012

The Trinity Engine is free software, distributed in the hope th-
at it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU Lesser General Public License for more det-
ails.

Client side entity manager
Written by Andrew Lucas
Transparency code by Neil "Jed" Jedrzejewski
*/

#if !defined(BSPLOADER_H)
#define BSPLOADER_H
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

class GL_BufferHandler;
class GL_VertexArrayObject;
class StudioMDL_Model;
struct studiohdr_t;
struct decal_msg_cache;
struct brushvertex_t;

//========================================
//			PROP MANAGER DEFINITIONS
//
//========================================
#define MAX_POINTS 64

//========================================
//			PROP MANAGER STRUCTS
//
//========================================
typedef struct epair_s
{
	struct epair_s* next;
	char* key;
	char* value;
} epair_t;

typedef struct
{
	Vector origin;
	int firstbrush;
	int numbrushes;
	epair_t* epairs;
} entity_t;

struct vbomesh_t
{
	int start_vertex;
	int num_vertexes;
};

struct vbosubmodel_t
{
	vbomesh_t* meshes;
	int nummeshes;
};

struct vboheader_t
{
	brushvertex_t* pBufferData;
	int numverts;

	unsigned int* indexes;
	int numindexes;

	vbosubmodel_t* submodels;
	int numsubmodels;
};

struct modeldata_t
{
	char name[256];

	model_t* pMdl;
	vboheader_t pVBOHeader;
};

struct entextradata_t
{
	Vector absmax;
	Vector absmin;
	Vector lightorigin;

	int num_leafs;
	short leafnums[MAX_ENT_LEAFS];
	//float pbones[MAXSTUDIOBONES][3][4]; unusued

	modeldata_t* pModelData;

	glm::mat4 modelmatrix;
};

#define PROPFLAG_FOLIAGE (1 << 0)
#define PROPFLAG_BLIMP (1 << 1)

struct entextrainfo_t
{
	int surfindex;
	int prop_flags;
	int lightstyles[4];
	Vector prevpos;

	lighting_ext pLighting;
	cl_entity_t* pEntity;
	entextradata_t* pExtraData; // only used by CL ents
};

struct cabledata_t
{
	int iwidth;
	int isegments;

	Vector vmins;
	Vector vmaxs;

	Vector vpoints[MAX_POINTS];
	int inumpoints;

	int num_leafs;
	short leafnums[MAX_ENT_LEAFS];
};

class CPropManager
{
public:
	void Init(void);
	void VidInit(void);
	void Shutdown(void);
	void ClearEntityData(void);

	void GenerateEntityList(void);
	void SetupVBO(void);

	void Reset(void);
	void RenderProps(bool bSkybox = false);
	void RenderPropsSolid(void);

	void HandleFoliageProp(cl_entity_s* ent, entextradata_t* extradata);
	void HandleBlimpProp(cl_entity_s* ent, entextradata_t* extradata);

	// Models
	bool LoadMDL(const char* name, cl_entity_t* pEntity, entity_t* pBSPEntity);
	modeldata_t* GetHeader(const char* name);

	bool SetupCable(cabledata_t* cable, entity_t* entity);
	void DrawCables(void);

	void ParseEntities(void);
	void LoadEntVars(void);
	char* ValueForKey(entity_t* ent, const char* key);

public:
	int m_iEntDataSize;
	char* m_pEntData;

	bool m_bAvailable;

	// Entity list extracted from BSP.
	std::vector<entity_t> m_pBSPEntities;

	// Total amount of renderable entities.
	std::vector<cl_entity_t> m_pEntities;

	// Total amount of renderable entities.
	std::vector<cl_entity_t> m_pModelLights;

	// Studio header pointers.
	std::vector<std::unique_ptr<modeldata_t>> m_pHeaders;

	// Decals
	std::vector<decal_msg_cache> m_pDecals;

	std::vector<cabledata_t> m_pCables;

	// Extra data for all entities.
	entextradata_t* m_pCurrentExtraData;
	std::vector<std::unique_ptr<entextradata_t>> m_pExtraData;
	std::vector<std::unique_ptr<entextrainfo_t>> m_pExtraInfo;

	brushvertex_t* m_pVertexData;
	int m_iNumTotalVerts;

	GL_ShaderProgram* m_CableShader;

	GL_BufferHandler *m_pStaticModelBuffer;
	GL_VertexArrayObject *m_pStaticModelVAO;

	//may be best to merge this with CBSPRenderer::m_pMainBuffer
	GL_BufferHandler* m_pCableVertsBuffer; 
	GL_VertexArrayObject* m_pCableVertsVAO;

	int m_iNumCableVerts;

	unsigned int* m_pIndexBuffer;

	cvar_t* m_pCvarDrawClientEntities;
};

extern CPropManager gPropManager;
#endif