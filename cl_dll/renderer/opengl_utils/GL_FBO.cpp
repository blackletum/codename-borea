#include "PlatformHeaders.h"
#include "Platform.h"
#include "hud.h"
#include "cl_util.h"

#include "renderer/rendererdefs.h"

#include "GL_FBO.h"


GLuint GL_FBOHandler::m_uiCurrentDrawFrameBuffer = 0;
GLuint GL_FBOHandler::m_uiCurrentReadFrameBuffer = 0;
GLuint GL_RBOHandler::m_uiCurrentRenderBuffer = 0;

GLuint GL_FBOHandler::m_uiNumFrameBuffers = 0;
GLuint GL_RBOHandler::m_uiNumRenderBuffers = 0;

GLuint GL_FBOHandler::m_uiMainGameFBO = 0;
bool GL_FBOHandler::m_bMainFBOSet = false;

GL_FBOHandler::GL_FBOHandler() 
{ 
	glGenFramebuffers(1, &m_uiFrameBuffer); 
	m_uiNumFrameBuffers++;
};


GL_FBOHandler::~GL_FBOHandler() 
{ 
	glDeleteFramebuffersEXT(1, &m_uiFrameBuffer);
	m_uiNumFrameBuffers--;
};
#ifdef _DEBUG

void GL_FBOHandler::Bind(const fbo_targets& target)
{
	if (target == GL_FBOHandler::Framebuffer)
	{
		if (m_uiCurrentDrawFrameBuffer == m_uiFrameBuffer && m_uiCurrentReadFrameBuffer == m_uiFrameBuffer)
			return;

		m_uiCurrentDrawFrameBuffer = m_uiCurrentReadFrameBuffer = m_uiFrameBuffer;
	}
	else if (target == GL_FBOHandler::DrawFramebuffer)
	{
		if (m_uiCurrentDrawFrameBuffer == m_uiFrameBuffer)
			return;

		m_uiCurrentDrawFrameBuffer = m_uiFrameBuffer;
	}
	else
	{
		if (m_uiCurrentReadFrameBuffer == m_uiFrameBuffer)
			return;

		m_uiCurrentReadFrameBuffer = m_uiFrameBuffer;
	}

	glBindFramebufferEXT(target, m_uiFrameBuffer);

};
void GL_FBOHandler::FramebufferRenderbuffer(const fbo_targets& target, const fbo_attachments& attachment, const GL_RBOHandler* renderbuffer)
{

	if (target == GL_FBOHandler::Framebuffer)
	{
		assert(m_uiCurrentDrawFrameBuffer == m_uiFrameBuffer && m_uiCurrentReadFrameBuffer == m_uiFrameBuffer);
	}
	else if (target == GL_FBOHandler::DrawFramebuffer)
	{
		assert(m_uiCurrentDrawFrameBuffer == m_uiFrameBuffer);
	}
	else
	{
		assert(m_uiCurrentReadFrameBuffer == m_uiFrameBuffer);
	}

	if (renderbuffer == nullptr)
	{
		m_uiBoundRenderBuffer = 0;
		glFramebufferRenderbufferEXT(target, attachment, GL_RENDERBUFFER, 0);
		return;
	}
	if (m_uiBoundRenderBuffer == renderbuffer->GetID())
		return;

	m_uiBoundRenderBuffer = renderbuffer->GetID();

	glFramebufferRenderbufferEXT(target, attachment, GL_RENDERBUFFER, renderbuffer->GetID());

}
void GL_FBOHandler::FramebufferTexture2D(const fbo_targets& target, const fbo_attachments& attachment, const GLenum& textarget, const GLuint& texture, const GLint& level)
{
	if (m_uiBoundTexture == texture && m_uiBoundTextureAttachment == attachment && m_uiBoundTextureMipmapLevel == level &&  m_uiBoundTextureTarget == textarget)
			return;

	if (target == GL_FBOHandler::Framebuffer)
	{
		assert(m_uiCurrentDrawFrameBuffer == m_uiFrameBuffer && m_uiCurrentReadFrameBuffer == m_uiFrameBuffer);
	}
	else if (target == GL_FBOHandler::DrawFramebuffer)
	{
		assert(m_uiCurrentDrawFrameBuffer == m_uiFrameBuffer);
	}
	else
	{
		assert(m_uiCurrentReadFrameBuffer == m_uiFrameBuffer);
	}

	m_uiBoundTexture = texture;
	m_uiBoundTextureTarget = textarget;
	m_uiBoundTextureAttachment = attachment;
	m_uiBoundTextureMipmapLevel = level;


	glFramebufferTexture2DEXT(target, attachment, textarget, texture, level);

}

void GL_FBOHandler::FramebufferTexture3D(const fbo_targets& target, const fbo_attachments& attachment, const GLenum& textarget, const GLuint& texture, const GLint& level, const GLint& layer)
{

	if (m_uiBoundTexture == texture && m_uiBoundTextureAttachment == attachment && m_uiBoundTextureMipmapLevel == level && m_uiBoundTextureTarget == textarget && m_uiBoundTextureLayer == layer)
		return;

	if (target == GL_FBOHandler::Framebuffer)
	{
		assert(m_uiCurrentDrawFrameBuffer == m_uiFrameBuffer && m_uiCurrentReadFrameBuffer == m_uiFrameBuffer);
	}
	else if (target == GL_FBOHandler::DrawFramebuffer)
	{
		assert(m_uiCurrentDrawFrameBuffer == m_uiFrameBuffer);
	}
	else
	{
		assert(m_uiCurrentReadFrameBuffer == m_uiFrameBuffer);
	}

	m_uiBoundTexture = texture;
	m_uiBoundTextureTarget = textarget;
	m_uiBoundTextureAttachment = attachment;
	m_uiBoundTextureMipmapLevel = level;
	m_uiBoundTextureLayer == layer;


	glFramebufferTexture3DEXT(target, attachment, textarget, texture, level, layer);
}

void GL_FBOHandler::FramebufferTextureLayer(const fbo_targets& target, const fbo_attachments& attachment, const GLenum& textarget, const GLuint& texture, const GLint& level, const GLint& layer)
{
	if (m_uiBoundTexture == texture && m_uiBoundTextureAttachment == attachment && m_uiBoundTextureMipmapLevel == level && m_uiBoundTextureTarget == textarget && m_uiBoundTextureLayer == layer)
		return;

	if (target == GL_FBOHandler::Framebuffer)
	{
		assert(m_uiCurrentDrawFrameBuffer == m_uiFrameBuffer && m_uiCurrentReadFrameBuffer == m_uiFrameBuffer);
	}
	else if (target == GL_FBOHandler::DrawFramebuffer)
	{
		assert(m_uiCurrentDrawFrameBuffer == m_uiFrameBuffer);
	}
	else
	{
		assert(m_uiCurrentReadFrameBuffer == m_uiFrameBuffer);
	}

	m_uiBoundTexture = texture;
	m_uiBoundTextureTarget = textarget;
	m_uiBoundTextureAttachment = attachment;
	m_uiBoundTextureMipmapLevel = level;
	m_uiBoundTextureLayer = layer;


	glFramebufferTextureLayerEXT(target, attachment, texture, level, layer);
}

void GL_FBOHandler::SetMainGameFBO(const GLuint& fbo)
{
	if (m_bMainFBOSet)
		return;

	m_uiMainGameFBO = fbo;
	m_bMainFBOSet = true;
}
void GL_FBOHandler::ResetToMainFBO()
{
	if (m_uiCurrentDrawFrameBuffer == m_uiMainGameFBO && m_uiCurrentReadFrameBuffer == m_uiMainGameFBO)
		return;

	glBindFramebufferEXT(GL_FRAMEBUFFER, m_uiMainGameFBO);
	m_uiCurrentDrawFrameBuffer = m_uiCurrentReadFrameBuffer = m_uiMainGameFBO;
}

#endif


GL_RBOHandler::GL_RBOHandler()
{
	glGenRenderbuffersEXT(1, &m_uiRenderBuffer);
	m_uiNumRenderBuffers++;
}
GL_RBOHandler::~GL_RBOHandler()
{
	glDeleteRenderbuffersEXT(1, &m_uiRenderBuffer);
	m_uiNumRenderBuffers--;
}



void GL_RBOHandler::Bind()
{
	if (m_uiCurrentRenderBuffer == m_uiRenderBuffer)
		return;

	m_uiCurrentRenderBuffer = m_uiRenderBuffer;
	glBindRenderbufferEXT(GL_RENDERBUFFER, m_uiRenderBuffer);
}

void GL_RBOHandler::RenderBufferStorage(const GLenum& internalformat, const GLsizei& width, const GLsizei& height)
{
	assert(m_uiCurrentRenderBuffer == m_uiRenderBuffer);

	glRenderbufferStorageEXT(GL_RENDERBUFFER, internalformat, width, height);
	m_iWidth = width;
	m_iHeight = height;
}

void GL_RBOHandler::GetWidthHeight(GLsizei* width, GLsizei* height)
{
	if (width)
		*width = m_iWidth;
	if (height)
		*height = m_iHeight;
}