#pragma once

#include "PlatformHeaders.h"
#include "hud.h"

#include "studio.h"
#include <vector>

#include "rendererdefs.h"
#include "opengl_utils/GL_VertexArrayObject.h"

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


struct studiotri_t;

class StudioMDL_BodyPart;
class StudioMDL_SubModel;
class StudioMDL_Mesh;
class StudioMDL_Texture;

class GL_BufferHandler;
class GL_ShaderProgram;
class GL_VertexArrayObject;

struct studiomdl_vertbufferdata_t
{
	Vector pos;
	short_3dvector normal;
	unsigned short texcoord[2];
	unsigned int bonedata;
	byte _padding[4]; //:(
};

class StudioMDL_Model
{
	//Hello, friend.
	friend class StudioMDL_Mesh;
	friend class StudioMDL_SubModel;
	friend class StudioMDL_BodyPart;

public:
	StudioMDL_Model(model_t* model);

	__forceinline void EnableBuffers() const noexcept { m_pModelVAO->BindVAO(); };
	__forceinline bool IsBufferEnabled() const noexcept { return m_pModelVAO == GL_VertexArrayObject::GetBoundVAO(); }
	__forceinline void DisableBuffers() const noexcept { GL_VertexArrayObject::ResetVAOBinding(); };

	__forceinline void DrawElements(int indexcount, int indexoffset) noexcept
	{
		glDrawElements(GL_TRIANGLES, indexcount * 3, GL_UNSIGNED_INT, (const void*)(indexoffset * sizeof(uint32_t)));
	};

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

	GL_BufferHandler* m_pModelVertBuffer = nullptr; // includes vert pos, vert normal, and vert texcoord. update with BufferSubData
	GL_BufferHandler* m_pModelVertIndexBuffer = nullptr; // includes vertex indexes
public:
	GL_VertexArrayObject* m_pModelVAO = nullptr;

private:
	
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

	static std::vector<StudioMDL_Model*> m_vCachedStudioModels;
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

	__forceinline byte* GetBoneVertInfo() noexcept { return m_vVertBoneInfo.data(); };
	__forceinline byte* GetBoneNormInfo() noexcept { return m_vNormBoneInfo.data(); };

	Vector* GetVertInfo();
	Vector* GetNormInfo();

	__forceinline int GetMeshNum() const noexcept { return m_iNumMesh; };
	__forceinline int GetNumVerts() const noexcept { return m_iNumVerts; };
	__forceinline int GetNumTriangles() const noexcept { return m_iNumTriangles; };

	__forceinline StudioMDL_Mesh* GetMeshbyIndex(int index) { return m_vMesh[index]; };

private:
	std::vector<byte> m_vVertBoneInfo; // bone info, index into m_pbonetransform
	std::vector<byte> m_vNormBoneInfo; // bone info, index into m_pbonetransform

	std::vector<Vector> m_vVertInfo; // mesh info, 3d position of the vertex
	std::vector<Vector> m_vNormInfo; // mesh info, 3d direction of the normal

	std::vector<Vector> m_vRealVertInfo; // mesh info, 3d position of the vertex
	std::vector<Vector> m_vRealNormInfo; // mesh info, 3d direction of the normal

	std::vector<StudioMDL_Mesh*> m_vMesh;

	mstudiomodel_t *m_pRawModel;

	StudioMDL_Model* m_pOwner;

	int m_iNumVerts = 0;
	int m_iNumTriangles = 0;
	int m_iStartVertexData = 0;
	int m_iNumMesh = 0;
};

class StudioMDL_Mesh
{
	friend class StudioMDL_SubModel;
	friend class StudioMDL_BodyPart;
	friend class StudioMDL_Model;

public:
	StudioMDL_Mesh(const mstudiomesh_t mesh, studiohdr_t* studiohdr, StudioMDL_Model* owner, StudioMDL_SubModel* submodelparent);

	std::vector<studiotri_t>& GetTriangleCmds() { return m_vTris; };

	__forceinline int GetNumVerts() const noexcept { return m_iNumVerts; };
	__forceinline int GetNumTriangles() const noexcept { return m_iNumTriangles; };
	__forceinline int GetNumNormals() const noexcept { return m_iNumNorms; };

	__forceinline int GetSkinReference() const noexcept { return m_iSkinRef; };

	__forceinline int GetMeshBufferOffset() const noexcept { return m_iStartVertex; };

private:
	std::vector<studiotri_t> m_vTris;

	StudioMDL_Model* m_pOwner = nullptr;

	int m_iSkinRef = 0;
	int m_iNumNorms = 0;
	int m_iNumTriangles = 0;
	int m_iNumVerts = 0;

	int m_iStartVertex = 0; // index into element array buffer, do not use in vertex array
	int m_iStartVertexData = 0;
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