////////////////////////////////////////////////////////////////
//
//		native implementation of gengfuncs's SPR system
//
////////////////////////////////////////////////////////////////

#include "hud.h"
#include "cl_util.h"
#include "studio_util.h"
#include "r_studioint.h"
#include "triangleapi.h"

#include "client_state.h"

#include "bsprenderer.h"
#include "goldsrc_spriterenderer.h"


extern engine_studio_api_t IEngineStudio;

struct spritelist_s
{
	model_t* pSprite;
	char* pName;
	int frameCount;
};

spritelist_s* gSpriteList;

#define MAX_HUD_SPRITES 256

HSPRITE_GOLDSRC ghCrosshair;

int gSpriteCount = 0;
msprite_t* gpSprite;

bool giScissorTest = false;
int scissor_x, scissor_y, scissor_width, scissor_height;

void Draw_SpriteFrameHoles(mspriteframe_t* pFrame, int x, int y, const Rect* prcSubRect);
void Draw_SpriteFrameAdditive(mspriteframe_t* pFrame, int x, int y, const Rect* prcSubRect);
void Draw_SpriteFrame(mspriteframe_t* pFrame, int ix, int iy, const Rect* prcSubrect);

void SPR_Init()
{
	if (!gSpriteList)
	{
		ghCrosshair = 0;
		gSpriteCount = MAX_HUD_SPRITES;
		gpSprite = nullptr;
		gSpriteList = (spritelist_s*)malloc(sizeof(spritelist_s) * MAX_HUD_SPRITES);
		memset(gSpriteList, 0, sizeof(spritelist_s) * MAX_HUD_SPRITES);
	}
}
void SPR_ShutDown()
{
	//whatevs
}

HSPRITE_GOLDSRC SPR_Load(const char* szPicName)
{
	if (!szPicName)
		return 0;

	if (!gSpriteList || gSpriteCount <= 0)
		return 0;

	int i = 0;

	for (i = 0; i < MAX_HUD_SPRITES; i++)
	{
		if (gSpriteCount <= i)
			gEngfuncs.Con_Printf("cannot allocate more than %d HUD sprites\n", MAX_HUD_SPRITES);

		spritelist_s* entry = &gSpriteList[i];
		if (!entry->pSprite)
		{
			entry->pName = (char*)malloc(strlen(szPicName) + 1);
			strcpy(entry->pName, szPicName);
		}

		if (!strcmp(entry->pName, szPicName))
			break;
	}

	//salsa: i hope that not setting mipmaps wont mess everything up
	// 
	//gSpriteMipMap = false;
	gSpriteList[i].pSprite = IEngineStudio.Mod_ForName(szPicName, false);
	//gSpriteMipMap = true;

	if (!gSpriteList[i].pSprite)
		return 0;


	g_LegacySpriteRenderer.GetSpriteFrames(gSpriteList[i].pSprite, gSpriteList[i].frameCount);
	return i + 1;
}
int SPR_Frames(HSPRITE_GOLDSRC hPic)
{
	int enIndex = hPic - 1;
	if (enIndex >= 0 && enIndex < gSpriteCount)
	{
		spritelist_s* entry = &gSpriteList[enIndex];
		if (entry)
			return entry->frameCount;
	}
	return 0;
}
int SPR_Height(HSPRITE_GOLDSRC hPic, int frame)
{
	int enIndex = hPic - 1;
	if (enIndex < 0 || enIndex >= gSpriteCount)
		return 0;

	spritelist_s* entry = &gSpriteList[enIndex];
	if (!entry)
		return 0;

	mspriteframe_t* spriteframe = g_LegacySpriteRenderer.GetSpriteFrame(entry->pSprite, frame, 0);
	if (!spriteframe)
		return 0;

	return spriteframe->height;
}
int SPR_Width(HSPRITE_GOLDSRC hPic, int frame)
{
	int enIndex = hPic - 1;
	if (enIndex < 0 || enIndex >= gSpriteCount)
		return 0;

	spritelist_s* entry = &gSpriteList[enIndex];
	if (!entry)
		return 0;

	mspriteframe_t* spriteframe = g_LegacySpriteRenderer.GetSpriteFrame(entry->pSprite, frame, 0);
	if (!spriteframe)
		return 0;

	return spriteframe->width;
}
void SPR_Set(HSPRITE_GOLDSRC hPic, int r, int g, int b)
{
	int enIndex = hPic - 1;
	if (enIndex < 0 || enIndex >= gSpriteCount)
		return;

	spritelist_s* entry = &gSpriteList[enIndex];
	if (!entry)
		return;

	gpSprite = (msprite_t*)entry->pSprite->cache.data;
	if (!gpSprite)
		return;

	glColor4f(r / 255.f, g / 255.f, b / 255.f, 1.0);
}
void SPR_Draw(int frame, int x, int y, const Rect* prc)
{
	if (!gpSprite || ScreenWidth <= x || ScreenHeight <= y)
		return;

	//lazy
	model_t dummy;
	dummy.cache.data = gpSprite;
	dummy.type = mod_sprite;

	mspriteframe_t* spriteframe = g_LegacySpriteRenderer.GetSpriteFrame(&dummy, frame, 0);
	if (!spriteframe)
	{
		gEngfuncs.Con_Printf("Client.dll SPR_Draw error:  invalid frame\n");
		return;
	}

	//Draw_SpriteFrame(spriteframe, x, y, prc);
}
void SPR_DrawHoles(int frame, int x, int y, const Rect* prc)
{
	if (!gpSprite || ScreenWidth <= x || ScreenHeight <= y)
		return;

	// lazy
	model_t dummy;
	dummy.cache.data = gpSprite;
	dummy.type = mod_sprite;

	mspriteframe_t* spriteframe = g_LegacySpriteRenderer.GetSpriteFrame(&dummy, frame, 0);
	if (spriteframe)
		Draw_SpriteFrameHoles(spriteframe, x, y, prc);
	else
		gEngfuncs.Con_DPrintf("Client.dll SPR_DrawHoles error:  invalid frame\n");
}

void SPR_DrawAdditive(int frame, int x, int y, const Rect* prc)
{
	if (!gpSprite || ScreenWidth <= x || ScreenHeight <= y)
		return;

	// lazy
	model_t dummy;
	dummy.cache.data = gpSprite;
	dummy.type = mod_sprite;

	mspriteframe_t* spriteframe = g_LegacySpriteRenderer.GetSpriteFrame(&dummy, frame, 0);
	if (spriteframe)
		Draw_SpriteFrameAdditive(spriteframe, x, y, prc);
	else
		gEngfuncs.Con_DPrintf("Client.dll SPR_DrawAdditive error:  invalid frame\n");
}
void SPR_EnableScissor(int x, int y, int width, int height)
{
	scissor_x = x;
	scissor_y = y;
	scissor_width = width;
	scissor_height = height;
	giScissorTest = true;
}
void SPR_DisableScissor(void)
{
	scissor_x = scissor_y = 0;
	scissor_width = scissor_height = 0;
	giScissorTest = 0;
}

model_t* GetSpritePointer(HSPRITE_GOLDSRC hSprite)
{
	int enIndex = hSprite - 1;
	if (enIndex >= 0 && enIndex < gSpriteCount)
	{
		spritelist_s* entry = &gSpriteList[enIndex];
		if (entry)
			return entry->pSprite;
	}
	return nullptr;
}


void Draw_SpriteFrame(mspriteframe_t* pFrame, int ix, int iy, const Rect* prcSubrect)
{
	float fBottom, fTop, fRight, fLeft;
	float width = pFrame->width;
	float height = pFrame->height;
	float x = ix + 0.5;
	float y = iy + 0.5;

	//VGUI2_ResetCurrentTexture();
	// 
	//  -- hacky way to call the function above, otherwise vgui text and sprite stuff gets messed up :(
	gEngfuncs.pTriAPI->Begin(0);
	glEnd();
	if (giScissorTest)
	{
		glScissor(scissor_x, scissor_y, scissor_width, scissor_height);
		glEnable(GL_SCISSOR_TEST);
	}
	bool wrong_dimensions = false;

	if (!prcSubrect)
		wrong_dimensions = true;
	else
		wrong_dimensions = prcSubrect->left >= prcSubrect->right || prcSubrect->top >= prcSubrect->bottom;

	if (wrong_dimensions)
	{
		fBottom = fRight = 1.0;
		fTop = fLeft = 0.0;
	}
	else
	{
		width = prcSubrect->right - prcSubrect->left;
		height = prcSubrect->bottom - prcSubrect->top;

		fLeft = (prcSubrect->left + 0.5) * 1.0 / pFrame->width;
		fRight = (prcSubrect->right - 0.5) * 1.0 / pFrame->width;

		fTop = (prcSubrect->top + 0.5) * 1.0 / pFrame->height;
		fBottom = (prcSubrect->bottom - 0.5) * 1.0 / pFrame->height;
	}

	glDepthMask(GL_FALSE);

	glBindTexture(GL_TEXTURE_2D, pFrame->gl_texturenum);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	const float texcoords[4][2]{
		{fLeft, fTop},
		{fRight, fTop},
		{fRight, fBottom},
		{fLeft, fBottom}
	};

	const float verts[4][2]{
		{x, y},
		{width + x, y},
		{width + x, height + y},
		{x, height + y},
	};

	glBegin(GL_QUADS);
		glTexCoord2f(texcoords[0][0], texcoords[0][1]);
		glVertex2f(verts[0][0], verts[0][1]);

		glTexCoord2f(texcoords[1][0], texcoords[1][1]);
		glVertex2f(verts[1][0], verts[1][1]);

		glTexCoord2f(texcoords[2][0], texcoords[2][1]);
		glVertex2f(verts[2][0], verts[2][1]);

		glTexCoord2f(texcoords[3][0], texcoords[3][1]);
		glVertex2f(verts[3][0], verts[3][1]);
	glEnd();

	glDepthMask(GL_TRUE);
	glDisable(GL_SCISSOR_TEST);
}

void Draw_SpriteFrameAdditive(mspriteframe_t* pFrame, int ix, int iy, const Rect* prcSubrect)
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	Draw_SpriteFrame(pFrame, ix, iy, prcSubrect);

	glDisable(GL_BLEND);
}

void Draw_SpriteFrameHoles(mspriteframe_t* pFrame, int x, int y, const Rect* prcSubRect)
{
	glEnable(GL_ALPHA_TEST);
	if (gEngfuncs.pfnGetCvarFloat("gl_spriteblend") != 0.0)
	{
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
	}

	Draw_SpriteFrame(pFrame, x, y, prcSubRect);

	glDisable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);
}

Rect gCrosshairRc;
int gCrosshairR;
int gCrosshairG;
int gCrosshairB;



void SetCrosshair(HSPRITE_GOLDSRC hspr, Rect rc, int r, int g, int b)
{
	ghCrosshair = hspr;
	gCrosshairRc = rc;
	gCrosshairR = r;
	gCrosshairG = g;
	gCrosshairB = b;
}

void DrawCrosshair()
{
	Vector angles;
	Vector crosshairangles;
	Vector forward;
	Vector point, screen(0, 0, 0);

	const auto& global_refdef = gBSPRenderer.m_RefParams;
	angles = global_refdef.viewangles + (engine_cl->crosshairangle * Vector(1.0, 1.0, 1.0) );
	AngleVectors(angles, &forward, nullptr, nullptr);
	point = global_refdef.vieworg + forward * 50;

	screen = gBSPRenderer.TriWorldToScreen(point);

	float flHeight = abs(gCrosshairRc.bottom - gCrosshairRc.top);
	float flWidth = abs(gCrosshairRc.right - gCrosshairRc.left);

	float center[2] = {
		{ScreenWidth / 2.f },
		{ScreenHeight / 2.f},
	};

	
	if ( (screen.x < 0.5001 && screen.y < 0.5001) && (screen.x > 0.4999 && screen.y > 0.4999) )
	{
		screen = Vector(0.5, 0.5, 0.5); //avoid crosshair jittering
	}

	screen.x *= ScreenWidth;
	screen.y *= ScreenHeight;

	center[0] -= center[0] - screen.x;
	center[1] -= center[1] - screen.y;
	center[0] -= flWidth * 0.5f;
	center[1] -= flHeight * 0.5f;

	if (!ghCrosshair)
		return;

	int enIndex = ghCrosshair - 1;

	if (enIndex >= 0 && enIndex < gSpriteCount)
	{
		spritelist_s* entry = &gSpriteList[enIndex];
		if (entry)
		{
			gpSprite = (msprite_t*)entry->pSprite->cache.data;
			if (gpSprite)
			{
				glColor4f(gCrosshairR / 255.f, gCrosshairG / 255.f, gCrosshairB / 255.f, 1.0);
			}
		}
	}

	if (!gpSprite || ScreenWidth <= flWidth || ScreenHeight <= flHeight)
		return;

	model_t dummy;
	dummy.type = mod_sprite;
	dummy.cache.data = gpSprite;

	mspriteframe_t* spriteframe = g_LegacySpriteRenderer.GetSpriteFrame(&dummy, 0, 0);
	if (spriteframe)
		Draw_SpriteFrameHoles(spriteframe, center[0], center[1], &gCrosshairRc);
	else
		gEngfuncs.Con_DPrintf("Client.dll SPR_DrawHoles error:  invalid frame\n");
}

void FillRGBA(float x, float y, float w, float h, int r, int g, int b, int a)
{
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glColor4f(r / 255.f, g / 255.f, b / 255.f, a / 255.f);

	glBegin(GL_QUADS);
		glVertex2f(x, y);
		glVertex2f(x + w, y);
		glVertex2f(x + w, y + h);
		glVertex2f(x, y + h);
	glEnd();

	glColor3f(1.0, 1.0, 1.0);

	glEnable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
}