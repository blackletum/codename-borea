#include "PlatformHeaders.h"
#include "Platform.h"
#include "hud.h"
#include "cl_util.h"

#include "renderer/rendererdefs.h"

#include "GL_VertexArrayObject.h"

GLuint GL_VertexArrayObject::m_uiCurrentBoundVAO = 0;

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
	if (m_uiCurrentBoundVAO == m_uiVAOIndex)
		return;

	glBindVertexArray(m_uiVAOIndex);
	m_uiCurrentBoundVAO = m_uiVAOIndex;
}

void GL_VertexArrayObject::ResetVAOBinding()
{
	glBindVertexArray(0);
	m_uiCurrentBoundVAO = 0;
}

#endif