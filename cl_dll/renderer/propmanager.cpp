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

#include "PlatformHeaders.h"
#include "hud.h"
#include "cl_util.h"

#include "const.h"
#include "studio.h"
#include "entity_state.h"
#include "triangleapi.h"
#include "event_api.h"
#include "pm_defs.h"

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <math.h>

#include "propmanager.h"
#include "bsprenderer.h"

#include "r_efx.h"
#include "r_studioint.h"
#include "studio_util.h"

#include "textureloader.h"
#include "particle_engine.h"
#include "opengl_utils/GL_Buffers.h"
#include "opengl_utils/GL_StateHandler.h"
#include "opengl_utils/GL_ShaderProgram.h"
#include "opengl_utils/GL_VertexArrayObject.h"

#include "StudioModelRenderer.h"
#include "StudioMDL_MeshGen.h"

CPropManager gPropManager;

modeldata_t* AllocModelHeader()
{
	auto& ptr = gPropManager.m_pHeaders.emplace_back(std::make_unique<modeldata_t>());
	return ptr.get();
}

entextradata_t* AllocExtraData()
{
	auto& ptr = gPropManager.m_pExtraData.emplace_back(std::make_unique<entextradata_t>());
	return ptr.get();
}

entextrainfo_t* AllocExtraInfo()
{
	auto& ptr = gPropManager.m_pExtraInfo.emplace_back(std::make_unique<entextrainfo_t>());
	return ptr.get();
}

/*
====================
Shutdown

====================
*/
void CPropManager::Shutdown(void)
{
	Reset();
}

/*
====================
Reset

====================
*/
void CPropManager::Reset(void)
{
	m_pEntities.clear();

	m_pModelLights.clear();

	m_pDecals.clear();

	m_pExtraData.clear();

	m_pExtraInfo.clear();

	m_pCurrentExtraData = NULL;

	if (!m_pHeaders.empty())
	{
		for (auto &header_ : m_pHeaders)
		{
			auto header = header_.get();

			if (header->pHdr)
			{
				for (int j = 0; j < header->pVBOHeader.numsubmodels; j++)
					delete[] header->pVBOHeader.submodels[j].meshes;

				delete[] header->pVBOHeader.submodels;
			}

			if (header->pVBOHeader.pBufferData)
			{
				delete[] header->pVBOHeader.pBufferData;
				header->pVBOHeader.pBufferData = NULL;
			}

			if (header->pVBOHeader.indexes)
			{
				delete[] header->pVBOHeader.indexes;
				header->pVBOHeader.indexes = NULL;
			}
		}
		m_pHeaders.clear();
	}

	ClearEntityData();

	if (m_pEntData)
	{
		m_pEntData = NULL;
		m_iEntDataSize = NULL;
	}

	if (m_pVertexData)
	{
		delete[] m_pVertexData;
		m_pVertexData = NULL;
		m_iNumTotalVerts = NULL;
	}

	if (m_pIndexBuffer)
	{
		delete[] m_pIndexBuffer;
		m_pIndexBuffer = NULL;
	}

	m_pCables.clear();
}

/*
====================
Init

====================
*/
void CPropManager::Init(void)
{
	m_pCvarDrawClientEntities = CVAR_CREATE("r_drawstudiomdl_staticprops", "1", 0);
}

/*
====================
VidInit

====================
*/
void CPropManager::VidInit(void)
{
	Reset();
}

/*
====================
ClearEntityData

====================
*/
void CPropManager::ClearEntityData(void)
{
	if (m_pBSPEntities.empty())
		return;

	for (int i = m_pBSPEntities.size() - 1; i >= 0; i--)
	{
		epair_t* pPair = m_pBSPEntities[i].epairs;
		while (pPair)
		{
			epair_t* pFree = pPair;
			pPair = pFree->next;

			delete[] pFree->key;
			delete[] pFree->value;
			delete[] pFree;
		}
		m_pBSPEntities.erase(m_pBSPEntities.begin() + i);
	}
}

/*
====================
LoadBSPFile

====================
*/
void CPropManager::GenerateEntityList(void)
{
	// reset all entity data
	Reset();

	// get pointer to world model
	if (!engine_cl->worldmodel)
	{
		gEngfuncs.pfnClientCmd("escape\n");
		MessageBox(NULL, "FATAL ERROR: Failed to get world!\n\nPress Ok to quit the game.\n", "ERROR", MB_OK);
		exit(-1);
	}

	// world model already adds pointer to ent data
	m_iEntDataSize = strlen(engine_cl->worldmodel->entities);
	m_pEntData = engine_cl->worldmodel->entities;

	ParseEntities();
	LoadEntVars();
	SetupVBO();
}

/*
====================
GetHeader

====================
*/
modeldata_t* CPropManager::GetHeader(const char* name)
{
	if (!m_pHeaders.empty())
	{
		for (auto& header : m_pHeaders)
		{
			if (!strcmp(header.get()->name, name))
				return header.get();
		}
	}
	return NULL;
}

/*
====================
ValueForKey

====================
*/
char* CPropManager::ValueForKey(entity_t* ent, const char* key)
{
	for (epair_t* pEPair = ent->epairs; pEPair; pEPair = pEPair->next)
	{
		if (!strcmp(pEPair->key, key))
			return pEPair->value;
	}
	return NULL;
}

/*
====================
ParseEntities

====================
*/
void CPropManager::ParseEntities(void)
{
	// Entity parser done by me, parses nicely, no errors detected ever.
	char* pCurText = m_pEntData;
	while (pCurText && pCurText - m_pEntData < m_iEntDataSize)
	{
		if (m_pBSPEntities.size() == MAXRENDERENTS)
			break;

		while (1)
		{
			if (pCurText[0] == '{')
				break;

			if (pCurText - m_pEntData >= m_iEntDataSize)
				break;

			pCurText++;
		}

		if (pCurText - m_pEntData >= m_iEntDataSize)
			break;

		entity_t pEntitydummy{};
		m_pBSPEntities.emplace_back(pEntitydummy);
		entity_t* pEntity = &m_pBSPEntities[m_pBSPEntities.size() - 1];

		while (1)
		{
			// skip to next token
			while (1)
			{
				if (pCurText[0] == '}')
					break;

				if (pCurText[0] == '"')
				{
					pCurText++;
					break;
				}

				pCurText++;
			}

			// end of ent
			if (pCurText[0] == '}')
				break;

			epair_t* pEPair = new epair_t;
			memset(pEPair, 0, sizeof(epair_t));

			if (pEntity->epairs)
				pEPair->next = pEntity->epairs;

			pEntity->epairs = pEPair;

			int iLength = 0;
			char* pTemp = pCurText;
			while (1)
			{
				if (pTemp[0] == '"')
					break;

				if (pCurText[0] == '}')
				{
					gEngfuncs.Con_Printf("BSP LOADER ERROR :: Entity data is corrupt!\n");
					m_bAvailable = false;
					return;
				}

				iLength++;
				pTemp++;
			}

			pEPair->key = new char[iLength + 1];
			pEPair->key[iLength] = NULL; // terminator

			memcpy(pEPair->key, pCurText, sizeof(char) * iLength);
			pCurText += iLength + 1;

			// skip to next token
			while (1)
			{
				if (pCurText[0] == '}')
				{
					gEngfuncs.Con_Printf("BSP LOADER ERROR :: Entity data is corrupt!\n");
					m_bAvailable = false;
					return;
				}

				if (pCurText[0] == '"')
				{
					pCurText++;
					break;
				}

				pCurText++;
			}

			iLength = 0;
			pTemp = pCurText;
			while (1)
			{
				if (pCurText[0] == '}')
				{
					gEngfuncs.Con_Printf("BSP LOADER ERROR :: Entity data is corrupt!\n");
					m_bAvailable = false;
					return;
				}

				if (pTemp[0] == '"')
					break;

				iLength++;
				pTemp++;
			}

			pEPair->value = new char[iLength + 1];
			pEPair->value[iLength] = NULL;

			memcpy(pEPair->value, pCurText, sizeof(char) * iLength);
			pCurText += iLength + 1;
		}
	}

	// Get sky name for bsp renderer
	char* szSky = ValueForKey(&m_pBSPEntities[0], "skyname");

	if (szSky)
		strcpy(gBSPRenderer.m_szSkyName, szSky);
	else
		sprintf(gBSPRenderer.m_szSkyName, "desert");
}

/*
====================
LoadEntVars

====================
*/
void CPropManager::LoadEntVars(void)
{
	for (auto& bspent : m_pBSPEntities)
	{
		char* pValue = ValueForKey(&bspent, "classname");

		cl_entity_t modellight{};

		if (!pValue)
			continue;

		if (!strcmp(pValue, "env_elight"))
		{
			pValue = ValueForKey(&bspent, "targetname");

			if (pValue)
				continue;

			memset(&modellight, 0, sizeof(cl_entity_t));

			pValue = ValueForKey(&bspent, "origin");
			if (pValue)
			{
				sscanf(pValue, "%f %f %f", &modellight.origin[0],
					&modellight.origin[1],
					&modellight.origin[2]);

				VectorCopy(modellight.origin, modellight.curstate.origin);
			}

			pValue = ValueForKey(&bspent, "renderamt");
			if (pValue)
			{
				sscanf(pValue, "%d", &modellight.curstate.renderamt);
			}

			pValue = ValueForKey(&bspent, "rendercolor");
			if (pValue)
			{
				int iColR, iColG, iColB;
				sscanf(pValue, "%d %d %d", &iColR, &iColG, &iColB);
				modellight.curstate.rendercolor.r = iColR;
				modellight.curstate.rendercolor.g = iColG;
				modellight.curstate.rendercolor.b = iColB;
			}

			model_t* pWorld = engine_cl->worldmodel;
			mleaf_t* pLeaf = Mod_PointInLeaf(modellight.origin, pWorld);

			if (pLeaf)
			{
				// In-void entities can go eat a dick
				modellight.visframe = pLeaf - pWorld->leafs - 1;
				m_pModelLights.emplace_back(modellight);
			}
		}
		if (!strcmp(pValue, "env_cable"))
		{
			cabledata_t cabledata;
			if (SetupCable(&cabledata, &bspent))
			{
				m_pCables.emplace_back(cabledata);
			}
		}
		else if (!strcmp(pValue, "env_decal"))
		{
			pValue = ValueForKey(&bspent, "targetname");

			if (pValue)
				continue;

			decal_msg_cache cachedecal{};

			// Always TRUE
			cachedecal.persistent = TRUE;

			pValue = ValueForKey(&bspent, "origin");
			if (pValue)
			{
				sscanf(pValue, "%f %f %f", &cachedecal.pos[0],
					&cachedecal.pos[1],
					&cachedecal.pos[2]);
			}

			pValue = ValueForKey(&bspent, "message");

			if (!pValue)
				continue;

			if (!strlen(pValue))
				continue;

			strcpy(cachedecal.name, pValue);
			m_pDecals.emplace_back(cachedecal);
		}
		else if (!strcmp(pValue, "item_generic") || !strcmp(pValue, "prop_static") || !strcmp(pValue, "prop_grass"))
		{
			bool isfoliage = !strcmp(pValue, "prop_grass");
			pValue = ValueForKey(&bspent, "targetname");

			if (pValue)
				continue;

			pValue = ValueForKey(&bspent, "parentname");

			if (pValue)
				continue;

			pValue = ValueForKey(&bspent, "model");

			if (!pValue)
				continue;

			if (!stristr(pValue, ".mdl"))
				continue;


			m_pCurrentExtraData = AllocExtraData();
			entextrainfo_t* pExtraInfo = AllocExtraInfo();

			cl_entity_t propentity{};

			if (!LoadMDL(pValue, &propentity, &bspent))
			{
				gEngfuncs.Con_Printf("BSP Loader: Failed to model load %s on the client!\n", pValue);
				continue;
			}

			memset(&propentity, 0, sizeof(cl_entity_t));
			propentity.index = m_pEntities.size() + 4096;
			propentity.topnode = (struct mnode_s*)pExtraInfo;
			propentity.visframe = -1;

			pExtraInfo->pExtraData = m_pCurrentExtraData;
			if (isfoliage)
			{
				pExtraInfo->prop_flags |= PROPFLAG_FOLIAGE;
				propentity.curstate.fuser3 = gEngfuncs.pfnRandomLong(1, 25);
				propentity.curstate.fuser4 = gEngfuncs.pfnRandomLong(1, 25);
			}

			pValue = ValueForKey(&bspent, "origin");
			if (pValue)
			{
				sscanf(pValue, "%f %f %f", &propentity.origin[0],
					&propentity.origin[1],
					&propentity.origin[2]);

				VectorCopy(propentity.origin, propentity.curstate.origin);
			}

			pValue = ValueForKey(&bspent, "angles");
			if (pValue)
			{
				// set the yaw angle...
				sscanf(pValue, "%f %f %f", &propentity.angles[0],
					&propentity.angles[1],
					&propentity.angles[2]);
				propentity.curstate.angles = propentity.angles;
			}

			pValue = ValueForKey(&bspent, "renderamt");
			if (pValue)
			{
				sscanf(pValue, "%d", &propentity.curstate.renderamt);
			}

			pValue = ValueForKey(&bspent, "sequence");

			if (pValue)
				sscanf(pValue, "%d", &propentity.curstate.sequence);

			pValue = ValueForKey(&bspent, "body");

			if (pValue)
				sscanf(pValue, "%d", &propentity.curstate.body);

			pValue = ValueForKey(&bspent, "skin");

			if (pValue)
				sscanf(pValue, "%d", &propentity.curstate.skin);


			pValue = ValueForKey(&bspent, "scale");

			if (pValue)
				sscanf(pValue, "%f", &propentity.curstate.scale);


			pValue = ValueForKey(&bspent, "renderfx");

			if (pValue)
				sscanf(pValue, "%d", &propentity.curstate.renderfx);

			pValue = ValueForKey(&bspent, "DisableShadows");

			if (pValue)
				propentity.curstate.iuser2 = FL_NOSHADOW;

			pValue = ValueForKey(&bspent, "rendercolor");
			if (pValue)
			{
				int iColR, iColG, iColB;
				sscanf(pValue, "%d %d %d", &iColR, &iColG, &iColB);
				propentity.curstate.rendercolor.r = iColR;
				propentity.curstate.rendercolor.g = iColG;
				propentity.curstate.rendercolor.b = iColB;
			}

			pValue = ValueForKey(&bspent, "lightorigin");
			if (pValue && strlen(pValue))
			{
				char szLightTarget[32];
				strcpy(szLightTarget, pValue);

				int j = 0;
				for (auto bspent2 : m_pBSPEntities)
				{
					pValue = ValueForKey(&m_pBSPEntities[j], "classname");

					if (strcmp(pValue, "info_light_origin"))
					{
						j++;
						continue;
					}

					pValue = ValueForKey(&m_pBSPEntities[j], "targetname");

					if (pValue)
					{
						if (!strcmp(pValue, szLightTarget))
						{
							pValue = ValueForKey(&m_pBSPEntities[j], "origin");
							if (pValue)
							{
								sscanf(pValue, "%f %f %f", &m_pCurrentExtraData->lightorigin[0],
									&m_pCurrentExtraData->lightorigin[1],
									&m_pCurrentExtraData->lightorigin[2]);

								break;
							}
						}
					}
				}

				if (j == m_pBSPEntities.size())
				{
					m_pCurrentExtraData->lightorigin = propentity.origin;
				}
			}
			else
			{
				m_pCurrentExtraData->lightorigin = propentity.origin;
			}

			g_StudioRenderer.m_bExternalEntity = true;
			m_pEntities.emplace_back(propentity);
			g_StudioRenderer.m_pCurrentEntity = &m_pEntities[m_pEntities.size() - 1];
			g_StudioRenderer.m_pStudioHeader = m_pCurrentExtraData->pModelData->pHdr;
			g_StudioRenderer.m_pCurrentStudioMDL = m_pCurrentExtraData->pModelData->pCacheModel;

			g_StudioRenderer.StudioSaveUniqueData(m_pCurrentExtraData);
		}
	}
}

/*
====================
SetupVBO

====================
*/
void CPropManager::SetupVBO(void)
{
	if (m_pStaticModelBuffer)
		delete m_pStaticModelBuffer;

	if (m_pStaticModelVAO)
		delete m_pStaticModelVAO;

	m_pStaticModelBuffer = nullptr;
	
	m_pStaticModelVAO = nullptr;

	if (m_pHeaders.empty())
		return;

	m_iNumTotalVerts = NULL;

	int iTotalIndexes = 0;

	for (auto &header_ : m_pHeaders)
	{
		modeldata_t* header = header_.get();
		m_iNumTotalVerts += header->pVBOHeader.numverts;
		iTotalIndexes += header->pVBOHeader.numindexes;
	}

	m_pVertexData = new brushvertex_t[m_iNumTotalVerts];
	memset(m_pVertexData, 0, sizeof(brushvertex_t) * m_iNumTotalVerts);

	m_pIndexBuffer = new unsigned int[iTotalIndexes];
	memset(m_pIndexBuffer, 0, sizeof(unsigned int) * iTotalIndexes);

	int iVertexOffset = 0;
	int iIndexOffset = 0;
	for (auto &header_ : m_pHeaders)
	{
		modeldata_t* header = header_.get();

		memcpy(&m_pVertexData[iVertexOffset], header->pVBOHeader.pBufferData,
			sizeof(brushvertex_t) * header->pVBOHeader.numverts);

		for (int j = 0; j < header->pVBOHeader.numindexes; j++)
			header->pVBOHeader.indexes[j] += iVertexOffset;

		memcpy(&m_pIndexBuffer[iIndexOffset], header->pVBOHeader.indexes,
			sizeof(unsigned int) * header->pVBOHeader.numindexes);

		for (int j = 0; j < header->pVBOHeader.numsubmodels; j++)
		{
			for (int k = 0; k < header->pVBOHeader.submodels[j].nummeshes; k++)
			{
				header->pVBOHeader.submodels[j].meshes[k].start_vertex += iIndexOffset;
			}
		}

		iVertexOffset += header->pVBOHeader.numverts;
		iIndexOffset += header->pVBOHeader.numindexes;
	}

	m_pStaticModelVAO = new GL_VertexArrayObject();

	m_pStaticModelBuffer = new GL_BufferHandler();
	m_pStaticModelBuffer->Bind(GL_BufferHandler::ElementArrayBuffer);
	m_pStaticModelBuffer->BufferData(GL_BufferHandler::ElementArrayBuffer, iTotalIndexes * sizeof(unsigned int), m_pIndexBuffer, GL_BufferHandler::StaticDraw);

	//we set up m_pStaticModelVAO in CBSPRenderer::GenerateVertexArray() since we need m_pMainBuffer

	GL_BufferHandler::ResetBufferBinding(GL_BufferHandler::ElementArrayBuffer);
}

void CPropManager::HandleFoliageProp(cl_entity_s* ent, entextradata_t* extradata)
{
	// bacontsu - interactive grass
	#define GRASS_SWAY_RADIUS 500.0f
	#define GRASS_RADIUS 40.0f
	#define GRASS_ANGLE 15.0f

	cl_entity_t* closest_player = nullptr;
	float closest_distance = 0;
	Vector dir;
	for (int i = 0; i < engine_cl->num_entities; i++)
	{
		cl_entity_t* _entity = gEngfuncs.GetEntityByIndex(i);
		if (!_entity)
			break;

		if (!_entity->player)
			continue;

		dir = (ent->origin - _entity->origin);
		float distance = dir.Length2D();
		if (distance > GRASS_SWAY_RADIUS)
			continue;

		if (closest_distance < distance)
		{
			closest_distance = distance;
		}
		else
		{
			continue;
		}

		closest_player = _entity;
	}

	if (!closest_player)
	{
		ent->curstate.angles.x = ent->baseline.angles.x = 0;// = lerp(m_pEntities[i].curstate.angles.x, sin(gEngfuncs.GetAbsoluteTime() + m_pEntities[i].curstate.fuser3) * 2.5f, gHUD.m_flTimeDelta * 10.0f);
		ent->curstate.angles.z = ent->baseline.angles.z = 0; // lerp(m_pEntities[i].curstate.angles.x, cos(gEngfuncs.GetAbsoluteTime() + m_pEntities[i].curstate.fuser4) * 2.5f, gHUD.m_flTimeDelta * 10.0f);;
		ent->curstate.angles.y = 0;
	}
	else if(closest_distance < GRASS_RADIUS)
	{

		// clamps
		float distX = std::clamp(dir.x, -GRASS_RADIUS, GRASS_RADIUS);
		float distY = std::clamp(dir.y, -GRASS_RADIUS, GRASS_RADIUS);

		if (distX > 0)
		{
			distX = GRASS_RADIUS - distX;
		}
		else if (distX < 0)
		{
			distX = -GRASS_RADIUS - distX;
		}

		if (distY > 0)
		{
			distY = GRASS_RADIUS - distY;
		}
		else if (distY < 0)
		{
			distY = -GRASS_RADIUS - distY;
		}

		ent->curstate.angles.x = ent->baseline.angles.x - (distX * GRASS_ANGLE / GRASS_RADIUS);
		ent->curstate.angles.z = ent->baseline.angles.z - (distY * GRASS_ANGLE / GRASS_RADIUS);
		ent->curstate.angles.y = 0;
	}
	else if (closest_distance < GRASS_SWAY_RADIUS)
	{
		ent->curstate.angles.x = ent->baseline.angles.x = lerp(ent->curstate.angles.x, sin(engine_cl->time + ent->curstate.fuser3) * 4.0f, gHUD.m_flTimeDelta * 10.0f);
		ent->curstate.angles.z = ent->baseline.angles.z = lerp(ent->curstate.angles.x, cos(engine_cl->time + ent->curstate.fuser4) * 4.0f, gHUD.m_flTimeDelta * 10.0f);;
		ent->curstate.angles.y = 0;
	}

	for (int j = 0; j < 3; j++)
	{
		ent->angles[j] = lerp(ent->angles[j], ent->curstate.angles[j], gHUD.m_flTimeDelta * 5);
	}

	glm::vec3 entityangles = glm::vec3(-ent->angles.x, ent->angles.y, ent->angles.z);

	float scale = ent->curstate.scale ? ent->curstate.scale : 1;

	glm::mat4 modelview = glm::translate(glm::mat4(1.0f), glm::vec3(ent->origin.x, ent->origin.y, ent->origin.z));
	modelview = glm::rotate(modelview, glm::radians(entityangles.y), glm::vec3(0.0f, 0.0f, 1.0f));
	modelview = glm::rotate(modelview, glm::radians(entityangles.x), glm::vec3(0.0f, 1.0f, 0.0f));
	modelview = glm::rotate(modelview, glm::radians(entityangles.z), glm::vec3(1.0f, 0.0f, 0.0f));
	modelview = glm::scale(modelview, glm::vec3(scale));

	extradata->modelmatrix = modelview;
}

/*
====================
RenderModels

====================
*/
void CPropManager::RenderProps(bool bSkybox)
{
	if (m_pCvarDrawClientEntities->value < 1)
		return;

	if (g_StudioRenderer.m_pCvarDrawStudioModels->value < 1)
		return;

	if (g_StudioRenderer.m_pCvarDrawEntities->value < 1)
		return;

	if (m_pCvarDrawClientEntities->value == 2)
		g_GlobalGLState.SetDepthTest(false);

	if (m_pStaticModelVAO)
		m_pStaticModelVAO->BindVAO();
	else
		return;

	g_StudioRenderer.m_ModelShader->Bind();

	g_StudioRenderer.m_bExternalEntity = true;

	cl_entity_t* ents = m_pEntities.data();

	for (int i = 0; i < m_pEntities.size(); i++, ents++)
	{

		entextrainfo_t* pExtraInfo = ((entextrainfo_t*)ents->topnode);
		entextradata_t* pExtraData = pExtraInfo->pExtraData;


		if (!pExtraData)
			return;

		int j = 0;
		for (; j < pExtraData->num_leafs; j++)
			if (gBSPRenderer.IsInPotentiallyVisibleSet(pExtraData->leafnums[j]))
				break;

		if (j == pExtraData->num_leafs)
			continue;

		if (pExtraInfo->prop_flags & PROPFLAG_FOLIAGE)
			HandleFoliageProp(ents, pExtraData);

		g_StudioRenderer.StudioDrawExternalEntity(ents, bSkybox);
	}

	if (m_pCvarDrawClientEntities->value == 2)
		g_GlobalGLState.SetDepthTest(true);

	g_StudioRenderer.m_bExternalEntity = true;

	GL_VertexArrayObject::ResetVAOBinding();
}

/*
====================
LoadMDL

====================
*/
bool CPropManager::LoadMDL(const char* name, cl_entity_t* pEntity, entity_t* pBSPEntity)
{
	if (m_pCurrentExtraData->pModelData = GetHeader(name))
		return true;

	if (m_pHeaders.size() == MAXRENDERENTS)
	{
		gEngfuncs.Con_Printf("BSP Loader: The client side limit of 4096 models has been reached!\n Not caching.\n");
		return false;
	}

	model_t* pModel = IEngineStudio.Mod_ForName(name, false);

	if (!pModel)
		return false;

	modeldata_t* modelheader = AllocModelHeader();

	modelheader->pHdr = (studiohdr_t*)pModel->cache.data;
	if (!pModel->entities)
	{
		pModel->entities = (char*)(new StudioMDL_Model(pModel));
	}

	modelheader->pCacheModel = (StudioMDL_Model*)pModel->entities;

	strcpy(modelheader->name, name);

	if (m_pHeaders.size() == MAXRENDERENTS)
	{
		gEngfuncs.Con_Printf("BSP Loader: The client side limit of 4096 models has been reached!\n Not caching.\n");
		return false;
	}

	// not very nice, but we don't support animations anyway
	cl_entity_t* pTempEnt = new cl_entity_t;
	memset(pTempEnt, 0, sizeof(cl_entity_t));
	model_t* pTempModel = new model_t;
	memset(pTempModel, 0, sizeof(model_t));
	strcpy(pTempModel->name, name);

	g_StudioRenderer.m_bExternalEntity = true;
	g_StudioRenderer.m_pCurrentEntity = pTempEnt;
	g_StudioRenderer.m_pStudioHeader = modelheader->pHdr;
	g_StudioRenderer.m_pCurrentStudioMDL = (StudioMDL_Model*)pModel->entities;
	g_StudioRenderer.m_pRenderModel = pTempModel;

	g_StudioRenderer.StudioSetUpTransform(0);
	g_StudioRenderer.StudioSetupBones();

	g_StudioRenderer.StudioSaveModelData(modelheader);

	delete[] pTempEnt;
	delete[] pTempModel;

	m_pCurrentExtraData->pModelData = modelheader;
	return true;
}

/*
====================
SetupCable

====================
*/
bool CPropManager::SetupCable(cabledata_t* cable, entity_t* entity)
{
	char sz[64];
	Vector vdroppoint;
	Vector vposition1;
	Vector vposition2;
	Vector vdirection;
	Vector vmidpoint;
	Vector vendpoint;

	// Get our origin
	char* pValue = ValueForKey(entity, "origin");

	if (!pValue)
		return false;

	sscanf(pValue, "%f %f %f", &vposition1[0], &vposition1[1], &vposition1[2]);

	// Find our target entity
	pValue = ValueForKey(entity, "target");

	if (!pValue)
		return false;

	strcpy(sz, pValue);

	for (auto& bspent : m_pBSPEntities)
	{
		pValue = ValueForKey(&bspent, "targetname");

		if (!pValue)
			continue;

		if (!strcmp(pValue, sz))
		{
			pValue = ValueForKey(&bspent, "origin");

			if (!pValue)
				return false;

			// Copy origin over
			sscanf(pValue, "%f %f %f", &vposition2[0], &vposition2[1], &vposition2[2]);
		}
	}

	// Get our falling depth
	pValue = ValueForKey(entity, "falldepth");

	if (!pValue)
		return false;

	// Calculate dropping point
	VectorSubtract(vposition2, vposition1, vdirection);
	VectorMA(vposition1, 0.5, vdirection, vmidpoint);
	vdroppoint = Vector(vmidpoint[0], vmidpoint[1], vmidpoint[2] - atoi(pValue));

	// Get sprite width
	pValue = ValueForKey(entity, "spritewidth");

	if (!pValue)
		return false;

	cable->iwidth = atoi(pValue);

	// Get segment count
	pValue = ValueForKey(entity, "segments");

	if (!pValue)
		return false;

	cable->isegments = atoi(pValue);
	cable->inumpoints = cable->isegments + 1;

	cable->vmins = Vector(4096, 4096, 4096);
	cable->vmaxs = Vector(-4096, -4096, -4096);

	for (int i = 0; i < cable->inumpoints; i++)
	{
		float f = (float)i / (float)cable->isegments;
		cable->vpoints[i][0] = vposition1[0] * ((1 - f) * (1 - f)) + vdroppoint[0] * ((1 - f) * f * 2) + vposition2[0] * (f * f);
		cable->vpoints[i][1] = vposition1[1] * ((1 - f) * (1 - f)) + vdroppoint[1] * ((1 - f) * f * 2) + vposition2[1] * (f * f);
		cable->vpoints[i][2] = vposition1[2] * ((1 - f) * (1 - f)) + vdroppoint[2] * ((1 - f) * f * 2) + vposition2[2] * (f * f);

		for (int j = 0; j < 3; j++)
		{
			if ((cable->vpoints[i][j] + cable->iwidth) > cable->vmaxs[j])
				cable->vmaxs[j] = cable->vpoints[i][j] + cable->iwidth;

			if ((cable->vpoints[i][j] - cable->iwidth) > cable->vmaxs[j])
				cable->vmaxs[j] = cable->vpoints[i][j] - cable->iwidth;

			if ((cable->vpoints[i][j] + cable->iwidth) < cable->vmins[j])
				cable->vmins[j] = cable->vpoints[i][j] + cable->iwidth;

			if ((cable->vpoints[i][j] - cable->iwidth) < cable->vmins[j])
				cable->vmins[j] = cable->vpoints[i][j] - cable->iwidth;
		}
	}

	entextradata_t pdata;
	memset(&pdata, 0, sizeof(pdata));

	VectorCopy(cable->vmaxs, pdata.absmax);
	VectorCopy(cable->vmins, pdata.absmin);
	SV_FindTouchedLeafs(&pdata, engine_cl->worldmodel->nodes);

	memcpy(cable->leafnums, pdata.leafnums, sizeof(short) * MAX_ENT_LEAFS);
	cable->num_leafs = pdata.num_leafs;

	return true;
}

/*
====================
DrawCables

====================
*/
void CPropManager::DrawCables(void)
{
	Vector vVertex;
	Vector vTangent;
	Vector vDir;
	Vector vRight;

	if (!m_pCvarDrawClientEntities->value || m_pCables.empty())
		return;

	g_GlobalGLState.SetCullFace(false);

	for (auto cable : m_pCables)
	{
		int j = 0;
		for (; j < cable.num_leafs; j++)
			if (gBSPRenderer.IsInPotentiallyVisibleSet(cable.leafnums[j]))
				break;

		if (j == cable.num_leafs)
			continue;

		if (gHUD.viewFrustum.CullBox(cable.vmins, cable.vmaxs))
			continue;

		glBegin(GL_TRIANGLE_STRIP);
		for (int j = 0; j < cable.inumpoints; j++)
		{
			if (j == 0)
			{
				VectorSubtract(cable.vpoints[0], cable.vpoints[1], vTangent);
			}
			else
			{
				VectorSubtract(cable.vpoints[0], cable.vpoints[j], vTangent);
			}

			VectorSubtract(cable.vpoints[j], gBSPRenderer.m_vRenderOrigin, vDir);
			CrossProduct(vTangent, -vDir, vRight);
			vRight = vRight.Normalize();

			glColor3f(GL_ZERO, GL_ZERO, GL_ZERO);
			VectorMA(cable.vpoints[j], cable.iwidth, vRight, vVertex);
			glVertex3fv(vVertex);

			VectorMA(cable.vpoints[j], -cable.iwidth, vRight, vVertex);
			glVertex3fv(vVertex);
		}
		glEnd();
	}

	g_GlobalGLState.SetCullFace(true);
}

/*
====================
RenderPropsSolid

====================
*/
void CPropManager::RenderPropsSolid(void)
{
	if (m_pCvarDrawClientEntities->value < 1)
		return;

	if (g_StudioRenderer.m_pCvarDrawStudioModels->value < 1)
		return;

	if (g_StudioRenderer.m_pCvarDrawEntities->value < 1)
		return;

	if (m_pStaticModelVAO)
		m_pStaticModelVAO->BindVAO();
	else
		return;

	g_StudioRenderer.m_ModelSolidShader->Bind();

	g_StudioRenderer.m_dSolidModelData.projviewmatrix = gBSPRenderer.m_ProjectionMatrix * gBSPRenderer.m_ViewMatrix;

	auto dynl = gBSPRenderer.m_pCurrentDynLight;
	g_StudioRenderer.m_dSolidModelData.light_pos = glm::vec4(dynl->origin.x, dynl->origin.y, dynl->origin.z, dynl->radius);
	g_StudioRenderer.m_dSolidModelData.int_values.x = 1;

	g_StudioRenderer.m_ModelSolid_Buffer->Bind(GL_BufferHandler::UniformBuffer);

	g_StudioRenderer.m_bExternalEntity = true;


	if (gBSPRenderer.m_bSunShadowMapPass)
	{
		// flip
		g_StudioRenderer.m_ModelSolidShader->Uniform1i(g_StudioRenderer.m_ModelShaderSolidLocs[CStudioModelRenderer::mdlshadersolid_sunshadow], 1);
		glCullFace(GL_BACK);
	}

	cl_entity_t* ents = m_pEntities.data();

	for (int i = 0; i < m_pEntities.size(); i++, ents++)
	{

		entextradata_t* pExtraData = ((entextrainfo_t*)ents->topnode)->pExtraData;

		if (!pExtraData)
			return;

		int j = 0;
		for (; j < pExtraData->num_leafs; j++)
			if (gBSPRenderer.IsInPotentiallyVisibleSet(pExtraData->leafnums[j]))
				break;

		if (j == pExtraData->num_leafs)
			continue;

		g_StudioRenderer.StudioDrawExternalEntitySolid(ents);
	}

	if (gBSPRenderer.m_bSunShadowMapPass)
	{
		// flip
		g_StudioRenderer.m_ModelSolidShader->Uniform1i(g_StudioRenderer.m_ModelShaderSolidLocs[CStudioModelRenderer::mdlshadersolid_sunshadow], 0);
		glCullFace(GL_FRONT);
	}

	g_StudioRenderer.m_dSolidModelData.int_values.x = 0;
	g_StudioRenderer.m_bExternalEntity = false;

	GL_VertexArrayObject::ResetVAOBinding();
}