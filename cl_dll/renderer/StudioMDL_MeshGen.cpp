
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

#include "StudioMDL_MeshGen.h"
#include "opengl_utils/GL_Buffers.h"
#include "opengl_utils/GL_ShaderProgram.h"
#include "opengl_utils/GL_VertexArrayObject.h"

#include <unordered_map>

std::vector<StudioMDL_Model*> StudioMDL_Model::m_vCachedStudioModels;

StudioMDL_Model::StudioMDL_Model(model_t* model)
{
	m_vCachedStudioModels.push_back(this);

	m_pEngineModel = model;
	m_pStudioHeader = (studiohdr_t*)model->cache.data;
	m_iNumBones = m_pStudioHeader->numbones;

	CheckExternTextures();

	mstudiobodyparts_t* bp = (mstudiobodyparts_t*)((byte*)m_pStudioHeader + m_pStudioHeader->bodypartindex);
	mstudiotexture_t* tex = (mstudiotexture_t*)((byte*)m_pStudioHeader + m_pStudioHeader->textureindex);

	studiohdr_t* textureheader = nullptr;

	if (m_pStudioHeader->textureindex)
	{
		textureheader = m_pStudioHeader;
	}
	else
	{
		// CheckExternTextures() has conveniently loaded these in for us already
		textureheader = (studiohdr_t*)((model_t*)m_pEngineModel->texinfo)->cache.data;
		tex = (mstudiotexture_t*)((byte*)textureheader + textureheader->textureindex);
	}

	short* skinindexes = (short*)((byte*)textureheader + textureheader->skinindex);
	m_iSkinFamilies = textureheader->numskinfamilies;

	for (int i = 0; i < textureheader->numtextures; i++)
	{

		m_vTextures.push_back(new StudioMDL_Texture(&tex[i]));
	}

	for (int i = 0; i < textureheader->numskinref * m_iSkinFamilies; i++)
	{
		m_vSkinIndexes.push_back(skinindexes[i]);
	}

	m_iNumTextures = m_vTextures.size();
	m_iNumSkinIndexes = static_cast<short>(m_vSkinIndexes.size() / m_iSkinFamilies);

	for (int i = 0; i < m_pStudioHeader->numbodyparts; i++)
	{
		m_vBodyParts.push_back(new StudioMDL_BodyPart(bp[i], this->m_pStudioHeader, this));
	}

	CheckCustomMDLData();

	m_iNumBodyParts = m_vBodyParts.size();

	model->entities = (char*)this;

	//
	//generate opengl mesh buffer
	//

	m_pModelVAO = new GL_VertexArrayObject();
	m_pModelVAO->BindVAO();

	m_pModelVertBuffer = new GL_BufferHandler();
	m_pModelVertBuffer->Bind(GL_BufferHandler::ArrayBuffer);
	m_pModelVertBuffer->BufferData(GL_BufferHandler::ArrayBuffer, m_vTotalVerts.size() * sizeof(studiomdl_vertbufferdata_t), m_vTotalVerts.data(), GL_BufferHandler::StaticDraw);

	m_pModelVertIndexBuffer = new GL_BufferHandler();
	m_pModelVertIndexBuffer->Bind(GL_BufferHandler::ElementArrayBuffer);
	m_pModelVertIndexBuffer->BufferData(GL_BufferHandler::ElementArrayBuffer, m_vTotalIndices.size() * sizeof(uint32_t), m_vTotalIndices.data(), GL_BufferHandler::StaticDraw);

	glEnableVertexAttribArray(GL_ShaderProgram::ShaderAttribs::VertexPos);
	glVertexAttribPointer(GL_ShaderProgram::ShaderAttribs::VertexPos, 3, GL_FLOAT, GL_FALSE, sizeof(studiomdl_vertbufferdata_t), (const void*)offsetof(studiomdl_vertbufferdata_t, pos));

	glEnableVertexAttribArray(GL_ShaderProgram::ShaderAttribs::Normal);
	glVertexAttribPointer(GL_ShaderProgram::ShaderAttribs::Normal, 3, GL_SHORT, GL_TRUE, /*GL_FLOAT, GL_FALSE,*/ sizeof(studiomdl_vertbufferdata_t), (const void*)offsetof(studiomdl_vertbufferdata_t, normal));

	glEnableVertexAttribArray(GL_ShaderProgram::ShaderAttribs::TexCoord);
	glVertexAttribPointer(GL_ShaderProgram::ShaderAttribs::TexCoord, 2, GL_SHORT, GL_TRUE, sizeof(studiomdl_vertbufferdata_t), (const void*)offsetof(studiomdl_vertbufferdata_t, texcoord));

	glEnableVertexAttribArray(GL_ShaderProgram::ShaderAttribs::StudioMDL_BoneID);
	glVertexAttribIPointer(GL_ShaderProgram::ShaderAttribs::StudioMDL_BoneID, 1, GL_UNSIGNED_INT, sizeof(studiomdl_vertbufferdata_t), (const void*)offsetof(studiomdl_vertbufferdata_t, bonedata));

	GL_VertexArrayObject::ResetVAOBinding();

	GL_BufferHandler::ResetBufferBinding(GL_BufferHandler::ArrayBuffer);
	GL_BufferHandler::ResetBufferBinding(GL_BufferHandler::ElementArrayBuffer);


}

extern char* UTIL_VarArgs_client(const char* format, ...);

void StudioMDL_Model::CheckCustomMDLData()
{
	std::string path;
	path += gEngfuncs.pfnGetGameDirectory();
	path += '/';
	path += m_pEngineModel->name;

	if (!g_pFileSystem->FileExists(path.c_str()))
		return; // dont bother, valve models have no custom data header duh


	std::vector<std::byte> data = FileSystem_LoadFileIntoBuffer(path.c_str(), FileContentFormat::Binary);

	const size_t header_size = sizeof("customdataheader");
	const size_t offset_size = sizeof(long);

	size_t filepointer = data.size() - header_size;

	char headername[17] = {};
	memcpy(headername, &data[filepointer], sizeof(headername));
	if (strcmp(headername, "customdataheader") != 0)
	{
		return; // Invalid header
	}

	//salsatobias: this will be used for my own little mod

	//m_studiocustomdata mdldata{};
	//
	//long datastart_offset;
	//
	//filepointer -= offset_size;
	//memcpy(&datastart_offset, &data[filepointer], sizeof(long));
	//
	//filepointer = datastart_offset;
	//
	//
	//memcpy(&mdldata.numeyes, &data[filepointer], sizeof(int));
	//filepointer += sizeof(int);
	//memcpy(&mdldata.numcdpaths, &data[filepointer], sizeof(int));
	//filepointer += sizeof(int);
	//
	//
	//if (mdldata.numeyes > 0)
	//{
	//	size_t size = mdldata.numeyes;
	//	mdldata.eyedata.resize(size);
	//	memcpy((char*)(mdldata.eyedata.data()), &data[filepointer], mdldata.numeyes * sizeof(m_eyedata));
	//
	//	filepointer += mdldata.numeyes * sizeof(m_eyedata);
	//}
	//
	//if (mdldata.numcdpaths > 0)
	//{
	//	mdldata.cdmaterials.resize(mdldata.numcdpaths);
	//	memcpy((char*)(mdldata.cdmaterials.data()), &data[filepointer], mdldata.numcdpaths * sizeof(m_cdmaterial));
	//
	//	filepointer += mdldata.numcdpaths * sizeof(m_cdmaterial);
	//}
	//
	//for (auto cdpath : mdldata.cdmaterials)
	//{
	//	for (int k = 0; k < m_vTextures.size(); k++)
	//	{
	//		char szTexture[64];
	//		char szPath[512];
	//
	//		auto texinfo = m_vTextures[k]->GetTextureInfo();
	//
	//		std::string path = cdpath.cdmaterials;
	//		std::replace(path.begin(), path.end(), '\\', '/');
	//		std::transform(path.begin(), path.end(),
	//			path.begin(), [](unsigned char c)
	//			{ return std::tolower(c); });
	//
	//		FilenameFromPath(texinfo.szName, szTexture);
	//		strLower(szTexture);
	//		sprintf(szPath, "materials/%s%s.dds", path.c_str(), szTexture);
	//		cl_texture_t* material = gTextureLoader.LoadTexture(szPath, 0, false, m_vTextures[k]->GetTextureFlags() & STUDIO_NF_NOMIPMAP ? 1 : 0);
	//		if (material)
	//		{
	//			// dont use new texture resolutions it messes up stuff
	//			material->iWidth = texinfo.iWidth;
	//			material->iHeight = texinfo.iHeight;
	//			strcpy(material->szName, texinfo.szName);
	//			m_vTextures[k]->ReplaceMDLTexture(material, m_vTextures[k]->GetTextureFlags());
	//
	//			gEngfuncs.Con_Printf("[STUDIOMDL_GEN] loaded custom material texture \"%s\" for model \"%s\".\n", szPath, this->m_pEngineModel->name);
	//		}
	//	}
	//}
	//
	//for (auto eyes : mdldata.eyedata)
	//{
	//	m_vEyePositions.push_back(eyes);
	//}
}

void StudioMDL_Model::CheckExternTextures()
{
	if (!m_pStudioHeader->textureindex)
	{
		//	stupid fucking t.mdl files.

		//	for some reason goldsrc only loads external model texture files in the
		//	middle of rendering models, and since we never let goldsrc render our models,
		//	we have to load them manually.
		//	this is 1:1 to how goldsrc loads these models, see "studiohdr_t* R_LoadTextures(model_t *psubmodel)"

		model_t* texture_model = (model_t*)m_pEngineModel->texinfo;
		if (!texture_model || !IEngineStudio.Cache_Check(&texture_model->cache))
		{

			std::string modelname = m_pEngineModel->name;
			modelname.resize(modelname.size() - 4);
			modelname += "T.mdl";

			model_t* modelfile = IEngineStudio.Mod_ForName(modelname.c_str(), 1);
			m_pEngineModel->texinfo = (mtexinfo_t*)modelfile;
			texture_model = modelfile;

			gEngfuncs.Con_Printf("[STUDIOMDL_GEN] loaded external texture model \"%s\". \n", modelname.c_str());
		}
	}
}

StudioMDL_BodyPart::StudioMDL_BodyPart(mstudiobodyparts_t bodypart, studiohdr_t* model, StudioMDL_Model* studio_model)
{
	strcpy(name, bodypart.name);
	base = bodypart.base;
	for (int i = 0; i < bodypart.nummodels; i++)
	{
		mstudiomodel_t* mesh = (mstudiomodel_t*)((byte*)model + bodypart.modelindex) + i;
		StudioMDL_SubModel* submodel = new StudioMDL_SubModel(mesh, model, studio_model);
		m_vSubModels.push_back(submodel);
		m_iNumVerts += submodel->GetNumVerts();
	}
	m_iNumSubModels = m_vSubModels.size();
}

StudioMDL_SubModel::StudioMDL_SubModel(mstudiomodel_t *submodel, studiohdr_t* studiohdr, StudioMDL_Model* owner)
{
	m_pOwner = owner;
	m_pRawModel = submodel;
	m_iStartVertexData = m_pOwner->m_iNumVerts;

	mstudiomesh_t* pmeshes = (mstudiomesh_t*)((byte*)studiohdr + submodel->meshindex);

	Vector* pstudioverts = (Vector*)((byte*)studiohdr + submodel->vertindex);
	Vector* pstudionorms = (Vector*)((byte*)studiohdr + submodel->normindex);

	byte* pvertbone = ((byte*)studiohdr + submodel->vertinfoindex);
	byte* pnormbone = ((byte*)studiohdr + submodel->norminfoindex);

	std::vector<studiovert_t> list;

	for (int i = 0; i < submodel->numverts; i++)
	{
		m_vVertInfo.push_back(pstudioverts[i]);
	}

	for (int i = 0; i < submodel->numnorms; i++)
	{
		m_vNormInfo.push_back(pstudionorms[i]);
	}


	for (int i = 0; i < submodel->nummesh; i++)
	{
		m_vMesh.push_back(new StudioMDL_Mesh(pmeshes[i], studiohdr, owner, this));
	}

	m_iNumMesh = m_vMesh.size();

	auto& numvertices = m_pOwner->m_iNumVerts;
	numvertices += m_iNumVerts;


	if (m_vMesh.empty())
		return;

	int base = m_vMesh[0]->m_iStartVertexData;

	m_vRealVertInfo.resize(m_iNumVerts);
	for (int i = 0; i < m_iNumVerts; i++)
	{
		auto pos = m_pOwner->m_vTotalVerts[i + base].pos;
		m_vRealVertInfo[i] = Vector(pos.x, pos.y, pos.z);
	}

	m_vRealNormInfo.resize(m_iNumVerts);
	for (int i = 0; i < m_iNumVerts; i++)
	{
		auto norm = m_pOwner->m_vTotalVerts[i + base].normal;
		m_vRealNormInfo[i] = Vector(norm.x, norm.y, norm.z) / std::numeric_limits<short>::max();
	}

}

Vector* StudioMDL_SubModel::GetVertInfo()
{ 
	if (m_vMesh.empty())
		return nullptr;

	return m_vRealVertInfo.data();
};
Vector* StudioMDL_SubModel::GetNormInfo()
{ 
	if (m_vMesh.empty())
		return nullptr;

	return m_vRealNormInfo.data();
};

void CheckNewVertex(studiovert_t vert, std::vector<studiovert_t>& vertlist)
{
	for (auto vertex : vertlist)
	{
		if (vertex.normindex == vert.normindex &&
			vertex.texcoord[0] == vert.texcoord[0] &&
			vertex.texcoord[1] == vert.texcoord[1] &&
			vertex.vertindex == vert.vertindex)
		{
			return;
		}
	}

	vertlist.push_back(vert);

	vert = vert;
}

struct VertexKey
{
	Vector pos;
	Vector normal;
	float uv[2];

	bool operator==(const VertexKey& other) const
	{
		return pos == other.pos &&
			   normal == other.normal &&
			   uv[0] == other.uv[0] &&
			   uv[1] == other.uv[1];
	}
};

StudioMDL_Mesh::StudioMDL_Mesh(const mstudiomesh_t mesh, studiohdr_t* studiohdr, StudioMDL_Model* owner, StudioMDL_SubModel* submodelparent)
{

	m_pOwner = owner;
	m_iSkinRef = mesh.skinref;

	short* ptricmds = (short*)((byte*)studiohdr + mesh.triindex);

	auto& numvertindexes = m_pOwner->m_iNumVertIndexes;

	auto textures = (mstudiotexture_t*)((byte*)studiohdr + studiohdr->textureindex);
	auto baseSkin = (short*)((byte*)studiohdr + studiohdr->skinindex);

	auto verts = submodelparent->m_vVertInfo;
	auto norms = submodelparent->m_vNormInfo;

	auto boneIndices = ((byte*)studiohdr + submodelparent->m_pRawModel->vertinfoindex);
	auto normBoneIndices = ((byte*)studiohdr + submodelparent->m_pRawModel->norminfoindex);

	m_iStartVertex = owner->m_vTotalIndices.size();
	m_iStartVertexData = owner->m_vTotalVerts.size();

	int numVerts = 0;
	int numVertsTotal = owner->m_vTotalVerts.size();
	std::vector<uint32_t> &indices = owner->m_vTotalIndices;

	std::vector<studiomdl_vertbufferdata_t> &vertices = owner->m_vTotalVerts;


	int skinnum = 0;

	short* pskinref = owner->GetSkinIndexes();

	if (skinnum != 0 && skinnum < owner->GetNumSkinFamilies())
		pskinref += (skinnum * owner->GetNumSkinIndexes());

	int meshskinref = this->GetSkinReference();

	if (meshskinref > (owner->GetNumTextures() - 1))
		meshskinref = (owner->GetNumTextures() - 1);

	auto ptex = owner->GetTextureByIndex(pskinref[meshskinref]);

	auto texinfo = ptex->GetTextureInfo();
	float basewidth = texinfo.iWidth;
	float baseheight = texinfo.iHeight;


	int i;
	while ((i = *(ptricmds++)))
	{

		int vertexState = 0;
		bool tri_strip;

		if (i < 0)
		{
			tri_strip = false;
			i = -i;
		}
		else
			tri_strip = true;

		for (; i > 0; i--, ptricmds += 4)
		{
			auto vpos = verts[ptricmds[0]];
			auto normal = norms[ptricmds[1]];
			unsigned int vertboneid = (unsigned int)boneIndices[ptricmds[0]];
			unsigned int normboneid = (unsigned int)normBoneIndices[ptricmds[1]];
		
			float uv[2];
			uv[0] = static_cast<float>(ptricmds[2]); //* invTexSize[0];
			uv[1] = static_cast<float>(ptricmds[3]); //* invTexSize[1];
		
			if (vertexState++ < 3)
			{
				indices.push_back(numVertsTotal);
			}
			else if (tri_strip)
			{
				// flip triangles between clockwise and counter clockwise
				if (vertexState & 1)
				{
					// draw triangle [n-2 n-1 n]
					indices.push_back(numVertsTotal - 2);
					indices.push_back(numVertsTotal - 1);
					indices.push_back(numVertsTotal);
				}
				else
				{
					// draw triangle [n-1 n-2 n]
					indices.push_back(numVertsTotal - 1);
					indices.push_back(numVertsTotal - 2);
					indices.push_back(numVertsTotal);
				}
			}
			else
			{
				// draw triangle fan [0 n-1 n]
				indices.push_back(numVertsTotal - (vertexState - 1));
				indices.push_back(numVertsTotal - 1);
				indices.push_back(numVertsTotal);
			}
		
			normal = normal.Normalize();
		
			submodelparent->m_vVertBoneInfo.push_back(vertboneid);
			submodelparent->m_vNormBoneInfo.push_back(normboneid);
		
		
			studiomdl_vertbufferdata_t vert;
			vert.pos[0] = vpos.x;
			vert.pos[1] = vpos.y;
			vert.pos[2] = vpos.z;

			//in doubt of whether memory alignment is more important than memory size but oh well
			vert.normal.x = normal.x * std::numeric_limits<short>::max();
			vert.normal.y = normal.y * std::numeric_limits<short>::max();
			vert.normal.z = normal.z * std::numeric_limits<short>::max();

			assert(uv[0] < 32767 && uv[0] > -32767 && uv[1] < 32767 && uv[1] > -32767);

			unsigned int bonedata = (vertboneid & 0xFF) | (normboneid & (0xFF << 8));
		
			vert.texcoord[0] = (uv[0] / basewidth) * std::numeric_limits<short>::max();
			vert.texcoord[1] = (uv[1] / baseheight) * std::numeric_limits<short>::max();

			vert.bonedata = bonedata;
		
			vertices.push_back(vert);
		
			numVerts++;
			numVertsTotal = owner->m_vTotalVerts.size();
		}
	}

	m_iNumTriangles = mesh.numtris;
	m_iNumVerts = numVerts;
	m_iNumNorms = mesh.numnorms;

	submodelparent->m_iNumVerts += m_iNumVerts;
	submodelparent->m_iNumTriangles += m_iNumTriangles;

	//assert(m_iNumVerts == owner->m_vTotalIndices.size() - m_iStartVertex);

}

void StudioMDL_SubModel::UploadVertexData(Vector* pos, Vector* norm, float (*chromeuv)[2])
{
	if (m_vMesh.empty()) //how?
		return;

	//std::vector<studiomdl_vertbufferdata_t> vertdata;
	//vertdata.resize(this->m_iNumVerts);
	//
	//int basedata = m_vMesh[0]->m_iStartVertexData;
	//
	//for (int i = 0; i < this->m_iNumVerts; i++)
	//{
	//	auto mdl_texcoord = m_pOwner->m_vTotalVerts[i + basedata].texcoord;
	//
	//	//todo: do all of this in glsl. gpu skinning, chrome tex coords, etc, im a bit tired for that now
	//	if (chromeuv != nullptr)
	//	{
	//		float chromecoords[2];
	//		chromecoords[0] = chromeuv[i][0];
	//		chromecoords[1] = chromeuv[i][1];
	//
	//		if (chromecoords[0] != 0 || chromecoords[1] != 0)
	//			mdl_texcoord = chromecoords;
	//	}
	//
	//	 vertdata[i] = studiomdl_vertbufferdata_t{pos[i], norm[i], 0, 0, {mdl_texcoord[0], mdl_texcoord[1]}};
	//}
	//
	//auto& vertbuffer = m_pOwner->m_pModelVertBuffer;
	//vertbuffer->Bind(GL_BufferHandler::ArrayBuffer);
	//vertbuffer->BufferSubData(GL_BufferHandler::ArrayBuffer, basedata * sizeof(studiomdl_vertbufferdata_t), this->m_iNumVerts * sizeof(studiomdl_vertbufferdata_t), vertdata.data());
}

StudioMDL_Texture::StudioMDL_Texture(mstudiotexture_t* texture)
{
	m_iTexFlags = texture->flags;
	m_pTexture = gTextureLoader.GenDummyTexture(texture->index);
	m_pTexture->iWidth = texture->width;
	m_pTexture->iHeight = texture->height;
	strcpy(m_pTexture->szName, texture->name);


}

void StudioMDL_Texture::ReplaceMDLTexture(cl_texture_t* texture, const int flags)
{
	m_iTexFlags = flags;
	m_pTexture = texture;
}