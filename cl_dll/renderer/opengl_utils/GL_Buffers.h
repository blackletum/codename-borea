#pragma once

#include "PlatformHeaders.h"
#include "Platform.h"
#include "hud.h"
#include "cl_util.h"

#include "renderer/rendererdefs.h"

class GL_BufferHandler
{
public:
	GL_BufferHandler();
	~GL_BufferHandler();

	enum vbo_targets
	{
		ArrayBuffer = GL_ARRAY_BUFFER,
		ElementArrayBuffer = GL_ELEMENT_ARRAY_BUFFER,
		UniformBuffer = GL_UNIFORM_BUFFER
	};

	//	STREAM: The data will be modified once and used at most a few times.
	//	STATIC: The data will be modified once and used many times.
	//	DYNAMIC:The data will be modified repeatedly and used many times.
	//	
	//	
	//	_DRAW: The data is modified by the application, and used as the source for GL drawing and image specification commands.
	//	_READ: The data is modified by reading data from the GL, and used to return that data when queried by the application.
	//	_COPY: The data is modified by reading data from the GL, and used as the source for GL drawing and image specification commands.

	enum vbo_usage
	{
		StreamDraw = GL_STREAM_DRAW,
		StreamRead = GL_STREAM_READ,
		StreamCopy = GL_STREAM_COPY,
		DynamicDraw = GL_DYNAMIC_DRAW,
		DynamicRead = GL_DYNAMIC_READ,
		DynamicCopy = GL_DYNAMIC_COPY,
		StaticDraw = GL_STATIC_DRAW,
		StaticRead = GL_STATIC_READ,
		StaticCopy = GL_STATIC_COPY,
	};

	enum vbo_flags
	{
		MapFlushExplicit_bit = GL_MAP_FLUSH_EXPLICIT_BIT,
		MapRead_bit = GL_MAP_READ_BIT,
		MapWrite_bit = GL_MAP_WRITE_BIT,
		MapPersistent_bit = GL_MAP_PERSISTENT_BIT,
		MapCoherent_bit = GL_MAP_COHERENT_BIT,
		DynamicStorage_bit = GL_DYNAMIC_STORAGE_BIT,
	};
	#ifdef _DEBUG
	void Bind(const vbo_targets& target);
	void BindBase(const vbo_targets& target, const GLuint index);
	void BindRange(const vbo_targets& target, const GLuint index, const GLsizei offset, const GLsizei size);

	void BufferData(const vbo_targets& target, const GLsizeiptr& size, const void* data, const vbo_usage& usage);
	void BufferStorage(const vbo_targets& target, const GLsizeiptr& size, const void* data, const GLbitfield& flags);
	void BufferSubData(const vbo_targets& target, const GLintptr& offset, const GLsizeiptr& size, const void* data);

	void* MapBuffer(const vbo_targets& target, const GLenum& access);
	void UnmapBuffer(const vbo_targets& target);

	GLuint GetIndex();
	GLsizeiptr GetBufferSize();

	static void ResetBufferBinding(const vbo_targets& target);
	static const GLuint GetCurrentBind(const vbo_targets& target);
	static const GLuint GetNumBuffers();
	static const GLuint GetTotalMemorySize();
	#else

	__forceinline void Bind(const vbo_targets& target) { glBindBuffer(target, this->m_uiBufferIndex); };
	__forceinline void BindBase(const vbo_targets& target, const GLuint index) { glBindBufferBase(target, index, this->m_uiBufferIndex); };
	__forceinline void BindRange(const vbo_targets& target, const GLuint index, const GLsizei offset, const GLsizei size) { glBindBufferRange(target, index, this->m_uiBufferIndex, offset, size); }

	__forceinline void BufferData(const vbo_targets& target, const GLsizeiptr& size, const void* data, const vbo_usage& usage)
	{
		glBufferData(target, size, data, usage);
		m_uiBufferSize = size;
	};
	__forceinline void BufferStorage(const vbo_targets& target, const GLsizeiptr& size, const void* data, const GLbitfield& flags)
	{
		glBufferStorage(target, size, data, flags);
		m_uiBufferSize = size;
		m_uiStorageFlags = flags;
	};
	__forceinline void BufferSubData(const vbo_targets& target, const GLintptr& offset, const GLsizeiptr& size, const void* data) { glBufferSubData(target, offset, size, data); };

	__forceinline void* MapBuffer(const vbo_targets& target, const GLenum& access) { return glMapBuffer(target, access); }
	__forceinline void UnmapBuffer(const vbo_targets& target) { glUnmapBuffer(target); }

	__forceinline GLuint GetIndex() { return this->m_uiBufferIndex; };
	__forceinline GLsizeiptr GetBufferSize() { return this->m_uiBufferSize; };

	static __forceinline void ResetBufferBinding(const vbo_targets& target) { glBindBuffer(target, 0); };
	static __forceinline const GLuint GetCurrentBind(const vbo_targets& target) { return 0; };
	static __forceinline const GLuint GetNumBuffers() { return m_uiNumBuffers; };
	static __forceinline const GLuint GetTotalMemorySize()  { return m_uiApproximated_VRAM_Bytes; };

	#endif

private:
	GLuint m_uiBufferIndex = 0;
	GLuint m_uiBufferSize = 0;
	GLuint m_uiStorageFlags = 0;
	vbo_usage m_uiBufferUsage;

	void* m_pBufferMapPointer = nullptr;

	bool m_bIsImmutable = false;

	static GLuint m_uiNumBuffers;
	static GLuint m_uiApproximated_VRAM_Bytes;

	static GLuint m_uiCurrentBoundArray;
	static GLuint m_uiCurrentBoundElementArray;
	static GLuint m_uiCurrentBoundUBO;

};