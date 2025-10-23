#include "PlatformHeaders.h"
#include "Platform.h"
#include "hud.h"
#include "cl_util.h"

#include "renderer/rendererdefs.h"
#include "renderer/bsprenderer.h"
#include "GL_ShaderProgram.h"
#include "GL_Buffers.h"

#include "GL_Buffers.h"
#include "GL_FBO.h"
#include "GL_ShadowMap.h"
#include "GL_StateHandler.h"

GL_FBOHandler* GL_ShadowMap::m_pMainShadowFBO = nullptr;
std::vector<GL_RBOHandler*> GL_ShadowMap::m_pShadowRBOs;


std::vector<std::unique_ptr<GL_ShadowMap>> GL_ShadowMap::m_vShadowMapList;

GL_ShadowMap* GL_ShadowMap::AllocateShadowMap(GL_TextureType target, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, bool canuseblur)
{
	for (auto& shadowmap : m_vShadowMapList)
	{
		if (!shadowmap->m_bInUse && 
			shadowmap->GetTextureType() == target && 
			shadowmap->m_iWidth == width && 
			shadowmap->m_iHeight == height &&
			shadowmap->m_TexInfo.internalformat == internalformat)
		{
			shadowmap->m_bInUse = true;
			return shadowmap.get();
		}
	}

	gl_texturecreationinfo_t textureinfo;
	textureinfo.SetInfo(std::string(), target, internalformat, width, height, depth, format, type);

	return new GL_ShadowMap(&textureinfo, canuseblur);
}

void GL_ShadowMap::DeAllocateShadowMap(GL_ShadowMap* pSM)
{
	//	MEMORY LEAK WARNING! need to automatically delete inactive shadowmaps from gpu after a period of time,
	//	can't depend on shadowmaps getting cleared on level load.
	pSM->m_bInUse = false;
}

void GL_ShadowMap::CheckFBO(GLsizei width, GLsizei height)
{
	if (m_pShadowRBOs.empty())
	{
		GL_RBOHandler* shadowrbo = new GL_RBOHandler();
		shadowrbo->Bind();
		shadowrbo->RenderBufferStorage(GL_DEPTH_COMPONENT24, width, height);
		m_pShadowRBOs.push_back(shadowrbo);
	}
	else
	{
		bool bNeedsNewRenderBuffer = true;
		for (auto shadowrbo : m_pShadowRBOs)
		{
			GLsizei m_iwidth, m_iheight;
			shadowrbo->GetWidthHeight(&m_iwidth, &m_iheight);
			if (width == m_iwidth && height == m_iheight)
			{
				bNeedsNewRenderBuffer = false;
				break;
			}
		}
		if (bNeedsNewRenderBuffer)
		{
			GL_RBOHandler* shadowrbo = new GL_RBOHandler();
			shadowrbo->Bind();
			shadowrbo->RenderBufferStorage(GL_DEPTH_COMPONENT16, width, height);
			m_pShadowRBOs.push_back(shadowrbo);
		}
	}
}

GL_ShadowMap::GL_ShadowMap(gl_texturecreationinfo_t* texinfo, bool canuseblur)
	: GL_TextureHandler(texinfo)
{
	CheckFBO(texinfo->width, texinfo->height);

	for (auto shadowrbo : m_pShadowRBOs)
	{
		GLsizei width, height;
		shadowrbo->GetWidthHeight(&width, &height);
		if (width == m_iWidth && height == m_iHeight)
		{
			m_pShadowRBO = shadowrbo;
			break;
		}
	}

	m_bInUse = true;
	m_bCanBlur = canuseblur;

	std::unique_ptr<GL_ShadowMap> ptr(this);
	m_vShadowMapList.push_back(std::move(ptr));
	gl_texturecreationinfo_t dummyinfo = *texinfo;
	dummyinfo.texturetype = _2DTexture_Storage;
	m_pDummyTexture = new GL_TextureHandler(&dummyinfo);
}

GL_ShadowMap::~GL_ShadowMap()
{
	// empty, automatically calls ~GL_TextureHandler()
}

void GL_ShadowMap::InitRendering(Vector cleancolor, GLsizei layer)
{
	m_iCurrentLayer = layer;

	if (m_TexInfo.texturetype == _2DTexture || m_TexInfo.texturetype == _2DTexture_Storage)
	{
		m_pMainShadowFBO->FramebufferTexture2D(GL_FBOHandler::DrawFramebuffer, GL_FBOHandler::ColorAttachment, GL_TEXTURE_2D, m_uiTextureHandle, 0);
	}
	else if (m_TexInfo.texturetype == _2DTextureArray)
	{
		m_pMainShadowFBO->FramebufferTextureLayer(GL_FBOHandler::DrawFramebuffer, GL_FBOHandler::ColorAttachment, GL_TEXTURE_2D_ARRAY, m_uiTextureHandle, 0, layer);
	}
	else
	{
#ifdef _DEBUG
		assert(m_iCubeMapIteration <= 5);
#endif
		m_pMainShadowFBO->FramebufferTexture2D(GL_FBOHandler::DrawFramebuffer, GL_FBOHandler::ColorAttachment, GL_TEXTURE_CUBE_MAP_POSITIVE_X + m_iCubeMapIteration++, m_uiTextureHandle, 0);
	}

	m_pMainShadowFBO->FramebufferRenderbuffer(GL_FBOHandler::DrawFramebuffer, GL_FBOHandler::DepthAttachment, m_pShadowRBO);

	glViewport(GL_ZERO, GL_ZERO, m_iWidth, m_iHeight);

	// Completely clear everything
	glClearColor((int)cleancolor.x, (int)cleancolor.y, GL_ZERO, GL_ZERO);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


}

void GL_ShadowMap::BlurShadows()
{

	m_pMainShadowFBO->Bind(GL_FBOHandler::DrawFramebuffer);
	
	gBSPRenderer.m_FilterShader->Bind();
	gBSPRenderer.m_FilterShader->Uniform1i(gBSPRenderer.m_FilterShader->GetUniformLoc("texture_"), 0);
	gBSPRenderer.m_FilterShader->Uniform1i(gBSPRenderer.m_FilterShader->GetUniformLoc("horizontal"), 1);
	
	gBSPRenderer.m_pScreenQuadVAO->BindVAO();
	
	g_GlobalGLState.SetDepthTest(false);
	g_GlobalGLState.SetCullFace(false);

	for (auto &shadowmap : m_vShadowMapList)
	{
		if (!shadowmap->m_bInUse || !shadowmap->m_bCanBlur)
			continue;

		//only blur closest light sources, my poor attempt at optimization
		float distance = (gBSPRenderer.m_vRenderOrigin - shadowmap->position).Length();
		if (distance > 512)
			continue;

		m_pMainShadowFBO->FramebufferTexture2D(GL_FBOHandler::DrawFramebuffer, GL_FBOHandler::ColorAttachment, GL_TEXTURE_2D, shadowmap->m_pDummyTexture->GetTextureID(), 0);
	
		glViewport(0, 0, shadowmap->m_iWidth, shadowmap->m_iHeight);

		if (shadowmap->GetTextureType() == _2DTexture || shadowmap->GetTextureType() == _2DTexture_Storage)
		{
			gBSPRenderer.BindGLTexture(GL_TEXTURE0, shadowmap->m_uiTextureHandle);
			gBSPRenderer.m_FilterShader->Uniform1i(gBSPRenderer.m_FilterShader->GetUniformLoc("gaussian_pass"), 1);
	
			//blur shadow into m_pDummyTexture
			glDrawArrays(GL_TRIANGLES, 0, 6);
	
			m_pMainShadowFBO->FramebufferTexture2D(GL_FBOHandler::DrawFramebuffer, GL_FBOHandler::ColorAttachment, GL_TEXTURE_2D, shadowmap->GetTextureID(), 0);
	
			gBSPRenderer.BindGLTexture(GL_TEXTURE0, shadowmap->m_pDummyTexture->GetTextureID());
			gBSPRenderer.m_FilterShader->Uniform1i(gBSPRenderer.m_FilterShader->GetUniformLoc("gaussian_pass"), 0);
	
			//now render blurred shadow into the actual shadowmap texture
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}
		else if (shadowmap->GetTextureType() == _CubeMap || shadowmap->GetTextureType() == _CubeMap_Storage)
		{
			gBSPRenderer.BindGLTexture(GL_TEXTURE1, shadowmap->m_uiTextureHandle);
			gBSPRenderer.BindGLTexture(GL_TEXTURE0, shadowmap->m_pDummyTexture->GetTextureID());
			for (int i = 0; i < 6; i++)
			{
				m_pMainShadowFBO->FramebufferTexture2D(GL_FBOHandler::DrawFramebuffer, GL_FBOHandler::ColorAttachment, GL_TEXTURE_2D, shadowmap->m_pDummyTexture->GetTextureID(), 0);
		
				gBSPRenderer.m_FilterShader->Uniform1i(gBSPRenderer.m_FilterShader->GetUniformLoc("gaussian_pass"), 1);
				gBSPRenderer.m_FilterShader->Uniform1i(gBSPRenderer.m_FilterShader->GetUniformLoc("cubemap"), 1);
				gBSPRenderer.m_FilterShader->Uniform1i(gBSPRenderer.m_FilterShader->GetUniformLoc("cubemap_layer"), i);
		
				// blur shadow into m_pDummyTexture
				glDrawArrays(GL_TRIANGLES, 0, 6);
		
				m_pMainShadowFBO->FramebufferTexture2D(GL_FBOHandler::DrawFramebuffer, GL_FBOHandler::ColorAttachment, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, shadowmap->GetTextureID(), 0);
		
				gBSPRenderer.m_FilterShader->Uniform1i(gBSPRenderer.m_FilterShader->GetUniformLoc("gaussian_pass"), 0);
				gBSPRenderer.m_FilterShader->Uniform1i(gBSPRenderer.m_FilterShader->GetUniformLoc("cubemap"), 0);
		
				// now render blurred shadow into the actual shadowmap texture
				glDrawArrays(GL_TRIANGLES, 0, 6);
			}
		}
	}
	
	g_GlobalGLState.SetDepthTest(true);
	g_GlobalGLState.SetCullFace(true);
	
	GL_FBOHandler::ResetToMainFBO();
	glViewport(GL_ZERO, GL_ZERO, ScreenWidth, ScreenHeight);

}

void GL_ShadowMap::FinishRendering()
{
	m_iCubeMapIteration = 0;
}

void GL_ShadowMap::StartShadowMapping()
{
	if (!m_pMainShadowFBO)
		m_pMainShadowFBO = new GL_FBOHandler();

	m_pMainShadowFBO->Bind(GL_FBOHandler::DrawFramebuffer);

	m_pMainShadowFBO->FramebufferTexture2D(GL_FBOHandler::DrawFramebuffer, GL_FBOHandler::ColorAttachment, GL_TEXTURE_2D, 0, 0); // clear that shit

}

void GL_ShadowMap::EndShadowMapping()
{
	if (gBSPRenderer.m_pCvarBlurShadows->value > 0)
		BlurShadows();

	GL_FBOHandler::ResetToMainFBO();

	glViewport(GL_ZERO, GL_ZERO, ScreenWidth, ScreenHeight);

}