#include "PlatformHeaders.h"
#include "Platform.h"
#include "hud.h"
#include "cl_util.h"

#include "renderer/rendererdefs.h"

#include "GL_Buffers.h"
#include "GL_DebugInterface.h"

//
// GL_ARB_debug_output
//
// requires that sdl creates the opengl context with the SDL_GL_CONTEXT_DEBUG_FLAG flag.

GL_DebugInterface g_IGLDebug;

void GL_DebugInterface::Initialize()
{
	#ifdef _DEBUG
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

	glDebugMessageCallback(DebugMessage, nullptr);

	glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW_ARB, 0, NULL, true);
	#endif
}

void GL_DebugInterface::DebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
	std::string error_msg;
	switch (type)
	{
	case GL_DEBUG_TYPE_ERROR:
		error_msg += "[GL_DEBUG_ERROR] ";
		break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
		error_msg += "[GL_DEBUG_DEPRECATED] ";
		break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
		error_msg += "[GL_DEBUG_UNDEFINED_BEHAVIOUR] ";
		break;
	case GL_DEBUG_TYPE_PORTABILITY:
		error_msg += "[GL_DEBUG_PORTABILITY] ";
		break;
	case GL_DEBUG_TYPE_PERFORMANCE:
		error_msg += "[GL_DEBUG_PERFORMANCE] ";
		break;
	case GL_DEBUG_TYPE_OTHER:
		error_msg += "[GL_DEBUG_OTHER] ";
		break;
	}

	switch (severity)
	{
	case GL_DEBUG_SEVERITY_HIGH:
		error_msg += "(SEVERE) ";
		break;
	case GL_DEBUG_SEVERITY_MEDIUM:
		error_msg += "(KINDA SEVERE) ";
		break;
	case GL_DEBUG_SEVERITY_LOW:
		error_msg += "(WARNING) ";
		break;
	}

	error_msg += message;
	error_msg += '\n';

	gEngfuncs.Con_Printf(error_msg.c_str());
}