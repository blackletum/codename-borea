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

void GL_VertexArrayObject::SetVertexAttributes(const gl_vertattriblist_t* attributelist)
{
	for (int i = 0; i < attributelist->numattribs; i++)
	{
		const gl_vertattrib_t* attribute = &attributelist->attribs[i];
		glEnableVertexAttribArray(attribute->index);

		if (attribute->integer_attrib)
			glVertexAttribIPointer(attribute->index, attribute->numelements, attribute->type, attribute->structsize, (const void*)attribute->offset);
		else
			glVertexAttribPointer(attribute->index, attribute->numelements, attribute->type, attribute->normalize, attribute->structsize, (const void*)attribute->offset);

		attribute++;
	}
}