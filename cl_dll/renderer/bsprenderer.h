/*
Trinity Rendering Engine - Copyright Andrew Lucas 2009-2012

The Trinity Engine is free software, distributed in the hope th-
at it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU Lesser General Public License for more det-
ails.

BSP Renderer
Original code by Buzer and Id Software
Extended and/or recoded by Andrew Lucas
*/

#pragma once

#include "PlatformHeaders.h"
#include "pm_defs.h"
#include "cl_entity.h"
#include "ref_params.h"
#include "dlight.h"
#include "parsemsg.h"
#include "cvardef.h"
#include "textureloader.h"
#include "rendererdefs.h"

#include "BSPModel_Gen.h"

#include <memory>

#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#undef clamp

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


#include "stb_image_write.h"

#define LIGHTMAP_TEXUNIT GL_TEXTURE0
#define SURFTEXTURE_TEXUNIT GL_TEXTURE1
#define SPOTLIGHT_TEXUNIT GL_TEXTURE2
#define SHADOWMAP_TEXUNIT GL_TEXTURE3
#define CUBEMAPSHADOW_TEXUNIT GL_TEXTURE4

#define DEFAULT_SHADOWMAP_RES 256

#define CHECKVISBIT( vis, b )		((b) >= 0 ? (byte)((vis)[(b) >> 3] & (1 << ((b) & 7))) : (byte)false )
#define SETVISBIT( vis, b )( void )	((b) >= 0 ? (byte)((vis)[(b) >> 3] |= (1 << ((b) & 7))) : (byte)false )
#define CLEARVISBIT( vis, b )( void )	((b) >= 0 ? (byte)((vis)[(b) >> 3] &= ~(1 << ((b) & 7))) : (byte)false )


struct DecalVert_t
{
	Vector pos;
	float texcoord[2];
	byte _padding[12];
};

struct skyvert_t
{
	Vector pos;
	float texcoord[2];
	byte _padding[12];
};

class GL_FBOHandler;
class GL_RBOHandler;
class GL_BufferHandler;
class GL_ShaderProgram;
class GL_VertexArrayObject;

/*
====================
CBSPRenderer

====================
*/
class CBSPRenderer
{
public:
	void Init(void);
	void VidInit(void);
	void Shutdown(void);
	void SetupRenderer(void);
	void SetupPreFrame(ref_params_t* vieworg);
	void CheckTextures(void);

	void Make_ShadowMaps(void);
	void Generate_Sun_Shadow(void);
	void Generate_Spotlight_Shadow(void);
	void Generate_Pointlight_Shadow(void);
	void DrawWorldSolid(void);

	void RecursiveWorldNodeSolid(clientmnode_t* node);
	void DrawBrushModelSolid(cl_entity_t* pEntity);

	bool IsInPotentiallyVisibleSet(int visframe) { return CHECKVISBIT(m_pPVS, visframe); };

	Vector TriWorldToScreen(Vector point);

	void RendererRefDef(ref_params_t* pparams);
	void DrawNormalTriangles(bool draw_world = true);
	void DrawNormalTriangles_Cheap(bool draw_world = true, bool draw_ents = true);
	void DrawTransparentTriangles(void);
	void RenderFirstPass();
	void RenderFinalPasses();
	void RenderWireframe(void);
	void DrawWorld(bool m_bSkyBox = false);

	void RenderSunShadow(void);

	void LoadWADDecals(void);

	void GetRenderEnts(void);
	void AddEntity(cl_entity_t* pEntity);
	int FilterEntities(int type, struct cl_entity_s* pEntity, const char* modelname); //

	void DecayLights(void);
	bool HasDynLights(void);
	void GetAdditionalLights(void);
	cl_dlight_t* CL_AllocDLight(int key);

	int MsgDynLight(const char* pszName, int iSize, void* pbuf);

	void SetupDynLight(void);
	void FinishDynLight(void);
	void SetDynLightBBox(void);
	void SetupSpotLight(void);
	void FinishSpotLight(void);
	bool LightCanShadow(void);
	int CullDynLightBBox(Vector mins, Vector maxs);

	void CreateTextures(void);

	void FreeBuffer(void);
	void GenerateVertexArray(void);

	void DrawBrushModel(cl_entity_t* pEntity, bool bStatic = false);
	void RecursiveWorldNode(clientmnode_t* node);

	void UpdateLightStylesLM();

	void SurfaceToChain(clientmsurface_t* psurfbase, clientmsurface_t* s);
	void DrawPolyFromArray(clientmsurface_t* pbaseptr, clientmsurface_t* psurf);

	bool DynamicLighted(const Vector& vmins, const Vector& vmaxs);
	void DrawDynamicLightsForWorld(void);
	void RecursiveWorldNodeLight(clientmnode_t* node);
	void DrawDynamicLightsForEntity(cl_entity_t* pEntity);
	void DrawEntityFacesForLight(cl_entity_t* pEntity);

	void InitSky(void);
	void DrawSky(void);
	void RemoveSky(void);
	int MsgSkyMarker_Sky(const char* pszName, int iSize, void* pbuf);
	int MsgSkyMarker_World(const char* pszName, int iSize, void* pbuf);

	void ClearSurfaceDrawChain(void);

	bool ExtensionSupported(const char* ext);

	void AnimateLight(void);
	void UploadLightmaps(void);
	void BuildLightmap(clientmsurface_t* surf, int surfindex, color24* out);
	void AddLightStyle(int iNum, const char* szStyle);

	void BindGLTexture(GLenum texture, GLuint id);

	texture_t* TextureAnimation(texture_t* base, int frame);

public:
	void DrawDecals(bool m_bTransPass = false);
	void LoadDecals(void);
	void DeleteDecals(void);

	decalgroup_t* FindGroup(const char* _name);
	cl_texture_t* LoadDecalTexture(const char* texname);
	decalgroupentry_t* GetRandomDecal(decalgroup_t* group);
	decalgroupentry_t* FindDecalByName(const char* szName);

	bool CullDecalBBox(Vector mins, Vector maxs);
	void CreateDecal(Vector endpos, Vector pnormal, const char* name, int persistent = 0, bool fromwad = false, float angle = 0);
	void RecursiveCreateDecal(mnode_t* node, decalgroupentry_t* texptr, customdecal_t* pDecal, Vector endpos, Vector pnormal, float angle = 0);
	void DecalSurface(clientmsurface_t* surf, decalgroupentry_t* texptr, cl_entity_t* pEntity, customdecal_t* pDecal, Vector endpos, Vector pnormal, float angle = 0);

	int MsgCustomDecal(const char* pszName, int iSize, void* pbuf);

	void CreateCachedDecals(void);
	void DrawSingleDecal(customdecal_t* decal, std::vector<DecalVert_t>& decalvertlist, bool m_bTransPass = false, bool *bNeedsBufferUpdate = nullptr);

	customdecal_t* AllocDecal(void);
	customdecal_t* AllocStaticDecal(void);

	void GetUpRight(Vector forward, Vector& up, Vector& right);
	int ClipPolygonByPlane(const Vector* arrIn, int numpoints, Vector normal, Vector planepoint, Vector* arrOut);
	void FindIntersectionPoint(const Vector& p1, const Vector& p2, const Vector& normal, const Vector& planepoint, Vector& newpoint);

public:

	enum QuadDebug_Sections
	{
		quad_FullScreen = 0,
		quad_TopRight = 6,
		quad_BottomRight = 12,
		quad_BottomLeft = 18,
		quad_TopLeft = 24
		
	};

	GL_BufferHandler *m_pMainBuffer;
	GL_BufferHandler *m_pBasicFullscreenQuad;
	GL_BufferHandler *m_pDecalsBuffer;
	brushvertex_t* m_pBufferData;
	brushface_t* m_pFacesExtraData;

	GL_BufferHandler* m_pSimpleSky_Buffer;

	GL_VertexArrayObject* m_pBSP_VAO;
	GL_VertexArrayObject* m_pDecalVAO;
	GL_VertexArrayObject* m_pSimpleSkyVAO;
	GL_VertexArrayObject* m_pScreenQuadVAO;

	GL_TextureHandler* m_pMainFBOTexture;
	GL_FBOHandler* m_pMainFBO;
	GL_RBOHandler* m_pMainRBO;
	
	int m_iNumTotalShadows; // total number of shadowmaps made in this frame

	cl_entity_t* m_pCurrentEntity;
	cl_dlight_t* m_pCurrentDynLight;
	byte* m_pPVS;
	clientmleaf_t* m_pViewLeaf;

	cl_entity_t* m_pWorldEnt;

	dlight_t* m_pFirstDLight;
	dlight_t* m_pFirstELight;

	ref_params_t m_RefParams;

	int m_iTotalVertCount;
	int m_iTotalFaceCount;
	int m_iTotalTriCount;

	int m_iTexPointer[4];
	int m_iEnvStates[4];
	int m_iTUSupport;

	int m_iFrameCount;

	cl_texture_t* m_pFlashlightTextures[MAX_SPOTLIGHT_TEXTURES];
	int m_iNumFlashlightTextures;

	bool m_bDrawSky;
	bool m_bMirroring;
	bool m_bLightShadow;
	bool m_bMainPass;
	bool m_bSunShadowMapPass;

	bool m_bReloaded;
	bool m_bGotAdditional;
	bool m_bGotStaticLights;

	bool m_bFullBright;

	bool m_bLoaded_decal_wad = false;

	Vector m_vRenderOrigin;
	Vector m_vViewAngles;
	Vector m_vVecToEyes;

	Vector m_vSkyOrigin;
	Vector m_vSkyWorldOrigin;

	Vector m_vCurDLightOrigin;
	Vector m_vCurSpotForward;

	cvar_t* m_pCvarSpeeds;
	cvar_t* m_pCvarDynamic;
	cvar_t* m_pCvarDrawWorld;
	cvar_t* m_pCvarLightStyles;
	cvar_t* m_pCvarWireFrame;
	cvar_t* m_pCvarShadows;
	cvar_t* m_pCvarFlashLightDepthRes;
	cvar_t* m_pCvarLightmapDebug;
	cvar_t* m_pCvar3DSkybox;
	cvar_t* m_pCvarSunShadowsQuality;
	cvar_t* m_pCvarBlurShadows;
	cvar_t* m_pCvarBlacknwhite;


	cvar_t* lightgamma;
	cvar_t* texgamma;
	cvar_t* r_fullbright;
	cvar_t* gl_fog;
	cvar_t* gl_widescreen_yfov;

	Vector m_vDLightMins;
	Vector m_vDLightMaxs;

	float m_fSkySpeed;

	cl_entity_t* m_pRenderEntities[MAXRENDERENTS];
	int m_iNumRenderEntities;

	std::vector<std::unique_ptr<cl_dlight_t>> m_pDynLights;

	GL_ShadowMap* m_pSunShadowMap;

	mlight_t m_pModelLights[MAXRENDERENTS];
	int m_iNumModelLights;

	lightstyle_t m_pLightStyles[MAX_LIGHTSTYLES];
	int m_iLightStyleValue[MAX_LIGHTSTYLES];

	color24 m_pBlockLights[BLOCKLIGHTS_SIZE];
	int m_iNumLightmaps;

	color24 m_pEngineLightmaps[MAX_LIGHTMAPS * BLOCK_SIZE * BLOCK_SIZE];
	GLuint m_iEngineLightmapIndex;

	brushface_t** m_pSurfacePointersArray;

	clientsurfdata_t* m_pSurfaces;
	int m_iNumSurfaces;

	int m_iSkyTextures[6];

	int m_iBSPVertsCounter;
	int m_iBrushPolyCounter;  // bmodel poly counter
	int m_iStudioPolyCounter; // studiomodel poly counter

	double m_fShadowGenerationTime;
	double m_fMainWorldRenderTime;

	int m_iSunShadow_Strength;
	int m_iSunShadow_FadeDist;

	char m_szSkyName[64];
	char m_szMapName[64];

	texture_t m_pNormalTextureList[MAX_MAP_TEXTURES];
	int m_iNumTextures;


	GL_ShaderProgram *m_WorldShader;
	GL_ShaderProgram *m_WorldSolidShader;
	GL_ShaderProgram *m_DecalShader;
	GL_ShaderProgram *m_SimpleSkyboxShader;
	GL_ShaderProgram *m_FilterShader;
	GL_ShaderProgram *m_BlacknwhiteShader;

	enum worldshader_uniforms
	{
		world_projectionmatrix = 0,
		world_viewmatrix,
		world_modelmatrix,

		world_spotlight_texturematrix,

		world_spotlight,
		world_pointlight,
		world_shadow,
		world_onlyshadow,

		world_sundir,
		
		world_waterpolys,
		world_scrollingpolys,
		world_specular,
		world_fltime,

		world_alphatest,

		world_renderamt,
		world_rendercolor,

		world_light_pos,
		world_light_color,//red green blue
		world_light_radius,
		world_sunshadow_fadedist,
		world_sunshadow_strength,
		world_renderorigin,
		world_renderforward,
		world_renderright,

		world_lightmap_pass,
		world_texture_pass,

		world_fog_active,
		world_fogcolor,
		world_fogstart,
		world_fogend,

		world_lightgamma,
		world_texgamma,

		world_wireframe,

		world_shaderlocs_size, //must be last
	};

	enum worldshadersolid_uniforms
	{
		worldsolid_projviewmatrix = 0,
		worldsolid_modelmatrix,

		worldsolid_alphatest,

		worldsolid_light_pos,

		worldsolid_shaderlocs_size, //must be last
	};

	enum decalshader_uniforms
	{
		decal_projviewmatrix = 0,

		decal_wireframe,

		decal_shaderlocs_size,
	};

	enum skyboxshader_uniforms
	{
		skybox_projviewmatrix = 0,

		skybox_skyfog,
		skybox_fogcolor,

		skybox_shaderlocs_size,
	};

	GLuint m_WorldShader_locs[world_shaderlocs_size];

	GLuint m_WorldSolidShader_locs[worldsolid_shaderlocs_size];

	GLuint m_SimpleSkyboxShader_locs[skybox_shaderlocs_size];

	GLuint m_DecalShader_locs[decal_shaderlocs_size];

	glm::mat4 m_ProjectionMatrix; //	fov, aspect, near, far
	glm::mat4 m_ViewMatrix;  //	camera position, camera angles
	glm::mat4 m_ModelMatrix;		//	moving entities


public:
	std::vector<std::unique_ptr<customdecal_t>> m_pDecals;
	std::vector<std::unique_ptr<customdecal_t>> m_pStaticDecals;
	std::vector<std::unique_ptr<decal_msg_cache>> m_pMsgCache;
	std::vector<std::unique_ptr<decalgroup_t>> m_pDecalGroups;

	Vector m_vDecalMins;
	Vector m_vDecalMaxs;
};
extern CBSPRenderer gBSPRenderer;