#pragma once

#include "renderer/rendererdefs.h"

class GL_RBOHandler
{
public:
	GL_RBOHandler();
	~GL_RBOHandler();

	void Bind();
	void RenderBufferStorage(const GLenum& internalformat, const GLsizei& width, const GLsizei& height);

	GLuint GetID() const { return m_uiRenderBuffer; };
	void GetWidthHeight(GLsizei* width, GLsizei* height);

	static __forceinline GLuint GetNumRenderBuffers() { return m_uiNumRenderBuffers; };


private:
	GLuint m_uiRenderBuffer = 0;
	GLsizei m_iWidth = 0, m_iHeight = 0;

	static GLuint m_uiCurrentRenderBuffer;
	static GLuint m_uiNumRenderBuffers;
};

class GL_FBOHandler
{
public:
	GL_FBOHandler();
	~GL_FBOHandler();

	enum fbo_targets
	{
		DrawFramebuffer = GL_DRAW_FRAMEBUFFER,
		ReadFramebuffer = GL_READ_FRAMEBUFFER,
		Framebuffer = GL_FRAMEBUFFER
	};

	enum fbo_attachments
	{
		AttachmentNotSet = 0,
		ColorAttachment = GL_COLOR_ATTACHMENT0,
		DepthAttachment = GL_DEPTH_ATTACHMENT,
		StencilAttachment = GL_STENCIL_ATTACHMENT,
		DepthStencilAttachment = GL_DEPTH_STENCIL_ATTACHMENT,
	};

	#ifdef _DEBUG

	void Bind(const fbo_targets& target);
	void FramebufferRenderbuffer(const fbo_targets& target, const fbo_attachments& attachment, const GL_RBOHandler* renderbuffer);
	void FramebufferTexture2D(const fbo_targets& target, const fbo_attachments& attachment, const GLenum& textarget, const GLuint& texture, const GLint& level);
	void FramebufferTexture3D(const fbo_targets& target, const fbo_attachments& attachment, const GLenum& textarget, const GLuint& texture, const GLint& level, const GLint& layer);
	void FramebufferTextureLayer(const fbo_targets& target, const fbo_attachments& attachment, const GLenum& textarget, const GLuint& texture, const GLint& level, const GLint& layer);

	static void SetMainGameFBO(const GLuint &fbo);
	static void ResetToMainFBO();

	#else

	__forceinline void Bind(const fbo_targets& target) noexcept { 
		glBindFramebufferEXT(target, m_uiFrameBuffer); 
	};
	__forceinline void FramebufferRenderbuffer(const fbo_targets& target, const fbo_attachments& attachment, const GL_RBOHandler* renderbuffer) noexcept {
		if (renderbuffer == nullptr)
		{
			glFramebufferRenderbufferEXT(target, attachment, GL_RENDERBUFFER, 0);
			return;
		}

		glFramebufferRenderbufferEXT(target, attachment, GL_RENDERBUFFER, renderbuffer->GetID());
	};
	__forceinline void FramebufferTexture2D(const fbo_targets& target, const fbo_attachments& attachment, const GLenum& textarget, const GLuint& texture, const GLint& level) noexcept { 
		glFramebufferTexture2DEXT(target, attachment, textarget, texture, level); 
	};
	__forceinline void FramebufferTexture3D(const fbo_targets& target, const fbo_attachments& attachment, const GLenum& textarget, const GLuint& texture, const GLint& level, const GLint& layer) noexcept { 
		glFramebufferTexture3DEXT(target, attachment, textarget, texture, level, layer); 
	};
	__forceinline void FramebufferTextureLayer(const fbo_targets& target, const fbo_attachments& attachment, const GLenum& textarget, const GLuint& texture, const GLint& level, const GLint& layer) noexcept { 
		glFramebufferTextureLayerEXT(target, attachment, texture, level, layer); 
	};

	static __forceinline void SetMainGameFBO(const GLuint& fbo) noexcept { m_uiMainGameFBO = fbo; };
	static __forceinline void ResetToMainFBO() noexcept { glBindFramebufferEXT(GL_FRAMEBUFFER, m_uiMainGameFBO); };

	#endif


	__forceinline GLuint GetID() noexcept { return m_uiFrameBuffer; };
	static __forceinline GLuint GetNumFrameBuffers() noexcept { return m_uiNumFrameBuffers; };

private:
	GLuint m_uiFrameBuffer = 0;
	GLuint m_uiBoundRenderBuffer = 0;
	GLuint m_uiBoundTexture = 0;
	GLenum m_uiBoundTextureTarget = 0;
	GLuint m_uiBoundTextureMipmapLevel = 0;
	GLuint m_uiBoundTextureLayer = 0;
	fbo_attachments m_uiBoundTextureAttachment = AttachmentNotSet;

	static GLuint m_uiCurrentDrawFrameBuffer;
	static GLuint m_uiCurrentReadFrameBuffer;
	static GLuint m_uiNumFrameBuffers;
	static GLuint m_uiMainGameFBO;
	static bool m_bMainFBOSet;
};