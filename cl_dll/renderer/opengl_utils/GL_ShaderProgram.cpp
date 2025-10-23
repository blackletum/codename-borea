
#include <SDL2/SDL_messagebox.h>
#include "PlatformHeaders.h"
#include "Platform.h"
#include "hud.h"
#include "cl_util.h"
#include <unordered_map>
#include <stdexcept>
#include <iostream>
#include <sstream>

#include "renderer/rendererdefs.h"

#include "GL_ShaderProgram.h"

extern SDL_Window* hlWindow;

const std::string glsl330_engine_defines_vertex = R"(

	//version 330 compatibility
	//#define GLSL_330

	#version 130
	#define GLSL_130


	#define M_PI 3.14159265358979323846 // matches value in gcc v2 math.h

	#define M_PI2 6.28318530718

	#extension GL_ARB_shading_language_420pack : enable // supported by +/- 76.86% of gpus
	#extension GL_ARB_explicit_uniform_location : enable // supported by +/- 75.11% of gpus
	#extension GL_ARB_uniform_buffer_object : enable // supported by +/- 78.22% of gpus
	#extension GL_ARB_enhanced_layouts : enable // supported by +/- 67.05% of gpus (yikes)


	in vec3 aPosition;
	in vec3 aNormal;
	in vec2 aTexCoord;
	in vec2 aTexCoordLM;
	in vec2 aTexCoordDetail;
	in vec4 aColor;
	in int aBoneID;


	//math utilities

	float sqr(float x){ return x * x; }

	#if defined(GLSL_130)	//add used functions that glsl 130 doesnt have	

	float det(mat2 matrix) {
		return matrix[0].x * matrix[1].y - matrix[0].y * matrix[1].x;
	}

	mat3 inverse(mat3 matrix) {
	    vec3 row0 = matrix[0];
	    vec3 row1 = matrix[1];
	    vec3 row2 = matrix[2];
	
	    vec3 minors0 = vec3(
	        det(mat2(row1.y, row1.z, row2.y, row2.z)),
	        det(mat2(row1.z, row1.x, row2.z, row2.x)),
	        det(mat2(row1.x, row1.y, row2.x, row2.y))
	    );
	    vec3 minors1 = vec3(
	        det(mat2(row2.y, row2.z, row0.y, row0.z)),
	        det(mat2(row2.z, row2.x, row0.z, row0.x)),
	        det(mat2(row2.x, row2.y, row0.x, row0.y))
	    );
	    vec3 minors2 = vec3(
	        det(mat2(row0.y, row0.z, row1.y, row1.z)),
	        det(mat2(row0.z, row0.x, row1.z, row1.x)),
	        det(mat2(row0.x, row0.y, row1.x, row1.y))
	    );
	
	    mat3 adj = transpose(mat3(minors0, minors1, minors2));
	
	    return (1.0 / dot(row0, minors0)) * adj;
	}
		
	#endif


)";

const std::string glsl330_engine_defines_fragment = R"(

	//version 330 compatibility
	//#define GLSL_330

	#version 130
	#define GLSL_130

	#extension GL_ARB_shading_language_420pack : enable // supported by +/- 76.86% of gpus
	#extension GL_ARB_explicit_uniform_location : enable // supported by +/- 75.11% of gpus
	#extension GL_ARB_uniform_buffer_object : enable // supported by +/- 78.22% of gpus
	#extension GL_ARB_enhanced_layouts : enable // supported by +/- 67.05% of gpus (yikes)

	//math utilities

	float sqr(float x){ return x * x; }

	#if defined(GLSL_130)	//add used functions that glsl 130 doesnt have	

	float det(mat2 matrix) {
		return matrix[0].x * matrix[1].y - matrix[0].y * matrix[1].x;
	}

	mat3 inverse(mat3 matrix) {
	    vec3 row0 = matrix[0];
	    vec3 row1 = matrix[1];
	    vec3 row2 = matrix[2];
	
	    vec3 minors0 = vec3(
	        det(mat2(row1.y, row1.z, row2.y, row2.z)),
	        det(mat2(row1.z, row1.x, row2.z, row2.x)),
	        det(mat2(row1.x, row1.y, row2.x, row2.y))
	    );
	    vec3 minors1 = vec3(
	        det(mat2(row2.y, row2.z, row0.y, row0.z)),
	        det(mat2(row2.z, row2.x, row0.z, row0.x)),
	        det(mat2(row2.x, row2.y, row0.x, row0.y))
	    );
	    vec3 minors2 = vec3(
	        det(mat2(row0.y, row0.z, row1.y, row1.z)),
	        det(mat2(row0.z, row0.x, row1.z, row1.x)),
	        det(mat2(row0.x, row0.y, row1.x, row1.y))
	    );
	
	    mat3 adj = transpose(mat3(minors0, minors1, minors2));
	
	    return (1.0 / dot(row0, minors0)) * adj;
	}
		
	#endif
	
	

)";


GL_ShaderProgram* GL_ShaderProgram::m_pCurrentProgram = nullptr;
GLuint GL_ShaderProgram::m_uiCurFreeUBOindex = 0;
GLint GL_ShaderProgram::m_Driver_UBOAlignment = 0;

GL_ShaderProgram::GL_ShaderProgram(const char* vertexSrc, const char* fragmentSrc)
{
	glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &m_Driver_UBOAlignment);
	//m_Driver_UBOAlignment = 256;

	std::string vertexcode = glsl330_engine_defines_vertex;
	std::string fragmentcode = glsl330_engine_defines_fragment;
	vertexcode += vertexSrc;
	fragmentcode += fragmentSrc;
	GLuint vertexShader = CompileShader(vertexcode.c_str(), GL_VERTEX_SHADER);
	GLuint fragmentShader = CompileShader(fragmentcode.c_str(), GL_FRAGMENT_SHADER);

	GLuint program = m_uiProgramIndex = glCreateProgram();
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);

	ShaderPreLink();
	glLinkProgram(program);

	// Check link status
	GLint success;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success)
	{
		char log[512];
		glGetProgramInfoLog(program, 512, nullptr, log);

		std::string errormsg = "A OpenGL Shader could not be linked!!!\n error message: \n \n";
		errormsg += log;
		errormsg += '\n';
		errormsg += "Reach out to the developer of this renderer and relate this issue if you wish to.\n";
		errormsg += "The program will now close.";
		
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "FATAL OPENGL ERROR", errormsg.c_str(), nullptr);
		exit(-1);
	}

	ShaderPostLink();

	// Cleanup
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	GenUniformList();
	GenUBOList();
	GenAttribList();

}

GL_ShaderProgram::~GL_ShaderProgram()
{
	glDeleteProgram(m_uiProgramIndex);
}


const GLuint GL_ShaderProgram::CompileShader(const char* source, const GLuint type)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);

	// Check compile status
	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		char log[512];
		glGetShaderInfoLog(shader, 512, nullptr, log);

		std::string errormsg = "A OpenGL Shader could not be compiled!!!\n error message: \n \n";
		errormsg += log;
		errormsg += '\n';
		errormsg += "Reach out to the developer of this renderer and relate this issue if you wish to.\n";
		errormsg += "The program will now close.";

		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "FATAL OPENGL ERROR", errormsg.c_str(), hlWindow);
		exit(-1);
	}

	return shader;
}

void GL_ShaderProgram::ShaderPreLink()
{
	//layout(location = x) is only supported in glsl 330 with extensions, versions < 330 dont support it even with extensions :(
	//our goal is to use glsl version 130, hopefully that will reduce issues with intel gpus
	glBindAttribLocation(this->m_uiProgramIndex, GL_ShaderProgram::VertexPos, "aPosition");
	glBindAttribLocation(this->m_uiProgramIndex, GL_ShaderProgram::Normal, "aNormal");
	glBindAttribLocation(this->m_uiProgramIndex, GL_ShaderProgram::TexCoord, "aTexCoord");
	glBindAttribLocation(this->m_uiProgramIndex, GL_ShaderProgram::LightMap_TexCoord, "aTexCoordLM");
	glBindAttribLocation(this->m_uiProgramIndex, GL_ShaderProgram::Detail_TexCoord, "aTexCoordDetail");
	glBindAttribLocation(this->m_uiProgramIndex, GL_ShaderProgram::Color, "aColor");
	glBindAttribLocation(this->m_uiProgramIndex, GL_ShaderProgram::StudioMDL_BoneID, "aBoneID");
}
void GL_ShaderProgram::ShaderPostLink()
{
	//nothing to do here yet
}

extern char* UTIL_VarArgs_client(const char* format, ...);

void GL_ShaderProgram::GenUniformList()
{
	GLint uniformcount;
	glGetProgramiv(m_uiProgramIndex, GL_ACTIVE_UNIFORMS, &uniformcount);

	for (GLint i = 0; i < uniformcount; ++i)
	{
		char name[256];
		std::string stdname;
		GLsizei length;
		GLint size;
		GLenum type;

		glGetActiveUniform(m_uiProgramIndex, i, sizeof(name), &length, &size, &type, name);
		stdname = name;

		GLint location = glGetUniformLocation(m_uiProgramIndex, name);

		m_vUniforms[name] = location;

		if (stdname.size() > 3 && stdname.substr(stdname.size() - 3) == "[0]")
		{
			int arrayloc = 1;
			int j = 1;
			while (1)
			{
				std::string arrayname = name;
				arrayname.erase(arrayname.size() - 3);
				arrayname += UTIL_VarArgs_client("[%d]", j);

				location = glGetUniformLocation(m_uiProgramIndex, arrayname.c_str());
				if (location < 0)
					break;

				m_vUniforms[arrayname] = location;
				j++;

			}
		}

	}
}

void GL_ShaderProgram::GenUBOList()
{
	GLint ubocount;
	glGetProgramiv(m_uiProgramIndex, GL_ACTIVE_UNIFORM_BLOCKS, &ubocount);

	for (GLint i = 0; i < ubocount; ++i)
	{
		char name[256];
		GLsizei buffersize;
		GLint length;
		GLint size;
		GLenum type;

		glGetActiveUniformBlockName(m_uiProgramIndex, i, sizeof(name), &length, name);

		GLint location = glGetUniformBlockIndex(m_uiProgramIndex, name);
		glUniformBlockBinding(m_uiProgramIndex, location, m_uiCurFreeUBOindex);

		m_vUBOs[name] = m_uiCurFreeUBOindex++;

	}
}

void GL_ShaderProgram::GenAttribList()
{
	GLint attribcount;
	glGetProgramiv(m_uiProgramIndex, GL_ACTIVE_ATTRIBUTES, &attribcount);

	for (GLint i = 0; i < attribcount; ++i)
	{
		char name[256];
		std::string stdname;
		GLsizei length;
		GLint size;
		GLenum type;

		glGetActiveAttrib(m_uiProgramIndex, i, sizeof(name), &length, &size, &type, name);
		stdname = name;

		GLint location = glGetAttribLocation(m_uiProgramIndex, name);

		m_vAttribs[name] = location;

		if (stdname.size() > 3 && stdname.substr(stdname.size() - 3) == "[0]")
		{
			int arrayloc = 1;
			int j = 1;
			while (1)
			{
				std::string arrayname = name;
				arrayname.erase(arrayname.size() - 3);
				arrayname += UTIL_VarArgs_client("[%d]", j);

				location = glGetUniformLocation(m_uiProgramIndex, arrayname.c_str());
				if (location < 0)
					break;

				m_vUniforms[arrayname] = location;
				j++;
			}
		}
	}
}

#ifdef _DEBUG

GLint GL_ShaderProgram::GetUniformLoc(const char* name)
{
	GLuint returnloc = -1;
	try
	{
		returnloc = m_vUniforms.at(name);
	}
	catch (const std::out_of_range& e)
	{
		const char* error = e.what();
		bool COULDNT_FIND_SHADER_UNIFORM = false;

		//assert(COULDNT_FIND_SHADER_UNIFORM);
	}
	
	return returnloc;

}

GLint GL_ShaderProgram::GetUBOIndex(const char* name)
{
	GLuint returnloc = -1;
	try
	{
		returnloc = m_vUBOs.at(name);
	}
	catch (const std::out_of_range& e)
	{
		const char* error = e.what();
		bool COULDNT_FIND_SHADER_UBO = false;

		//assert(COULDNT_FIND_SHADER_UBO);
	}

	return returnloc;
}

GLint GL_ShaderProgram::GetAttribLoc(const char* name)
{
	GLuint returnloc = -1;
	try
	{
		returnloc = m_vAttribs.at(name);
	}
	catch (const std::out_of_range& e)
	{
		const char* error = e.what();
		bool COULDNT_FIND_SHADER_ATTRIBUTE_THIS_FUNCTION_SHOULDNT_EVEN_BE_USED = false;

		//assert(COULDNT_FIND_SHADER_ATTRIBUTE_THIS_FUNCTION_SHOULDNT_EVEN_BE_USED);
	}

	return returnloc;
}

void GL_ShaderProgram::Bind()
{
	if (m_pCurrentProgram == this)
		return;

	glUseProgram(m_uiProgramIndex);
	m_pCurrentProgram = this;
}

void GL_ShaderProgram::ResetShaderBind()
{
	if (!m_pCurrentProgram)
		return;

	m_pCurrentProgram = 0;
	glUseProgram(0);
}

#endif



//
//				GL_ARBShaderProgram
//



GL_ARBShaderProgram::GL_ARBShaderProgram(const char* vertexSrc, const char* fragmentSrc)
{
	GenVertProgram(vertexSrc);
	GenFragProgram(fragmentSrc);
}


void GL_ARBShaderProgram::BindVertProgram()
{
	glEnable(GL_VERTEX_PROGRAM_ARB);
	glBindProgramARB(GL_VERTEX_PROGRAM_ARB, m_uiVertexProgramIndex);
}
void GL_ARBShaderProgram::BindFragProgram()
{
	glEnable(GL_FRAGMENT_PROGRAM_ARB);
	glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, m_uiFragmentProgramIndex);
}

void GL_ARBShaderProgram::DisableVertProgram()
{
	glDisable(GL_VERTEX_PROGRAM_ARB);
	glBindProgramARB(GL_VERTEX_PROGRAM_ARB, 0);
}
void GL_ARBShaderProgram::DisableFragProgram()
{
	glDisable(GL_FRAGMENT_PROGRAM_ARB);
	glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);
}


void GL_ARBShaderProgram::GenVertProgram(const char* vertexSrc)
{
	GLint iErrorPos = 0, iIsNative = 0;

	glEnable(GL_VERTEX_PROGRAM_ARB);
	glGenProgramsARB(1, &m_uiVertexProgramIndex);
	glBindProgramARB(GL_VERTEX_PROGRAM_ARB, m_uiVertexProgramIndex);

	glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, strlen(vertexSrc) - 1, vertexSrc);
	glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &iErrorPos);
	glGetProgramivARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &iIsNative);
	glDisable(GL_VERTEX_PROGRAM_ARB);

	assert(iErrorPos != -1 || iIsNative);
}
void GL_ARBShaderProgram::GenFragProgram(const char* fragmentSrc)
{
	GLint iErrorPos = 0, iIsNative = 0;

	glEnable(GL_FRAGMENT_PROGRAM_ARB);
	glGenProgramsARB(1, &m_uiVertexProgramIndex);
	glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, m_uiVertexProgramIndex);

	glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, strlen(fragmentSrc) - 1, fragmentSrc);
	glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &iErrorPos);
	glGetProgramivARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &iIsNative);
	glDisable(GL_FRAGMENT_PROGRAM_ARB);

	assert(iErrorPos != -1 || iIsNative);
}
