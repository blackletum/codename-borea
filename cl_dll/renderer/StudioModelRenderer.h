/*
Trinity Rendering Engine - Copyright Andrew Lucas 2009-2012

The Trinity Engine is free software, distributed in the hope th-
at it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU Lesser General Public License for more det-
ails.

Model Renderer
Original code by Valve
Additional code written by Andrew Lucas
Transparency code by Neil "Jed" Jedrzejewski
*/

#pragma once

#include "windows.h"
#include "dlight.h"

#include <vector>
#include <array>
#include <list>
#include <unordered_map>

#include "../renderer/rendererdefs.h"

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#undef clamp

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class GL_ShaderProgram;
class GL_BufferHandler;
class GL_VertexArrayObject;
class StudioMDL_Model;
class StudioMDL_BodyPart;
class StudioMDL_Mesh;
class StudioMDL_Texture;

/*
====================
CStudioModelRenderer

====================
*/
class CStudioModelRenderer
{
public:

	// Initialization
	static void Init(void);
	static void VidInit(void);

private:

	static void StudioDrawModel(int flags);
	static void StudioDrawPlayer(int flags, struct entity_state_s* pplayer);


public:
	// Local interfaces
	//

	//	setup per-frame/per-pov info
	static void StudioPreFrame(ref_params_t* pparams);

	//	makes a render list sorted by models
	static void StudioPushEntityToDraw(cl_entity_s* pEnt); //dont add entities twice as this function doesnt test against that. unless thats what you wanna do, be smart

	// clears draw list
	static void StudioClearDrawList();

	// draw from list made by StudioMakeOrganizedRenderList
	static void StudioDrawModels( bool bDrawLocalPlayer = true);
	static void StudioDrawViewmodel();

	// draw (to shadowmaps) from list made by StudioMakeOrganizedRenderList
	static void StudioDrawModelsSolid();

	// just so StudioDrawModels is a bit smaller and compact
	static void StudioHandleDeadPlayer(int flags);

	// cleanup a entity's custom data (m_pCurrentEntity->efrag)
	static void StudioFreeEntity();

	// Look up animation data for sequence
	static mstudioanim_t* StudioGetAnim(model_t* m_pModel, mstudioseqdesc_t* pseqdesc);

	// Interpolate model position and angles and set up matrices
	static void StudioSetUpTransform(int trivial_accept);

	// Set up model bone positions
	static void StudioSetupBones(void);

	// Find final attachment points
	static void StudioCalcAttachments(void);

	// Determine interpolation fraction
	static float StudioEstimateInterpolant(void);

	// Determine current frame for rendering
	static float StudioEstimateFrame(mstudioseqdesc_t* pseqdesc);

	// Apply special effects to transform matrix
	static void StudioFxTransform(cl_entity_t* ent, matrix3x4_t &transform);

	// Spherical interpolation of bones
	static void StudioSlerpBones(vec4_t q1[], float pos1[][3], vec4_t q2[], float pos2[][3], float s);

	// Compute bone adjustments ( bone controllers )
	static void StudioCalcBoneAdj(float dadt, float* adj, const byte* pcontroller1, const byte* pcontroller2, byte mouthopen);

	// Get bone quaternions
	static void StudioCalcBoneQuaterion(int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* q);

	// Get bone positions
	static void StudioCalcBonePosition(int frame, float s, mstudiobone_t* pbone, mstudioanim_t* panim, float* adj, float* pos);

	// Compute rotations
	static void StudioCalcRotations(float pos[][3], vec4_t* q, mstudioseqdesc_t* pseqdesc, mstudioanim_t* panim, float f);

	//checks general mdl projection matrix and viewmdl projection matrix.
	static void CheckProjection();

	//client anim events
	static void StudioClientEvents();

	// Send bones and verts to renderer
	static void StudioRenderModel(void);

	// Finalize rendering
	static void StudioRenderFinal(void);

	static void StudioSaveBones(void);
	static void StudioMergeBones(model_t* m_pSubModel);

	// Player specific data
	// Determine pitch and blending amounts for players
	static void StudioPlayerBlend(mstudioseqdesc_t* pseqdesc, int* pBlend, float* pPitch);

	// Estimate gait frame for player
	static void StudioEstimateGait(entity_state_t* pplayer);

	// Process movement of player
	static void StudioProcessGait(entity_state_t* pplayer);

public:

	// Do interpolation?
	static bool m_fDoInterp;
	// Do gait estimation?
	static bool m_fGaitEstimation;

	// Cvars that studio model code needs to reference
	//
	// Use high quality models?
	static cvar_t* m_pCvarHiModels;
	// Developer debug output desired?
	static cvar_t* m_pCvarDeveloper;
	// Draw entities bone hit boxes, etc?
	static cvar_t* m_pCvarDrawEntities;
	// Draw viewmodel?
	static cvar_t* m_pCvarDrawViewmodel;

	// The entity which we are currently rendering.
	static cl_entity_t* m_pCurrentEntity;

	//extra ent data for static props
	static entextradata_t* CStudioModelRenderer::m_pCurrentExtraData;

	// The model for the entity being rendered
	static model_t* m_pRenderModel;

	// Player info for current player, if drawing a player
	static player_info_t* m_pPlayerInfo;

	// The index of the player being drawn
	static int m_nPlayerIndex;

	// Pointer to header block for studio model data
	static studiohdr_t* m_pStudioHeader;

	// Pointers to current body part and submodel
	static mstudiobodyparts_t* m_pBodyPart;
	static mstudiomodel_t* m_pSubModel;

	static StudioMDL_Model* m_pCurrentStudioMDL;
	static studioentity_data_t* m_pCurrentStudioEntData;

	// Palette substition for top and bottom of model
	static int m_nTopColor;
	static int m_nBottomColor;

	//
	// Sprite model used for drawing studio model chrome
	static model_t* m_pChromeSprite;

	// Matrices
	// Model to world transformation
	static matrix3x4_t(*m_protationmatrix);
	// Model to view transformation
	static matrix3x4_t(*m_paliastransform);

	// Concatenated bone and light transforms
	static matrix3x4_t (*m_pbonetransform)[MAXSTUDIOBONES];
	static matrix3x4_t (*m_plighttransform)[MAXSTUDIOBONES];

	static std::unordered_map<StudioMDL_Model*, std::vector<cl_entity_s*>> m_vStudioDrawList;

	// Caching
	// Number of bones in bone cache
	static int m_nCachedBones;
	// Names of cached bones
	static char m_nCachedBoneNames[MAXSTUDIOBONES][32];
	// Cached bone & light transformation matrices
	static matrix3x4_t m_rgCachedBoneTransform[MAXSTUDIOBONES];

public:
	static void StudioSetupModel(int bodypart);
	static void StudioDrawPoints(StudioMDL_BodyPart* bodypart);
	static void StudioDrawMesh(StudioMDL_Mesh* pmesh, StudioMDL_Texture* ptex);
	static void StudioDrawWireframe(void);

	static void StudioSetupRenderer(int rendermode);
	static qboolean StudioCheckBBox(void);

	static void StudioEntityLight(void);
	static bool StudioCullBBox(const Vector& mins, const Vector& maxs);

	static void StudioSetupLighting(void);
	static bool StudioRecursiveLightPoint(entextrainfo_t* ext, mnode_t* node, Vector start, Vector end, Vector& color);

	static void StudioSwapEngineCache(void);

	static entextrainfo_t* StudioAllocExtraInfo(void);

	static void StudioDrawBBox(void);
	static void StudioDrawModelSolid(void);
	static void StudioDrawPlayerSolid(entity_state_t* pplayer);
	static void StudioDrawPointsSolid(StudioMDL_BodyPart* bodypart);

	static Vector m_vMins;
	static Vector m_vMaxs;

	static Vector m_vVertexTransform[MAXSTUDIOVERTS]; // transformed vertices
	static Vector m_vNormalTransform[MAXSTUDIOVERTS]; // transformed normals

	static Vector* m_pVertexTransform; // pointer to vertex transform
	static Vector* m_pNormalTransform; // pointer to normal transform

	static lighting_ext m_pLighting; // buz

	static mlight_t* m_pModelLights[MAX_MODEL_LIGHTS];
	static int m_iNumModelLights;

	static entextrainfo_t m_pExtraInfo[MAXRENDERENTS];
	static int m_iNumExtraInfo;

	static float m_flLastFov, m_flLastVMFov;

	static double m_fStudioMDLRenderTime;

	static bool m_bExternalEntity;
	static bool m_bChromeShell;
	static bool m_bShadowMapOn;

	// glsl start

	static GL_BufferHandler* m_Model_PerEntityBuffer;
	static GL_BufferHandler* m_Model_PerFrameBuffer;
	static GL_BufferHandler* m_ModelBones_Buffer;
	static GL_BufferHandler* m_ModelSolid_Buffer;

	static GL_ShaderProgram *m_ModelShader;
	static GL_ShaderProgram *m_ModelSolidShader;

	enum modelshader_uniforms
	{
		mdlshader_texturescale,
		mdlshader_viewmodel,

		mdlshader_texturematrix,

		mdlshader_fullbright,
		mdlshader_wireframe,
		mdlshader_chrometexture,



		_mdlshader_uniformsize //must be last
	};

	static GLuint m_ModelShaderLocs[_mdlshader_uniformsize];

	enum modelshadersolid_uniforms
	{
		mdlshadersolid_texturescale = 0,
		mdlshadersolid_sunshadow,
		mdlshadersolid_alphatest,

		_mdlshadersolid_uniformsize //must be last
	};

	static GLuint m_ModelShaderSolidLocs[_mdlshadersolid_uniformsize];

	static glm::mat4 m_VM_ProjectionMatrix;

	struct mdlshadersolid_data_t
	{
		glm::mat4 projviewmatrix;
		glm::mat4 modelmatrix;
		glm::vec4 light_pos;
		glm::ivec4 int_values; // x represents if we're rendering a static model or not

		matrix3x4_t bonematrixes[128];
	};

	struct mdlshader_perframedata_t
	{
		glm::mat4 projviewmatrix;
		glm::mat4 VMprojviewmatrix;
		glm::vec4 fogcolor_n_fogstart; // w = fogstart
		glm::vec4 fogend_n_fogactive_n_lightdebug;  // x = fogend, y = fogactive, z = light debug cvar

		// w is empty for both these :( wasted space

		glm::vec4 renderorigin;
		glm::vec4 renderright;
	};

	struct mdlshader_perentitydata_t
	{
		glm::vec4 lightdir;
		glm::vec4 ambientlight;
		glm::vec4 diffuselight;

		glm::mat4 modelmatrix;

		glm::ivec4 int_values; // x = numlights; y = chromeshell boolean; z = is this entity is static (prop_static) or not

		glm::mat3x4 modellight_info[MAX_MODEL_LIGHTS];
	};

	static mdlshadersolid_data_t m_dSolidModelData;
	static mdlshader_perframedata_t m_dModelPerFrameData;
	static mdlshader_perentitydata_t m_dModelPerEntityData;

	// glsl end

	static cvar_t* m_pCvarDrawStudioModels;
	static cvar_t* m_pCvarStudioModelBBox;
	static cvar_t* m_pCvarStudioModelLightDebug;
	static cvar_t* m_pCvarStudioModelDecals;
	static cvar_t* m_pCvarGlowShellFreq;

	static cvar_t* m_pCvarSkyVecX;
	static cvar_t* m_pCvarSkyVecY;
	static cvar_t* m_pCvarSkyVecZ;

	static cvar_t* m_pCvarSkyColorX;
	static cvar_t* m_pCvarSkyColorY;
	static cvar_t* m_pCvarSkyColorZ;

	static cvar_t* m_pCvarViewmodelFov;

public:
	static void StudioDrawExternalEntity(cl_entity_t* pEntity, bool bSkybox = false);
	static void StudioSetupLightingEXT(void);
	static void StudioRenderModelEXT(void);
	static void StudioDrawPointsEXT(void);
	static void StudioDrawMeshEXT(StudioMDL_Texture *ptex, vbomesh_t* pmesh);
	static void StudioDrawWireframeEXT(void);

	static void StudioDrawExternalEntitySolid(cl_entity_t* pEntity);
	static void StudioDrawPointsSolidEXT(StudioMDL_BodyPart* bodypart, int baseindex);

	static void StudioSaveModelData(modeldata_t* pExtraData);
	static void StudioSaveUniqueData(entextradata_t* pExtraData);
	static void StudioManageVertex(studiovert_t* pvert);

	static vboheader_t* m_pVBOHeader;
	static vbosubmodel_t* m_pVBOSubModel;

public:

	static std::vector<studiovert_t> m_pRefArray;

	static std::vector<brushvertex_t> m_pVBOVerts;

	static std::vector<unsigned int> m_usIndexes;

	static int m_iCurStart;

public:
	static studioentity_data_t* StudioAllocEntityData(void);

public:
	static void StudioDrawDecals(void);
	static studiodecal_t* StudioAllocDecal(void);
	static studiodecal_t* StudioAllocDecalSlot(void);

	static void StudioDecalExternal(Vector vpos, Vector vnorm, const char* name);
	static void StudioDecalForEntity(Vector position, Vector normal, const char* szName, cl_entity_t* pEntity);
	static void StudioDecalForSubModel(Vector position, Vector normal, studiodecal_t* decal);
	static void StudioDecalTriangle(studiotri_t* tri, Vector position, Vector normal, studiodecal_t* decal);

	static std::vector<std::unique_ptr<studiodecal_t>> m_pStudioDecals;
	static std::vector<std::unique_ptr<studioentity_data_t>> m_pStudioEntityData;
};

extern CStudioModelRenderer g_StudioRenderer;