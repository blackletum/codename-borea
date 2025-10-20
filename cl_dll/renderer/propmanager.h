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