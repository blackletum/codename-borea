#pragma once

#include "PlatformHeaders.h"
#include "Platform.h"
#include "hud.h"
#include "cl_util.h"

#include "renderer/rendererdefs.h"

class GL_VertexArrayObject // should this be shortened to VAO ? dunno, maybe not, want my code to be clear
{
public:
	GL_VertexArrayObject();
	~GL_VertexArrayObject();

	#ifdef _DEBUG
	void BindVAO();

	static void ResetVAOBinding();

	#else
	__forceinline void BindVAO() {glBindVertexArray(m_uiVAOIndex);}

	static __forceinline void ResetVAOBinding() { glBindVertexArray(0); };

	#endif

	GLuint m_uiVAOIndex = 0;

private:

	static GLuint m_uiCurrentBoundVAO;
};