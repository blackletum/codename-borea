#pragma once

#include "PlatformHeaders.h"
#include "hud.h"

#include "studio.h"
#include <vector>

#include "rendererdefs.h"
#include "opengl_utils/GL_Mesh.h"

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#undef clamp

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

//
// STUDIO MODEL MODEL GENERATOR
// 
// this is a class that digests studio models and turns them into a more readable and user-friendly
// structure, 
//

#define GL_ONE_BIG_BUFFER_FOR_STUDIOMDLS

struct studiotri_t;

class StudioMDL_BodyPart;
class StudioMDL_SubModel;
class StudioMDL_Mesh;
class StudioMDL_Texture;

struct studiomdl_vertbufferdata_t
{
	Vector pos;
	float texcoord[2];
	unsigned int bonedata;
	short_3dvector normal;
	byte _padding[5]; //:(
	GL_DECLARE_ATTRIBLIST();
};

class StudioMDL_Model
{
	//Hello, friend.
	friend class StudioMDL_Mesh;
	friend class StudioMDL_SubModel;
	friend class StudioMDL_BodyPart;

public:
	StudioMDL_Model(model_t* model);

#if !defined(GL_ONE_BIG_BUFFER_FOR_STUDIOMDLS)
	void BindMesh() const noexcept; 
	bool IsMeshBound() const noexcept;
	void UnbindMesh() const noexcept;
#else
	static void BindGlobalMesh() noexcept;
	static void UnbindGlobalMesh() noexcept;
#endif

	void DrawElements(int indexoffset, int indexcount) const noexcept;

	__forceinline int GetNumBodyParts() const noexcept { return m_iNumBodyParts; };
	__forceinline int GetNumTextures() const noexcept { return m_iNumTextures; }
	__forceinline short GetNumSkinIndexes() const noexcept { return m_iNumSkinIndexes; /*m_vSkinIndexes.size() / m_iSkinFamilies;*/ };
	__forceinline int GetNumSkinFamilies() const noexcept { return m_iSkinFamilies; }
	__forceinline int GetNumBones() const noexcept { return m_iNumBones; }

	__forceinline StudioMDL_BodyPart* GetBodyPartbyIndex(int index) noexcept { return m_vBodyParts[index]; };
	__forceinline StudioMDL_Texture* GetTextureByIndex(int index) noexcept { return m_vTextures[index]; }
	__forceinline short* GetSkinIndexes() noexcept { return m_vSkinIndexes.data(); }

	__forceinline model_t* GetEngineModel() noexcept { return m_pEngineModel;  }

private:

	void CheckCustomMDLData();
	void CheckExternTextures();

	model_t* m_pEngineModel = nullptr;
	studiohdr_t* m_pStudioHeader = nullptr;

	std::vector<StudioMDL_BodyPart*> m_vBodyParts;
	std::vector<StudioMDL_Texture*> m_vTextures;
	std::vector<short> m_vSkinIndexes;

private:
#if !defined(GL_ONE_BIG_BUFFER_FOR_STUDIOMDLS)
	GL_Mesh* m_p3DMesh;
#else
	uint32_t m_uiIndexOffset;
	uint32_t m_uiVertexOffset;
#endif
	
	int m_iNumVertIndexes = 0;
	int m_iNumVerts = 0;
	int m_iNumModelVertexes = 0;
	int m_iSkinFamilies = 0;
	int m_iNumBones = 0;
	int m_iNumBodyParts = 0;
	int m_iNumTextures = 0;
	short m_iNumSkinIndexes = 0;

	std::vector<studiomdl_vertbufferdata_t> m_vTotalVerts;
	std::vector<uint32_t> m_vTotalIndices;
};



class StudioMDL_BodyPart
{
	friend class StudioMDL_Mesh;
	friend class StudioMDL_SubModel;
	friend class StudioMDL_Model;

public:
	StudioMDL_BodyPart(const mstudiobodyparts_t bodypart, studiohdr_t* model, StudioMDL_Model* studio_model);

	__forceinline int GetNumModels() const noexcept { return m_iNumSubModels; };
	__forceinline StudioMDL_SubModel* GetModelbyIndex(int index) noexcept { return m_vSubModels[index]; };

	__forceinline int GetBase() const noexcept { return base; };

private:
	char name[64];
	int base = 0;
	std::vector<StudioMDL_SubModel*> m_vSubModels;
	int m_iNumVerts = 0;
	int m_iNumSubModels = 0;
};



class StudioMDL_SubModel
{
	friend class StudioMDL_Mesh;
	friend class StudioMDL_BodyPart;
	friend class StudioMDL_Model;

public:
	StudioMDL_SubModel(mstudiomodel_t *submodel, studiohdr_t* studiohdr, StudioMDL_Model* owner);

	void UploadVertexData(Vector* pos, Vector* normal, float (*chromeuv)[2]); //unused for now

	__forceinline int GetMeshNum() const noexcept { return m_iNumMesh; };
	__forceinline int GetNumVerts() const noexcept { return m_iNumVerts; };

	__forceinline StudioMDL_Mesh* GetMeshbyIndex(int index) { return m_vMesh[index]; };

private:
	std::vector<Vector> m_vVertInfo; // mesh info, 3d position of the vertex
	std::vector<Vector> m_vNormInfo; // mesh info, 3d direction of the normal

	std::vector<StudioMDL_Mesh*> m_vMesh;

	mstudiomodel_t *m_pRawModel;

	StudioMDL_Model* m_pOwner;

	int m_iNumVerts = 0;
	int m_iStartVertexData = 0;
	int m_iNumMesh = 0;
};

class StudioMDL_Mesh
{
	friend class StudioMDL_SubModel;
	friend class StudioMDL_BodyPart;
	friend class StudioMDL_Model;

public:
	StudioMDL_Mesh(const mstudiomesh_t *mesh, studiohdr_t* studiohdr, StudioMDL_Model* owner, StudioMDL_SubModel* submodelparent);

	std::vector<studiotri_t>& GetTriangleCmds() { return m_vTris; };

	__forceinline int NumVerts() const noexcept { return m_iNumVerts; };
	__forceinline int NumIndices() const noexcept { return m_iNumIndices; };
	__forceinline int NumNormals() const noexcept { return m_pRawMesh->numnorms; };

	__forceinline int SkinReference() const noexcept { return m_iSkinRef; };

	__forceinline int MeshBufferOffset() const noexcept { return m_iStartVertex; };

	__forceinline void DrawMesh() const noexcept { return m_pOwner->DrawElements(m_iStartVertex, m_iNumIndices); }

private:
	std::vector<studiotri_t> m_vTris;
	StudioMDL_Model* m_pOwner = nullptr;
	const mstudiomesh_t* m_pRawMesh;

	int m_iSkinRef = 0;
	int m_iNumVerts = 0;
	int m_iNumIndices = 0;

	int m_iStartVertex = 0; // index into element array buffer, do not use in vertex array
};

class StudioMDL_Texture
{
public:
	StudioMDL_Texture(mstudiotexture_t* texture);

	// THE TEXTURE PASSED TO THIS SHOULD NEVER BE DELETED !!
	void ReplaceMDLTexture(cl_texture_t* texture, const int flags);

	__forceinline cl_texture_t GetTextureInfo() const noexcept { return *m_pTexture; };
	__forceinline int GetTextureFlags() const noexcept { return m_iTexFlags; };

private:
	cl_texture_t* m_pTexture;
	int m_iTexFlags;
};