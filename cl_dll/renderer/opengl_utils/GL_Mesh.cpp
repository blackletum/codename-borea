
#include "GL_Buffers.h"
#include "GL_ShaderProgram.h"
#include "GL_VertexArrayObject.h"

#include "GL_Mesh.h"

static GL_Mesh* s_CurrentBoundMesh;

static const gl_vertattrib_t basicattrib { GL_ShaderProgram::VertexPos, 3, GL_FLOAT, sizeof(float) * 3, 0, false, false};
static const gl_vertattriblist_t basicattriblist{&basicattrib, 1};

GL_Mesh::GL_Mesh(uint32_t numverts, const gl_vertattriblist_t* vertattribute, bool bStatic, void* vertdata)
{
	if (!vertattribute)
		vertattribute = &basicattriblist;

	m_pVAO = new GL_VertexArrayObject;
	m_pBuffer = new GL_BufferHandler;

	m_pVAO->BindVAO();
	m_pBuffer->Bind(GL_BufferHandler::ArrayBuffer);

	m_uiVertSize = vertattribute->attribs[0].structsize;

	m_pBuffer->BufferData(GL_BufferHandler::ArrayBuffer, m_uiVertSize * numverts, vertdata, bStatic ? GL_BufferHandler::StaticDraw : GL_BufferHandler::DynamicDraw);

	m_pVAO->SetVertexAttributes(vertattribute);

	GL_VertexArrayObject::ResetVAOBinding();
	GL_BufferHandler::ResetBufferBinding(GL_BufferHandler::ArrayBuffer);
}

GL_Mesh::~GL_Mesh()
{
	delete m_pVAO;
	delete m_pBuffer;

	if (s_CurrentBoundMesh == this)
		s_CurrentBoundMesh = nullptr;
}

void GL_Mesh::BindMesh() 
{
	m_pVAO->BindVAO();
	s_CurrentBoundMesh = this;
}

void GL_Mesh::SetDrawMode(uint32_t drawmode)
{
	m_eDrawMode = drawmode;
}
void GL_Mesh::SetIndices(uint32_t* indices, uint32_t numindices)
{
	m_pIndiceBuffer = new GL_BufferHandler;

	m_pVAO->BindVAO();

	m_pBuffer->Bind(GL_BufferHandler::ArrayBuffer);

	m_pIndiceBuffer->Bind(GL_BufferHandler::ElementArrayBuffer);
	m_pIndiceBuffer->BufferData(GL_BufferHandler::ElementArrayBuffer, sizeof(uint32_t) * numindices, indices, GL_BufferHandler::StaticDraw);

	GL_VertexArrayObject::ResetVAOBinding();
	GL_BufferHandler::ResetBufferBinding(GL_BufferHandler::ArrayBuffer);
	GL_BufferHandler::ResetBufferBinding(GL_BufferHandler::ElementArrayBuffer);
}

void GL_Mesh::DrawArrays(uint32_t startvert, uint32_t numverts)
{
	glDrawArrays(m_eDrawMode, startvert, numverts);
}
void GL_Mesh::MultiDrawArrays(int* startverts, int* numverts, uint32_t numdraws)
{
	glMultiDrawArrays(m_eDrawMode, startverts, numverts, numdraws);
}
void GL_Mesh::DrawElements(uint32_t startindex, uint32_t numindeces)
{
	if (!m_pIndiceBuffer)
		return;

	glDrawElements(m_eDrawMode, numindeces, GL_UNSIGNED_INT, (const void*)(startindex * sizeof(uint32_t)));
}

void GL_Mesh::UnbindMesh()
{
	GL_VertexArrayObject::ResetVAOBinding();
	s_CurrentBoundMesh = nullptr;
}
GL_Mesh* GL_Mesh::GetBoundMesh()
{
	return s_CurrentBoundMesh;
}

void GL_Mesh::ModifyMesh(void* vertdata, uint32_t numverts, uint32_t offset)
{
	UnbindMesh();
	m_pBuffer->Bind(GL_BufferHandler::ArrayBuffer);
	m_pBuffer->BufferSubData(GL_BufferHandler::ArrayBuffer, offset * m_uiVertSize, numverts * m_uiVertSize, vertdata);
	GL_BufferHandler::ResetBufferBinding(GL_BufferHandler::ArrayBuffer);
	BindMesh();
}

