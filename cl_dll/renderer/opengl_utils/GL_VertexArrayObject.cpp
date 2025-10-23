#include "PlatformHeaders.h"
#include "Platform.h"
#include "hud.h"
#include "cl_util.h"

#include "renderer/rendererdefs.h"

#include "GL_VertexArrayObject.h"

GL_VertexArrayObject* GL_VertexArrayObject::m_pCurrentBoundVAO = nullptr;

GL_VertexArrayObject::GL_VertexArrayObject()
{
	glGenVertexArrays(1, &m_uiVAOIndex);
}

GL_VertexArrayObject::~GL_VertexArrayObject()
{
	glDeleteVertexArrays(1, &m_uiVAOIndex);
}

#ifdef _DEBUG

void GL_VertexArrayObject::BindVAO()
{
	if (m_pCurrentBoundVAO == this)
		return;

	glBindVertexArray(m_uiVAOIndex);
	m_pCurrentBoundVAO = this;
}

void GL_VertexArrayObject::ResetVAOBinding()
{
	glBindVertexArray(0);
	m_pCurrentBoundVAO = nullptr;
}

#endif