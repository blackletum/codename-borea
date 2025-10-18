#pragma once

#include "PlatformHeaders.h"
#include "Platform.h"
#include "hud.h"
#include "cl_util.h"

#include "renderer/rendererdefs.h"

#include "GL_TextureHandler.h"
#include "GL_VertexArrayObject.h"

////////////////////////////////////
//	shadow map class. can be of 2 types: 2d texture and cubemap. a cubemap shadowmap can render all 6 faces
//	by calling InitRendering 6 times. 
// 
//	it must be correctly reset with FinishRendering after youre done rendering to it.

//	salsa: todo: make this class derive from a texture class, GL_TextureHandler or whatever

class GL_ShadowMap : public GL_TextureHandler
{
public:


	GL_ShadowMap(gl_texturecreationinfo_t *texinfo, bool canuseblur);
	~GL_ShadowMap();

	void InitRendering(Vector cleancolor = Vector(0, 0, 0), GLsizei layer = 0);
	void FinishRendering();

	__forceinline void SetPosition(Vector pos) { position = pos; }

	static void BlurShadows();

	static void StartShadowMapping();
	static void EndShadowMapping();

	static GL_ShadowMap* AllocateShadowMap(GL_TextureType target, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, bool canuseblur = true);
	static void DeAllocateShadowMap(GL_ShadowMap* pSM);
	static void ClearAllShadowMaps() { m_vShadowMapList.clear(); };

private:
	static void CheckFBO(GLsizei width, GLsizei height);
	bool m_bInUse = true;

	GLuint m_iCubeMapIteration = 0; // to render the 6 faces, gets reset in FinishRendering
	GLuint m_iCurrentLayer = 0; //for _2DTexture_Array shadowmap

	Vector position; //kinda not nice but whatever, cant blur all shadows in the entire damn world

	GL_TextureHandler* m_pDummyTexture = nullptr;
	GL_RBOHandler* m_pShadowRBO = nullptr;
	bool m_bCanBlur = false;

	static GL_FBOHandler* m_pMainShadowFBO;
	static std::vector<GL_RBOHandler*> m_pShadowRBOs;

	static std::vector<std::unique_ptr<GL_ShadowMap>> m_vShadowMapList;
};