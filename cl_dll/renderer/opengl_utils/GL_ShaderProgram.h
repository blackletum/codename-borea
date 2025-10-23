#pragma once

#include "PlatformHeaders.h"
#include "Platform.h"
#include "hud.h"
#include "cl_util.h"
#include <unordered_map>

#include "renderer/rendererdefs.h"


#define VERTPOS_LOC 0
#define NORMAL_LOC 1
#define TEXCOORD_LOC 2
#define LM_TEXCOORD_LOC 3
#define COLOR_LOC 4
#define STUDIOMDL_BONEID_LOC 5

class GL_ShaderProgram
{
public:
	GL_ShaderProgram(const char* vertexSrc, const char* fragmentSrc);
	~GL_ShaderProgram();

#ifdef _DEBUG

	void Bind();

	void Uniform1i(GLint loc, const int& value){
		assert(m_pCurrentProgram == this);
		glUniform1i(loc, value);
	};
	void Uniform1iv(GLint loc, const int& count, const int* value){
		assert(m_pCurrentProgram == this);
		glUniform1iv(loc, count, value);
	};

	void Uniform2i(GLint loc, const int& value1, const int& value2){
		assert(m_pCurrentProgram == this);
		glUniform2i(loc, value1, value2);
	};
	void Uniform2iv(GLint loc, const int& count, const int* value){
		assert(m_pCurrentProgram == this);
		glUniform2iv(loc, count, value);
	};

	void Uniform3i(GLint loc, const int& value1, const int& value2, const int& value3){
		assert(m_pCurrentProgram == this);
		glUniform3i(loc, value1, value2, value3);
	};
	void Uniform3iv(GLint loc, const int& count, const int* value){
		assert(m_pCurrentProgram == this);
		glUniform3iv(loc, count, value);
	};

	void Uniform4i(GLint loc, const int& value1, const int& value2, const float& value3, const float& value4){
		assert(m_pCurrentProgram == this);
		glUniform4i(loc, value1, value2, value3, value4);
	};
	void Uniform4iv(GLint loc, const int& count, const int* value){
		assert(m_pCurrentProgram == this);
		glUniform4iv(loc, count, value);
	};




	void Uniform1f(GLint loc, const float& value){
		assert(m_pCurrentProgram == this);
		glUniform1f(loc, value);
	};
	void Uniform1fv(GLint loc, const int& count, const float* value){
		assert(m_pCurrentProgram == this);
		glUniform1fv(loc, count, value);
	};

	void Uniform2f(GLint loc, const float& value1, const float& value2){
		assert(m_pCurrentProgram == this);
		glUniform2f(loc, value1, value2);
	};
	void Uniform2fv(GLint loc, const int& count, const float* value){
		assert(m_pCurrentProgram == this);
		glUniform2fv(loc, count, value);
	};

	void Uniform3f(GLint loc, const float& value1, const float& value2, const float& value3){
		assert(m_pCurrentProgram == this);
		glUniform3f(loc, value1, value2, value3);
	};
	void Uniform3fv(GLint loc, const int& count, const float* value){
		assert(m_pCurrentProgram == this);
		glUniform3fv(loc, count, value);
	};

	void Uniform4f(GLint loc, const float& value1, const float& value2, const float& value3, const float& value4){
		assert(m_pCurrentProgram == this);
		glUniform4f(loc, value1, value2, value3, value4);
	};
	void Uniform4fv(GLint loc, const int& count, const float* value){
		assert(m_pCurrentProgram == this);
		glUniform4fv(loc, count, value);
	};






	void UniformMatrix4fv(GLint loc, const int& count, const bool& transpose, const float* value){
		assert(m_pCurrentProgram == this);
		glUniformMatrix4fv(loc, count, transpose, value);
	};

	void UniformMatrix3x4fv(GLint loc, const int& count, const bool& transpose, const float* value){
		assert(m_pCurrentProgram == this);
		glUniformMatrix3x4fv(loc, count, transpose, value);
	};

	void UniformMatrix4x3fv(GLint loc, const int& count, const bool& transpose, const float* value){
		assert(m_pCurrentProgram == this);
		glUniformMatrix4x3fv(loc, count, transpose, value);
	};

	GLint GetUniformLoc(const char* name);
	GLint GetUBOIndex(const char* name);
	GLint GetAttribLoc(const char* name);

	static void ResetShaderBind();
	static GL_ShaderProgram* GetCurrentProgram() { return m_pCurrentProgram; };

#else

	__forceinline void Bind() { glUseProgram(m_uiProgramIndex); };

	__forceinline void Uniform1i(GLint loc, const int& value){
		glUniform1i(loc, value);
	};
	__forceinline void Uniform1iv(GLint loc, const int& count, const int* value){
		glUniform1iv(loc, count, value);
	};

	__forceinline void Uniform2i(GLint loc, const int& value1, const int& value2){
		glUniform2i(loc, value1, value2);
	};
	__forceinline void Uniform2iv(GLint loc, const int& count, const int* value){
		glUniform2iv(loc, count, value);
	};

	__forceinline void Uniform3i(GLint loc, const int& value1, const int& value2, const int& value3){
		glUniform3i(loc, value1, value2, value3);
	};
	__forceinline void Uniform3iv(GLint loc, const int& count, const int* value){
		glUniform3iv(loc, count, value);
	};

	__forceinline void Uniform4i(GLint loc, const int& value1, const int& value2, const float& value3, const float& value4){
		glUniform4i(loc, value1, value2, value3, value4);
	};
	__forceinline void Uniform4iv(GLint loc, const int& count, const int* value){
		glUniform4iv(loc, count, value);
	};




	__forceinline void Uniform1f(GLint loc, const float& value){
		glUniform1f(loc, value);
	};
	__forceinline void Uniform1fv(GLint loc, const int& count, const float* value){
		glUniform1fv(loc, count, value);
	};

	__forceinline void Uniform2f(GLint loc, const float& value1, const float& value2){
		glUniform2f(loc, value1, value2);
	};
	__forceinline void Uniform2fv(GLint loc, const int& count, const float* value){
		glUniform2fv(loc, count, value);
	};

	__forceinline void Uniform3f(GLint loc, const float& value1, const float& value2, const float& value3){
		glUniform3f(loc, value1, value2, value3);
	};
	__forceinline void Uniform3fv(GLint loc, const int& count, const float* value){
		glUniform3fv(loc, count, value);
	};

	__forceinline void Uniform4f(GLint loc, const float& value1, const float& value2, const float& value3, const float& value4){
		glUniform4f(loc, value1, value2, value3, value4);
	};
	__forceinline void Uniform4fv(GLint loc, const int& count, const float* value){
		glUniform4fv(loc, count, value);
	};






	__forceinline void UniformMatrix4fv(GLint loc, const int& count, const bool& transpose, const float* value){
		glUniformMatrix4fv(loc, count, transpose, value);
	};

	__forceinline void UniformMatrix3x4fv(GLint loc, const int& count, const bool& transpose, const float* value){
		glUniformMatrix3x4fv(loc, count, transpose, value);
	};

	__forceinline void UniformMatrix4x3fv(GLint loc, const int& count, const bool& transpose, const float* value){
		glUniformMatrix4x3fv(loc, count, transpose, value);
	};

	__forceinline GLint GetUniformLoc(const char* name) { return m_vUniforms[name]; };
	__forceinline GLint GetUBOIndex(const char* name) { return m_vUBOs[name]; };
	__forceinline GLint GetAttribLoc(const char* name) { return m_vAttribs[name]; };

	static __forceinline void ResetShaderBind() { glUseProgram(0); };
	static __forceinline GL_ShaderProgram* GetCurrentProgram() { return 0; };

#endif

	enum ShaderAttribs
	{
		VertexPos = 0,
		Normal,
		TexCoord,
		LightMap_TexCoord, //bsp exclusive
		Detail_TexCoord, //bsp exclusive
		Color,
		StudioMDL_BoneID, //studiomdl exclusive
	};

	static __forceinline int GetDriverUBOAlignment() { return m_Driver_UBOAlignment; }


private:

	static const GLuint CompileShader(const char* source, const GLuint type);
	void ShaderPreLink();
	void ShaderPostLink();

	void GenUniformList();
	void GenUBOList();
	void GenAttribList();

	std::unordered_map<std::string, GLint> m_vUniforms;
	std::unordered_map<std::string, GLint> m_vUBOs;
	std::unordered_map<std::string, GLint> m_vAttribs;

	GLuint m_uiProgramIndex = 0;

	static GL_ShaderProgram* m_pCurrentProgram;
	static GLuint m_uiCurFreeUBOindex;
	static GLint m_Driver_UBOAlignment;
};

//
// i have no use for this class for now but it'll remain here	
//
class GL_ARBShaderProgram
{
public:
	GL_ARBShaderProgram(const char* vertexSrc, const char* fragmentSrc);
	~GL_ARBShaderProgram();

	void BindVertProgram();
	void BindFragProgram();

	static void DisableVertProgram();
	static void DisableFragProgram();

private:
	void GenVertProgram(const char* vertexSrc);
	void GenFragProgram(const char* fragmentSrc);

	GLuint m_uiFragmentProgramIndex = 0;
	GLuint m_uiVertexProgramIndex = 0;
};