#include "GL_Buffers.h"

GLuint GL_BufferHandler::m_uiCurrentBoundArray = 0;
GLuint GL_BufferHandler::m_uiCurrentBoundElementArray = 0;
GLuint GL_BufferHandler::m_uiCurrentBoundUBO = 0;

GLuint GL_BufferHandler::m_uiNumBuffers = 0;
GLuint GL_BufferHandler::m_uiApproximated_VRAM_Bytes = 0;

GL_BufferHandler::GL_BufferHandler()
{
	glGenBuffers(1, &m_uiBufferIndex);
	GL_BufferHandler::m_uiNumBuffers++;
}
GL_BufferHandler::GL_BufferHandler(vbo_targets target, vbo_usage usage, int bytesize)
{
	glGenBuffers(1, &m_uiBufferIndex);
	GL_BufferHandler::m_uiNumBuffers++;

	Bind(target);
	BufferData(target, bytesize, nullptr, usage);

	GL_BufferHandler::ResetBufferBinding(target);
}
GL_BufferHandler::~GL_BufferHandler()
{
	glDeleteBuffers(1, &m_uiBufferIndex);

	GL_BufferHandler::m_uiNumBuffers--;
	GL_BufferHandler::m_uiApproximated_VRAM_Bytes -= m_uiBufferSize;
}

#ifdef _DEBUG

void GL_BufferHandler::Bind(const vbo_targets& target)
{
	switch (target)
	{
		case ArrayBuffer:
		{
			if (m_uiCurrentBoundArray == this->m_uiBufferIndex)
				return;
			else
				m_uiCurrentBoundArray = this->m_uiBufferIndex;
			break;
		}
		case ElementArrayBuffer:
		{
			if (m_uiCurrentBoundElementArray == this->m_uiBufferIndex)
				return;
			else
				m_uiCurrentBoundElementArray = this->m_uiBufferIndex;
			break;
		}
		case UniformBuffer:
		{
			if (m_uiCurrentBoundUBO == this->m_uiBufferIndex)
				return;
			else
				m_uiCurrentBoundUBO = this->m_uiBufferIndex;
			break;
		}
	}

	glBindBuffer(target, this->m_uiBufferIndex);
	
}

void GL_BufferHandler::BindBase(const vbo_targets& target, const GLuint index)
{
	switch (target)
	{
		case UniformBuffer:
		{
			break;
		}
		default:
			assert(0);
	}

	glBindBufferBase(target, index, this->m_uiBufferIndex);
}

void GL_BufferHandler::BindRange(const vbo_targets& target, const GLuint index, const GLsizei offset, const GLsizei size)
{
	switch (target)
	{
	case UniformBuffer:
	{
		break;
	}
	default:
		assert(0);
	}

	assert(offset < m_uiBufferSize && size <= m_uiBufferSize && (offset + size) <= m_uiBufferSize);

	glBindBufferRange(target, index, this->m_uiBufferIndex, offset, size);
}

void GL_BufferHandler::BufferData(const vbo_targets& target, const GLsizeiptr& size, const void* data, const vbo_usage& usage)
{
	assert(!m_bIsImmutable);

	switch (target)
	{
		case ArrayBuffer:
		{
			assert(m_uiCurrentBoundArray == this->m_uiBufferIndex);
			break;
		}
		case ElementArrayBuffer:
		{
			assert(m_uiCurrentBoundElementArray == this->m_uiBufferIndex);
			break;
		}
		case UniformBuffer:
		{
			assert(m_uiCurrentBoundUBO == this->m_uiBufferIndex);
			break;
		}
	}

	glBufferData(target, size, data, usage);

	if (m_uiBufferSize)
	{
		m_uiApproximated_VRAM_Bytes = m_uiApproximated_VRAM_Bytes - (m_uiBufferSize - size);
	}
	else
		m_uiApproximated_VRAM_Bytes += size;

	m_uiBufferSize = size;
	m_uiBufferUsage = usage;
}
void GL_BufferHandler::BufferStorage(const vbo_targets& target, const GLsizeiptr& size, const void* data, const GLbitfield& flags)
{
	assert(!m_bIsImmutable);

	switch (target)
	{
		case ArrayBuffer:
		{
			assert(m_uiCurrentBoundArray == this->m_uiBufferIndex);
			break;
		}
		case ElementArrayBuffer:
		{
			assert(m_uiCurrentBoundElementArray == this->m_uiBufferIndex);
			break;
		}
		case UniformBuffer:
		{
			assert(m_uiCurrentBoundUBO == this->m_uiBufferIndex);
			break;
		}
	}

	glBufferStorage(target, size, data, flags);

	if (m_uiBufferSize)
	{
		m_uiApproximated_VRAM_Bytes = m_uiApproximated_VRAM_Bytes - (m_uiBufferSize - size);
	}
	else
		m_uiApproximated_VRAM_Bytes += size;

	m_uiBufferSize = size;
	m_uiStorageFlags = flags;
	m_bIsImmutable = true;
	m_uiBufferUsage = StaticDraw;
}
void GL_BufferHandler::BufferSubData(const vbo_targets& target, const GLintptr& offset, const GLsizeiptr& size, const void* data)
{
	bool canchange = m_bIsImmutable ? (m_uiStorageFlags & GL_DYNAMIC_STORAGE_BIT) : true;
	assert(canchange);

	assert(offset + size <= m_uiBufferSize);

	switch (target)
	{
		case ArrayBuffer:
		{
			assert(m_uiCurrentBoundArray == this->m_uiBufferIndex);
			break;
		}
		case ElementArrayBuffer:
		{
			assert(m_uiCurrentBoundElementArray == this->m_uiBufferIndex);
			break;
		}
		case UniformBuffer:
		{
			assert(m_uiCurrentBoundUBO == this->m_uiBufferIndex);
			break;
		}
	}

	glBufferSubData(target, offset, size, data);

}

GLuint GL_BufferHandler::GetIndex()
{
	return this->m_uiBufferIndex;
}
GLsizeiptr GL_BufferHandler::GetBufferSize()
{
	return this->m_uiBufferSize;
}

void GL_BufferHandler::ResetBufferBinding(const vbo_targets& target)
{
	switch (target)
	{
		case ArrayBuffer:
		{
			if (!m_uiCurrentBoundArray)
				return;
			else
				m_uiCurrentBoundArray = 0;
			break;
		}
		case ElementArrayBuffer:
		{
			if (!m_uiCurrentBoundElementArray)
				return;
			else
				m_uiCurrentBoundElementArray = 0;
			break;
		}
		case UniformBuffer:
		{
			if (!m_uiCurrentBoundUBO)
				return;
			else
				m_uiCurrentBoundUBO = 0;
			break;
		}

	}

	glBindBuffer(target, 0);
}

const GLuint GL_BufferHandler::GetCurrentBind(const vbo_targets& target)
{
	switch (target)
	{
		case ArrayBuffer:
		{
			GLint binding;
			glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &binding);
			assert(m_uiCurrentBoundArray == binding);
			return m_uiCurrentBoundArray;
		}
		case ElementArrayBuffer:
		{
			GLint binding;
			glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &binding);
			assert(m_uiCurrentBoundElementArray == binding);
			return m_uiCurrentBoundElementArray;
		}
		case UniformBuffer:
		{
			GLint binding;
			glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &binding);
			assert(m_uiCurrentBoundUBO == binding);
			return m_uiCurrentBoundUBO;
		}
	}
}
const GLuint GL_BufferHandler::GetNumBuffers()
{
	return m_uiNumBuffers;
}

const GLuint GL_BufferHandler::GetTotalMemorySize()
{
	return m_uiApproximated_VRAM_Bytes;
}

#endif