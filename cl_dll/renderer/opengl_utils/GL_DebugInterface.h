#pragma once

#include "PlatformHeaders.h"
#include "Platform.h"
#include "hud.h"
#include "cl_util.h"

#include "renderer/rendererdefs.h"


class GL_DebugInterface
{
public:
	void Initialize();

private:
	// function to be given to opengl interface
	static void APIENTRY DebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
};

extern GL_DebugInterface g_IGLDebug;