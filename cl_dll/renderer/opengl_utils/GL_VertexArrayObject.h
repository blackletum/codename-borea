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
	__forceinline void BindVAO() { glBindVertexArray(m_uiVAOIndex); m_pCurrentBoundVAO = this; }

	static __forceinline void ResetVAOBinding() { glBindVertexArray(0); m_pCurrentBoundVAO = nullptr; };

	#endif


	static __forceinline GL_VertexArrayObject* GetBoundVAO() { return m_pCurrentBoundVAO; };

	GLuint m_uiVAOIndex = 0;

private:

	static GL_VertexArrayObject* m_pCurrentBoundVAO;
};