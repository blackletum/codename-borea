#include "PlatformHeaders.h"
#include "Platform.h"
#include "hud.h"
#include "cl_util.h"

#include "renderer/rendererdefs.h"

#include "GL_TextureHandler.h"

std::vector<std::unique_ptr<GL_TextureHandler>> GL_TextureHandler::m_vTextureList;

GLuint GL_TextureHandler::m_uiApproximatedBytesTextureAllocated = 0;

int BytesPerPixel(GLenum internalFormat)
{
	switch (internalFormat)
	{
		case GL_R8:			case GL_R8_SNORM:		case GL_R8I:		case GL_R8UI:		return 1;
		case GL_RG8:		case GL_RG8_SNORM:		case GL_RG8I:		case GL_RG8UI:		return 2;
		case GL_RGB8:		case GL_RGB8_SNORM:		case GL_RGB8I:		case GL_RGB8UI:		return 3;
		case GL_RGBA8:		case GL_RGBA8_SNORM:	case GL_RGBA8I:		case GL_RGBA8UI:	return 4;

		case GL_R16:		case GL_R16_SNORM:		case GL_R16F:							return 2;
		case GL_RG16:		case GL_RG16_SNORM:		case GL_RG16F:							return 4;	
		case GL_RGB16:		case GL_RGB16_SNORM:	case GL_RGB16F:							return 6;
		case GL_RGBA16:		case GL_RGBA16_SNORM:	case GL_RGBA16F:						return 8;

		case GL_R32F:		case GL_R32I:			case GL_R32UI:							return 4;
		case GL_RG32F:		case GL_RG32I:			case GL_RG32UI:							return 8;
		case GL_RGB32F:		case GL_RGB32I:			case GL_RGB32UI:						return 12;
		case GL_RGBA32F:	case GL_RGBA32I:		case GL_RGBA32UI:						return 16;

		default: return 0;
	}
}

GL_TextureHandler::GL_TextureHandler(gl_texturecreationinfo_t* texinfo)
{
	m_TexInfo = *texinfo;
	m_iWidth = texinfo->width;
	m_iHeight = texinfo->height;
	int bytes = BytesPerPixel(texinfo->internalformat);


	float color[4];
	color[0] = color[1] = color[2] = color[3] = 0;

	glGenTextures(1, &m_uiTextureHandle);
	if (m_TexInfo.texturetype == _2DTexture)
	{
		glBindTexture(GL_TEXTURE_2D, m_uiTextureHandle);
		glTexImage2D(GL_TEXTURE_2D, 0, m_TexInfo.internalformat, m_iWidth, m_iHeight, 0, m_TexInfo.format, m_TexInfo.colortype, nullptr);
		m_uiApproximatedBytesTextureAllocated += (m_iWidth * m_iHeight) * bytes;
		m_uiTextureByteSize += (m_iWidth * m_iHeight) * bytes;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, color);

	}
	else if (m_TexInfo.texturetype == _2DTexture_Storage)
	{
		glBindTexture(GL_TEXTURE_2D, m_uiTextureHandle);
		glTexStorage2D(GL_TEXTURE_2D, 1, m_TexInfo.internalformat, m_iWidth, m_iHeight);
		m_uiApproximatedBytesTextureAllocated += (m_iWidth * m_iHeight) * bytes;
		m_uiTextureByteSize += (m_iWidth * m_iHeight) * bytes;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, color);
	}
	else if (m_TexInfo.texturetype == _Rectangle)
	{
		glBindTexture(GL_TEXTURE_RECTANGLE, m_uiTextureHandle);
		glTexImage2D(GL_TEXTURE_RECTANGLE, 0, m_TexInfo.internalformat, m_iWidth, m_iHeight, 0, m_TexInfo.format, m_TexInfo.colortype, nullptr);
		m_uiApproximatedBytesTextureAllocated += (m_iWidth * m_iHeight) * bytes;
		m_uiTextureByteSize += (m_iWidth * m_iHeight) * bytes;
		glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameterfv(GL_TEXTURE_RECTANGLE, GL_TEXTURE_BORDER_COLOR, color);

	}
	else if (m_TexInfo.texturetype == _2DTextureArray)
	{
		glBindTexture(GL_TEXTURE_2D_ARRAY, m_uiTextureHandle);
		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, m_TexInfo.internalformat, m_iWidth, m_iHeight, m_TexInfo.arraydepth, 0, m_TexInfo.format, m_TexInfo.colortype, nullptr);
		m_uiApproximatedBytesTextureAllocated += ((m_iWidth * m_iHeight) * bytes) * m_TexInfo.arraydepth;
		m_uiTextureByteSize += ((m_iWidth * m_iHeight) * bytes) * m_TexInfo.arraydepth;
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, color);
	}
	else if (m_TexInfo.texturetype == _CubeMap)
	{
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_uiTextureHandle);
		for (GLuint i = 0; i < 6; ++i)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, m_TexInfo.internalformat,
				m_iWidth, m_iHeight, 0, m_TexInfo.format, m_TexInfo.colortype, nullptr);

			m_uiApproximatedBytesTextureAllocated += (m_iWidth * m_iHeight) * bytes;
			m_uiTextureByteSize += (m_iWidth * m_iHeight) * bytes;
		}
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameterfv(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BORDER_COLOR, color);

	}
	else if (m_TexInfo.texturetype == _CubeMap_Storage)
	{
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_uiTextureHandle);

		for (GLuint i = 0; i < 6; ++i)
		{
			m_uiApproximatedBytesTextureAllocated += (m_iWidth * m_iHeight) * bytes;
			m_uiTextureByteSize += (m_iWidth * m_iHeight) * bytes;
		}

		glTexStorage2D(GL_TEXTURE_CUBE_MAP, 1, m_TexInfo.internalformat,
			m_iWidth, m_iHeight);

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameterfv(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BORDER_COLOR, color);
	}
	else
		assert(0);

	std::unique_ptr<GL_TextureHandler> ptr(this);
	m_vTextureList.push_back(std::move(ptr));
}

GL_TextureHandler::~GL_TextureHandler()
{
	//when exiting game normally, some textures arent cleared and so it doesnt have much of a 
	//smooth exit, of course it'll only throw an error when on debug config
	assert(glIsTexture(m_uiTextureHandle) && m_uiTextureHandle > 0);

	glDeleteTextures(1, &m_uiTextureHandle);
	m_uiApproximatedBytesTextureAllocated -= m_uiTextureByteSize;
}

void GL_TextureHandler::UploadPixelData(const void* pixeldata)
{
	if (m_TexInfo.texturetype == _2DTexture)
	{
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_iWidth, m_iHeight, m_TexInfo.format, m_TexInfo.colortype, pixeldata);
	}
	else if (m_TexInfo.texturetype == _Rectangle)
	{
		glTexSubImage2D(GL_TEXTURE_RECTANGLE, 0, 0, 0, m_iWidth, m_iHeight, m_TexInfo.format, m_TexInfo.colortype, pixeldata);
	}
}