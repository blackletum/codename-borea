#include "PlatformHeaders.h"
#include "Platform.h"
#include "hud.h"
#include "cl_util.h"

#include "renderer/rendererdefs.h"

#include "GL_StateHandler.h"

GL_StateHandler g_GlobalGLState;

#ifdef _DEBUG

void GL_StateHandler::ResetStates()
{
	m_bBlend = 255;
	m_bAlphaTest = 255;
	m_bTexture2D = 255;
	m_bCullFace = 255;
	m_bDepthTest = 255;
	m_bPolygonOffsetFill = 255;

	m_iBlendSrc = 255;
	m_iBlendDst = 255;
	m_iDepthFunc = 255;
	m_iAlphaFunc = 255;
	m_iAlphaThreshold = 255;
}

void GL_StateHandler::SetBlend(const bool benable)
{
	if (m_bBlend == benable)
	{
		gEngfuncs.Con_DPrintf("[OPENGL] performance warning: useless call to SetBlend !\n");
	}

	if (benable)
	{
		m_bBlend = 1;
		glEnable(GL_BLEND);
	}
	else if (!benable)
	{
		m_bBlend = 0;
		glDisable(GL_BLEND);
	}
}

void GL_StateHandler::SetBlendFunc(const int src, const int dst)
{
	if (m_iBlendSrc == src && m_iBlendDst == dst)
	{
		gEngfuncs.Con_DPrintf("[OPENGL] performance warning: useless call to SetBlendFunc !\n");
	}

	m_iBlendSrc = src;
	m_iBlendDst = dst;

	glBlendFunc(src, dst);
}

void GL_StateHandler::SetAlphaTest(const bool benable)
{
	if (m_bAlphaTest == benable)
	{
		gEngfuncs.Con_DPrintf("[OPENGL] performance warning: useless call to SetAlphaTest !\n");
	}

	if (benable)
	{
		m_bAlphaTest = 1;
		glEnable(GL_ALPHA_TEST);
	}
	else if (!benable)
	{
		m_bAlphaTest = 0;
		glDisable(GL_ALPHA_TEST);
	}
}
void GL_StateHandler::SetAlphaFunc(const int func, const float threshold)
{
	if (m_iAlphaFunc == func && m_iAlphaThreshold == threshold)
	{
		gEngfuncs.Con_DPrintf("[OPENGL] performance warning: useless call to SetAlphaFunc !\n");
	}

	m_iAlphaFunc = func;
	m_iAlphaThreshold = threshold;

	glAlphaFunc(func, threshold);
}

void GL_StateHandler::SetCullFace(const bool benable)
{
	if (m_bCullFace == benable)
	{
		gEngfuncs.Con_DPrintf("[OPENGL] performance warning: useless call to SetCullFace !\n");
	}

	if (benable)
	{
		m_bCullFace = 1;
		glEnable(GL_CULL_FACE);
	}
	else if (!benable)
	{
		m_bCullFace = 0;
		glDisable(GL_CULL_FACE);
	}
}

void GL_StateHandler::SetDepthTest(const bool benable)
{
	if (m_bDepthTest == benable)
	{
		gEngfuncs.Con_DPrintf("[OPENGL] performance warning: useless call to SetDepthTest !\n");
	}

	if (benable)
	{
		m_bDepthTest = 1;
		glEnable(GL_DEPTH_TEST);
	}
	else if (!benable)
	{
		m_bDepthTest = 0;
		glDisable(GL_DEPTH_TEST);
	}
}

void GL_StateHandler::SetDepthFunc(const int func)
{
	if (m_iDepthFunc == func)
	{
		gEngfuncs.Con_DPrintf("[OPENGL] performance warning: useless call to SetDepthFunc !\n");
	}

	m_iDepthFunc = func;

	glDepthFunc(func);
}

void GL_StateHandler::SetDepthWrite(const bool benable)
{
	if (m_bDepthMask == benable)
	{
		gEngfuncs.Con_DPrintf("[OPENGL] performance warning: useless call to SetDepthWrite !\n");
	}

	if (benable)
		glDepthMask(GL_TRUE);
	else
		glDepthMask(GL_FALSE);
};

void GL_StateHandler::SetPolygonOffsetFill(const bool benable)
{
	if (m_bPolygonOffsetFill == benable)
	{
		gEngfuncs.Con_DPrintf("[OPENGL] performance warning: useless call to SetPolygonOffsetFill !\n");
	}

	if (benable)
	{
		m_bPolygonOffsetFill = 1;
		glEnable(GL_POLYGON_OFFSET_FILL);
	}
	else if (!benable)
	{
		m_bPolygonOffsetFill = 0;
		glDisable(GL_POLYGON_OFFSET_FILL);
	}
}

#endif