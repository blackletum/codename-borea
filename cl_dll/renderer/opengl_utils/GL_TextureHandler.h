#pragma once

#include "PlatformHeaders.h"
#include "Platform.h"
#include "hud.h"
#include "cl_util.h"

#include "renderer/rendererdefs.h"

class GL_TextureHandler
{
public:
	enum GL_TextureType
	{
		_2DTexture = 0,
		_2DTexture_Storage, //gltexstorage2d
		_Rectangle,
		_2DTextureArray,
		_CubeMap,
		_CubeMap_Storage //gltexstorage2d too
	};

	struct gl_texturecreationinfo_t
	{
		void SetInfo(std::string szName, GL_TextureType iTexType, int iInternalFormat, int iWidth, int iHeight, int iArrayDepth, int iFormat, int iColorType)
		{
			name = szName;
			texturetype = iTexType;
			internalformat = iInternalFormat;
			width = iWidth;
			height = iHeight;
			arraydepth = iArrayDepth;
			format = iFormat;
			colortype = iColorType;
		}

		std::string name;
		GL_TextureType texturetype;
		int internalformat;
		int width, height;
		int arraydepth;
		int format;
		int colortype;
	};

	GL_TextureHandler(gl_texturecreationinfo_t* texinfo);
	~GL_TextureHandler();

	void UploadPixelData(const void* pixeldata);

	__forceinline GL_TextureType GetTextureType() const noexcept { return m_TexInfo.texturetype; };
	__forceinline GLuint GetTextureID() const noexcept { return m_uiTextureHandle; };

protected:
	GLuint m_uiTextureHandle;
	
	gl_texturecreationinfo_t m_TexInfo;


	GLuint m_iWidth = 0, m_iHeight = 0;
	GLuint m_uiTextureByteSize = 0;

	static std::vector<std::unique_ptr<GL_TextureHandler>> m_vTextureList;
	static GLuint m_uiApproximatedBytesTextureAllocated;
};