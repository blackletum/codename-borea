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
#include "../renderer/propmanager.h"

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

	static void StudioPreFrame(ref_params_t* pparams);	//	setup per-frame info

	static void StudioPushEntityToDraw(cl_entity_s* pEnt); 	//	makes a render list sorted by models
	static void StudioClearDrawList();	// clears render list

	static void StudioUploadRenderData();	//	uploads render data to gpu (just bones for now)

	static void StudioDrawModels( bool bDrawLocalPlayer = true);	// draw from list of pushed entities
	static void StudioDrawPoints(StudioMDL_BodyPart* bodypart);
	static void StudioDrawMesh(StudioMDL_Mesh* pmesh, StudioMDL_Texture* ptex);

	static void StudioDrawModelsSolid();	// draw (to shadowmaps) from list of pushed entities
	static void StudioDrawModelSolid(void);
	static void StudioDrawPlayerSolid(entity_state_t* pplayer);
	static void StudioDrawPointsSolid(StudioMDL_BodyPart* bodypart);
	static void StudioDrawViewmodel();


	static void StudioDrawBBox(void);
	static void StudioDrawWireframe(void);

	static void StudioSwapEngineCache(void);


	static void StudioSetupPropDraw(bool solidpass);

	static void StudioDrawPropEntity(cl_entity_t* pEntity, bool bSkybox = false);
	static void StudioDrawPropModel(void);
	static void StudioDrawPointsProp(void);
	static void StudioDrawMeshProp(StudioMDL_Texture* ptex, vbomesh_t* pmesh);
	static void StudioDrawWireframeProp(void);

	static void StudioDrawPropModelSolid(cl_entity_t* pEntity);
	static void StudioDrawPointsSolidEXT(StudioMDL_BodyPart* bodypart, int baseindex);


	static void StudioFinishPropDraw(bool solidpass);

	static void SetCurrentEntity(cl_entity_t* pEnt, bool bStaticProp = false);
	static void SetCurrentMDL(model_t* pMdl);

	static void StudioSetupViewmodel();	// sets up viewmodel for rendering later

	static void StudioFreeEntity();	// cleanup a entity's custom data (m_pCurrentEntity->efrag)

	static mstudioanim_t* StudioGetAnim(model_t* m_pModel, mstudioseqdesc_t* pseqdesc);	// Look up animation data for sequence

	static void StudioSetUpTransform(int trivial_accept);	// Interpolate model position and angles and set up matrices

	static void StudioHandleDeadPlayer(int flags);	// just so StudioDrawModels is a bit smaller and compact

	static void StudioSetupBones(void);	// Set up model bone positions

	static void StudioCalcAttachments(void);	// Find final attachment points

	static float StudioEstimateInterpolant(void);	// Determine interpolation fraction

	static float StudioEstimateFrame(mstudioseqdesc_t* pseqdesc);	// Determine current frame for rendering

	static void StudioFxTransform(cl_entity_t* ent, matrix3x4_t &transform);	// Apply special effects to transform matrix

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

	static void CheckVMProjection();	//checks viewmdl projection matrix.

	static void StudioClientEvents();	//client anim events

	static void StudioRenderModel(void);	// Send bones and verts to renderer

	static void StudioRenderFinal(void);	// Finalize rendering

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

	// Matrices
	// Model to world transformation
	static matrix3x4_t(*m_protationmatrix);
	// Model to view transformation
	static matrix3x4_t(*m_paliastransform);

	// Concatenated bone and light transforms
	static matrix3x4_t (*m_pbonetransform)[MAXSTUDIOBONES];
	static matrix3x4_t (*m_plighttransform)[MAXSTUDIOBONES];

	// Caching
	// Number of bones in bone cache
	static int m_nCachedBones;
	// Names of cached bones
	static char m_nCachedBoneNames[MAXSTUDIOBONES][32];
	// Cached bone & light transformation matrices
	static matrix3x4_t m_rgCachedBoneTransform[MAXSTUDIOBONES];

public:
	static void StudioSetupModel(int bodypart);

	static void StudioSetupRenderer();
	static qboolean StudioCheckBBox(void);

	static void StudioEntityLight(void);
	static bool StudioCullBBox(const Vector& mins, const Vector& maxs);

	static void StudioSetupLighting(void);
	static bool StudioRecursiveLightPoint(entextrainfo_t* ext, clientmnode_t* node, Vector start, Vector end, Vector& color);

	static entextrainfo_t* StudioAllocExtraInfo(void);

	static double m_fStudioMDLRenderTime;

	// Cvars that studio model code needs to reference
	//
	static cvar_t* m_pCvarHiModels;	// Use high quality models?
	static cvar_t* m_pCvarDeveloper;	// Developer debug output desired?
	static cvar_t* m_pCvarDrawEntities;	// Draw entities bone hit boxes, etc?
	static cvar_t* m_pCvarDrawViewmodel;	// Draw viewmodel?

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
	static void StudioSetupPropLighting(void);

	static void StudioSaveModelData(modeldata_t* pExtraData);
	static void StudioSaveUniqueData(entextradata_t* pExtraData);
	static void StudioManageVertex(studiovert_t* pvert);

public:
	static studioentity_data_t* StudioAllocEntityData(void);

	static void StudioDrawDecals(void);
	static studiodecal_t* StudioAllocDecal(void);
	static studiodecal_t* StudioAllocDecalSlot(void);

	static void StudioDecalExternal(Vector vpos, Vector vnorm, const char* name);
	static void StudioDecalForEntity(Vector position, Vector normal, const char* szName, cl_entity_t* pEntity);
	static void StudioDecalForSubModel(Vector position, Vector normal, studiodecal_t* decal);
	static void StudioDecalTriangle(studiotri_t* tri, Vector position, Vector normal, studiodecal_t* decal);
};

extern CStudioModelRenderer g_StudioRenderer;