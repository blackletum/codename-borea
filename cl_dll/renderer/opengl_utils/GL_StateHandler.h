#pragma once
#include "PlatformHeaders.h"
#include "Platform.h"
#include "hud.h"
#include "cl_util.h"

class GL_StateHandler
{
public:
	#ifdef _DEBUG
	void ResetStates();

	void SetBlend(const bool benable);
	void SetBlendFunc(const int src, const int dst);

	void SetAlphaTest(const bool benable);
	void SetAlphaFunc(const int func, const float threshold);

	void SetCullFace(const bool benable);

	void SetDepthTest(const bool benable);
	void SetDepthFunc(const int func);

	void SetDepthWrite(const bool benable);

	void SetPolygonOffsetFill(const bool benable);
	#else

	__forceinline void ResetStates() noexcept {};

	__forceinline void SetBlend(const bool benable) noexcept {
		if (benable)
			glEnable(GL_BLEND);
		else
			glDisable(GL_BLEND);
	};
	__forceinline void SetBlendFunc(const int src, const int dst) noexcept {
		glBlendFunc(src, dst);
	};

	__forceinline void SetAlphaTest(const bool benable) noexcept {
		if (benable)
			glEnable(GL_ALPHA_TEST);
		else
			glDisable(GL_ALPHA_TEST);
	};
	__forceinline void SetAlphaFunc(const int func, const float threshold) noexcept {
		glAlphaFunc(func, threshold);
	};


	__forceinline void SetCullFace(const bool benable) noexcept {
		if (benable)
			glEnable(GL_CULL_FACE);
		else
			glDisable(GL_CULL_FACE);
	};

	__forceinline void SetDepthTest(const bool benable) noexcept {
		if (benable)
			glEnable(GL_DEPTH_TEST);
		else
			glDisable(GL_DEPTH_TEST);
	};
	__forceinline void SetDepthFunc(const int func) noexcept {
		glDepthFunc(func);
	};

	__forceinline void SetDepthWrite(const bool benable) noexcept
	{
		glDepthMask(benable);
	};

	__forceinline void SetPolygonOffsetFill(const bool benable) noexcept {
		if (benable)
			glEnable(GL_POLYGON_OFFSET_FILL);
		else
			glDisable(GL_POLYGON_OFFSET_FILL);
	};

	#endif

private:
	byte m_bBlend = 255;
	byte m_bAlphaTest = 255;
	byte m_bTexture2D = 255;
	byte m_bCullFace = 255;
	byte m_bDepthTest = 255;
	byte m_bDepthMask = 255;
	byte m_bPolygonOffsetFill = 255;

	int m_iBlendSrc = 0, m_iBlendDst = 0;
	int m_iDepthFunc = 0;
	int m_iAlphaFunc = 0, m_iAlphaThreshold = 0;
};