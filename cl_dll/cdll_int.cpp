/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
//  cdll_int.c
//
// this implementation handles the linking of the engine to the DLL
//

#include <SDL2/SDL_messagebox.h>

#include "hud.h"
#include "cl_util.h"
#include "netadr.h"
#include "interface.h"
//#include "vgui_schememanager.h"

#include "pm_shared.h"

#include <string.h>
#include "vgui_int.h"

#include "Platform.h"
#include "Exports.h"

#include "tri.h"
#include "triangleapi.h"
#include "vgui_TeamFortressViewport.h"
#include "filesystem_utils.h"

// RENDERERS START
#include "../renderer/rendererdefs.h"
#include "../renderer/particle_engine.h"
#include "../renderer/bsprenderer.h"
#include "../renderer/propmanager.h"
#include "../renderer/textureloader.h"
#include "../renderer/watershader.h"
#include "../renderer/mirrormanager.h"

#include "../renderer/opengl_utils/GL_StateHandler.h"

#include "studio.h"
#include "../renderer/StudioModelRenderer.h"

extern engine_studio_api_t IEngineStudio;
// RENDERERS END

#include "openal/OpenAL_System.h"


model_t dummymodel;
msprite_t dummysprite;
mspriteframe_t dummyspriteframe;

modfuncs_t* g_pModFuncs;

extern int cl_numvisedicts;
extern cl_entity_s* cl_visedicts[512];


cl_enginefunc_t gEngfuncs;
CHud gHUD;
TeamFortressViewport* gViewPort = NULL;

void InitInput();
void EV_HookEvents();
void IN_Commands();

/*
================================
HUD_GetHullBounds

  Engine calls this to enumerate player collision hulls, for prediction.  Return 0 if the hullnumber doesn't exist.
================================
*/
int DLLEXPORT HUD_GetHullBounds(int hullnumber, float* mins, float* maxs)
{
	//	RecClGetHullBounds(hullnumber, mins, maxs);

	int iret = 0;

	switch (hullnumber)
	{
	case 0:				// Normal player
		memcpy(mins, &VEC_HULL_MIN, sizeof(VEC_HULL_MIN));
		memcpy(maxs, &VEC_HULL_MAX, sizeof(VEC_HULL_MAX));
		iret = 1;
		break;
	case 1:				// Crouched player
		memcpy(mins, &VEC_DUCK_HULL_MIN, sizeof(VEC_DUCK_HULL_MIN));
		memcpy(maxs, &VEC_DUCK_HULL_MAX, sizeof(VEC_DUCK_HULL_MAX));
		iret = 1;
		break;
	case 2:				// Point based hull
		memcpy(mins, &g_vecZero, sizeof(g_vecZero));
		memcpy(maxs, &g_vecZero, sizeof(g_vecZero));
		iret = 1;
		break;
	}

	return iret;
}

/*
================================
HUD_ConnectionlessPacket

 Return 1 if the packet is valid.  Set response_buffer_size if you want to send a response packet.  Incoming, it holds the max
  size of the response_buffer, so you must zero it out if you choose not to respond.
================================
*/
int DLLEXPORT HUD_ConnectionlessPacket(const struct netadr_s* net_from, const char* args, char* response_buffer, int* response_buffer_size)
{
	//	RecClConnectionlessPacket(net_from, args, response_buffer, response_buffer_size);

	// Parse stuff from args
	int max_buffer_size = *response_buffer_size;

	// Zero it out since we aren't going to respond.
	// If we wanted to response, we'd write data into response_buffer
	*response_buffer_size = 0;

	// Since we don't listen for anything here, just respond that it's a bogus message
	// If we didn't reject the message, we'd return 1 for success instead.
	return 0;
}

void DLLEXPORT HUD_PlayerMoveInit(struct playermove_s* ppmove)
{
	//	RecClClientMoveInit(ppmove);

	PM_Init(ppmove);
}

char DLLEXPORT HUD_PlayerMoveTexture(char* name)
{
	//	RecClClientTextureType(name);

	return PM_FindTextureTypeID(name);
}

void DLLEXPORT HUD_PlayerMove(struct playermove_s* ppmove, int server)
{
	//	RecClClientMove(ppmove, server);

	PM_Move(ppmove, server);
}

static bool CL_InitClient()
{
	EV_HookEvents();

	if (!FileSystem_LoadFileSystem())
	{
		return false;
	}

	if (UTIL_IsValveGameDirectory())
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error",
			"This mod has detected that it is being run from a Valve game directory which is not supported\n"
			"Run this mod from its intended location\n\nThe game will now shut down", nullptr);
		return false;
	}

	// get tracker interface, if any
	return true;
}

extern void Hook_gEngfuncs_Functions();

SDL_Window* hlWindow = nullptr;
SDL_Window** windowPptr = nullptr;
SDL_GLContext newGlContext;


void LoadWindowIcon()
{
	int width, height, bbps;
	std::string filedirectory = gEngfuncs.pfnGetGameDirectory();
	filedirectory += "/game.tga";

	byte* buffer = gTextureLoader.LoadTGAFileRaw(filedirectory.c_str(), width, height, bbps, false);
	if (!buffer)
		return;

	SDL_Surface* pSurface = SDL_CreateRGBSurfaceFrom(buffer, width, height, bbps * 8, bbps * width, 0xFF, 0xFF << 8, 0xFF << 16, 0xFF << 24);

	if (pSurface)
	{
		SDL_SetWindowIcon(hlWindow, pSurface);
		SDL_FreeSurface(pSurface);
	}
	delete[] buffer;
}

size_t GetModuleSize(HMODULE hModule)
{
	if (!hModule)
		return 0;

	// Base pointer
	auto base = reinterpret_cast<BYTE*>(hModule);

	// DOS header
	auto dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
	if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
		return 0;

	// NT headers
	auto ntHeader = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dosHeader->e_lfanew);
	if (ntHeader->Signature != IMAGE_NT_SIGNATURE)
		return 0;

	// Size of the image in memory
	return ntHeader->OptionalHeader.SizeOfImage;
}


int DLLEXPORT Initialize(cl_enginefunc_t* pEnginefuncs, int iVersion)
{
	gEngfuncs = *pEnginefuncs;

	//	RecClInitialize(pEnginefuncs, iVersion);


	// !!!!!!!!! credits to Meetem for the code below !!!!!!!

	auto oldHlWindow = hlWindow == nullptr ? SDL_GetWindowFromID(1) : hlWindow;
	hlWindow = oldHlWindow;

	if (oldHlWindow != nullptr)
	{
		auto flags = SDL_GetWindowFlags(oldHlWindow);
		auto brightness = SDL_GetWindowBrightness(oldHlWindow);
		auto a = SDL_GetWindowPixelFormat(oldHlWindow);
		auto displayIndex = SDL_GetWindowDisplayIndex(oldHlWindow);

		SDL_DisplayMode dm;
		auto displayMode = SDL_GetWindowDisplayMode(oldHlWindow, &dm);
		auto surf = SDL_GetWindowSurface(oldHlWindow);

		int w, h;
		int x, y;
		SDL_GetWindowSize(oldHlWindow, &w, &h);
		SDL_GetWindowPosition(oldHlWindow, &x, &y);

		auto title = SDL_GetWindowTitle(oldHlWindow);
		char titlecp[256];
		strcpy(titlecp, title);

		//SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
		//SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 4);

#ifdef _DEBUG
//GL_ARB_debug_output
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif

		// No MSAA
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);

		// -Stencil
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);

		// -8bit Alpha
		SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);

		// MH_Initialize();
		// auto oldCtx = wglGetCurrentContext();
		// savedGlContext = oldCtx;

		auto fullScreenFlags = flags & SDL_WINDOW_FULLSCREEN_DESKTOP;
		flags |= SDL_WINDOW_HIDDEN; // only start hidden seems to work properly

		SDL_Window* window = SDL_CreateWindow(titlecp, x, y, w, h, flags);
		auto engineBase = GetModuleHandleA("hw.dll");
		auto size = GetModuleSize(engineBase);
		if (windowPptr == nullptr)
		{
			for (int i = 0; i < size; i += 4)
			{
				auto* ptr = (SDL_Window**)((uint8_t*)engineBase + i);
				if (*ptr == oldHlWindow)
				{
					windowPptr = ptr;
					// replace window.
					*ptr = window;
					break;
					//Debug.Log("Found!");
				}
			}
		}
		else
		{
			*windowPptr = window;
		}

		if (windowPptr == nullptr)
			assert(0);

		/*
		auto glMakeCurrentTarget = (pfnGLMakeCurrent)&wglMakeCurrent;
		auto hookstatus = MH_CreateHook(glMakeCurrentTarget, WGL_MakeCurrent_Hooked, (LPVOID*)&wglMakeCurrentOrg);
		if (hookstatus != MH_OK)
		{
			Debug.Fatal("Can't install hook on wglMakeCurrent, %p", hookstatus);
		}

		hookstatus = MH_EnableHook(glMakeCurrentTarget);
		if (hookstatus != MH_OK)
		{
			Debug.Fatal("Can't enable hook on wglMakeCurrent, %p", hookstatus);
		}
		*/

		SDL_GLContext glContext = SDL_GL_CreateContext(window);
		newGlContext = glContext;

		SDL_SetWindowDisplayMode(window, &dm);
		SDL_GL_MakeCurrent(window, glContext);

		SDL_DestroyWindow(oldHlWindow);
		SDL_ShowWindow(window);
		SDL_RaiseWindow(window);

		int flags_ = 0;
		SDL_GL_GetAttribute(SDL_GL_CONTEXT_FLAGS, &flags_);
		assert(flags_ & SDL_GL_CONTEXT_DEBUG_FLAG);

		//SDL_AddEventWatch(CloseWindowOnAltF4, nullptr);

		hlWindow = window;
		// wglMakeCurrent()

		//*sdlWindow = window;
	}

	LoadWindowIcon();



	//////////////////////////////////////////////////





	if (iVersion != CLDLL_INTERFACE_VERSION)
		return 0;

	memcpy(&gEngfuncs, pEnginefuncs, sizeof(cl_enginefunc_t));

	Hook_gEngfuncs_Functions();

	const bool result = CL_InitClient();

	if (!result)
	{
		gEngfuncs.Con_DPrintf("Error initializing client\n");
		gEngfuncs.pfnClientCmd("quit\n");
		return 0;
	}

	return 1;
}

bool is_readable_memory(const MEMORY_BASIC_INFORMATION& mbi)
{
	if (mbi.State != MEM_COMMIT)
		return false;
	DWORD prot = mbi.Protect & ~PAGE_GUARD & ~PAGE_NOCACHE;
	if (prot == 0)
		return false;

	return (prot & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) != 0;
}

std::uintptr_t findPatternInModule(uint8_t* hModule, size_t iSize, const char* pattern)
{
	if (!hModule || !pattern)
		return 0;

	BYTE* base = hModule;
	SIZE_T size = iSize;
	SIZE_T patlen = strlen(pattern);

	SIZE_T offset = 0;
	while (offset < size)
	{
		MEMORY_BASIC_INFORMATION mbi;
		if (!VirtualQuery(base + offset, &mbi, sizeof(mbi)))
			break;
		if (is_readable_memory(mbi))
		{
			SIZE_T regionSize = std::min<SIZE_T>(mbi.RegionSize, size - offset);
			BYTE* regionBase = reinterpret_cast<BYTE*>(mbi.BaseAddress);
			for (SIZE_T i = 0; i + patlen <= regionSize; ++i)
			{
				if (memcmp(regionBase + i, pattern, patlen) == 0)
				{
					return reinterpret_cast<std::uintptr_t>(regionBase + i);
				}
			}
		}
		offset += mbi.RegionSize;
	}
	return 0;
}


void FindEngineCLS()
{
	//WARNING: pretty low-level stuff here



	// in goldsrc, cls.userinfo is set like this:
	// value = Steam_GetCommunityName();
	// if (!value)
	// 	value = "unknown";
	//
	// Info_SetValueForKey(cls.userinfo, "name", value, 256);
	// Info_SetValueForKey(cls.userinfo, "topcolor", "0", 256);
	// Info_SetValueForKey(cls.userinfo, "bottomcolor", "0", 256);
	// Info_SetValueForKey(cls.userinfo, "rate", "2500", 256);
	// Info_SetValueForKey(cls.userinfo, "cl_updaterate", "20", 256);
	// Info_SetValueForKey(cls.userinfo, "cl_lw", "1", 256);
	// Info_SetValueForKey(cls.userinfo, "cl_lc", "1", 256);
	// so we need to check for string:
	// \topcolor\0\bottomcolor\0\rate\2500\cl_updaterate\20\cl_lw\1\cl_lc\1
	// and then iterate backwards to check for: \name\
	// that'll give us the address of cl.userinfo ! hopefully !

	// this needs to be done RIGHT HERE RIGHT NOW,

	if (!engine_cls)
	{
		auto engineBase = GetModuleHandleA("hw.dll");
		auto size = GetModuleSize(engineBase);

		const char* find = "\\topcolor\\0\\bottomcolor\\0\\rate\\2500\\cl_updaterate\\20\\cl_lw\\1\\cl_lc\\1";
		const char* name = "\\name\\";
		std::uintptr_t found = findPatternInModule((uint8_t*)engineBase, size, find);
		int i = 1;
		while (true)
		{
			char test[7];
			char* test2 = (char*)((size_t)found - i);
			strncpy_s(test, sizeof(test), test2, sizeof(test) - 1); // yay!

			if (!strcmp(test, name))
			{
				engine_cls = (client_static_s*)((size_t)test2 - offsetof(client_static_s, userinfo));
				break; // FOUND IT BABY !
			}

			i++;
		}
	}
	if (!engine_cls)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "FATAL ERROR", "Could not hook into goldsrc's cls data.\n Please contact the mod's development team.\n The game will now close.", nullptr);
		exit(-1);
	}
}


/*
==========================
	HUD_VidInit

Called when the game initializes
and whenever the vid_mode is changed
so the HUD can reinitialize itself.
==========================
*/

int DLLEXPORT HUD_VidInit()
{
	//	RecClHudVidInit();

	//Reset to default on new map load

	SPR_Init();

	UnpackRGB(giR, giG, giB, RGB_HUD_COLOR);

	gHUD.VidInit();

	VGui_Startup();

	return 1;
}

/*
==========================
	HUD_Init

Called whenever the client connects
to a server.  Reinitializes all
the hud variables.
==========================
*/

void DLLEXPORT HUD_Init()
{
	//	RecClHudInit();

	FindEngineCLS();

	InitInput();

	gHUD.Init();
	Scheme_Init();

	//SALSATOBIAS: ugly but just set them to lowest values on startup, atleast until we have a good default value
	gEngfuncs.Cvar_SetValue("texgamma", 1.0);
	gEngfuncs.Cvar_SetValue("lightgamma", 1.0);
}

/*
==========================
	HUD_Redraw

called every screen frame to
redraw the HUD.
===========================
*/

int DLLEXPORT HUD_Redraw(float time, int intermission)
{
	//	RecClHudRedraw(time, intermission);

	gHUD.Redraw(time, 0 != intermission);

	HUD_PrintSpeeds();

	if (gEngfuncs.pfnGetCvarFloat("crosshair"))
		DrawCrosshair();

	// salsatobias: pretty hacky way to call into goldsrc's GL_Bind() function and change currenttexture variable, 
	// this makes it so the following check will always fail and bind the loading texture:
	//
	// 
	//	if (*pic->data != currenttexture) 
	//	{
	//		pic_palette = (*pic->data >> 16) - 1;
	//		currenttexture = *pic->data;
	//		qglBindTexture(GL_TEXTURE_2D, *pic->data);
	//		if (pic_palette >= 0 && pic_palette != g_currentpalette)
	//		{
	//			g_currentpalette = pic_palette;
	//			qglColorTableEXT(GL_SHARED_TEXTURE_PALETTE_EXT, GL_RGB, GL_DYNAMIC_STORAGE_BIT, GL_RGB, GL_UNSIGNED_BYTE, gGLPalette[pic_palette].colors);
	//		}
	//	}

	if (dummymodel.type != mod_sprite)
	{
		dummymodel.type = mod_sprite;
		dummysprite.numframes = 1;
		dummyspriteframe.gl_texturenum = 0;
		dummysprite.frames[0].type = SPR_SINGLE;
		dummysprite.frames[0].frameptr = &dummyspriteframe;
		dummymodel.cache.data = &dummysprite;
	}

	mspriteframe_t* test = &dummyspriteframe;

	gEngfuncs.pTriAPI->SpriteTexture(&dummymodel, 0);

	glEnable(GL_ALPHA_TEST);

	return 1;
}

extern int restore_numleafs;

/*
==========================
	HUD_UpdateClientData

called every time shared client
dll/engine data gets changed,
and gives the cdll a chance
to modify the data.

returns 1 if anything has been changed, 0 otherwise.
==========================
*/

int DLLEXPORT HUD_UpdateClientData(client_data_t* pcldata, float flTime)
{
	//	RecClHudUpdateClientData(pcldata, flTime);

	IN_Commands();

	if (engine_cl->worldmodel && restore_numleafs) // from what i've understood we need to restore leafs before sv_frame so entities wont get culled to only 1 visframe
		engine_cl->worldmodel->numleafs = restore_numleafs;

	return static_cast<int>(gHUD.UpdateClientData(pcldata, flTime));
}

/*
==========================
	HUD_Reset

Called at start and end of demos to restore to "non"HUD state.
==========================
*/

void DLLEXPORT HUD_Reset()
{
	//	RecClHudReset();

	gHUD.VidInit();
}

/*
==========================
HUD_Frame

Called by engine every frame that client .dll is loaded
==========================
*/

void DLLEXPORT HUD_Frame(double time)
{
	//	RecClHudFrame(time);

	GetClientVoiceMgr()->Frame(time);

	static bool lastpaused = engine_cl->paused;

	if (engine_cl->paused && !lastpaused)
	{
		lastpaused = true;
		gSoundSystem.Pause();
	}
	else if(!engine_cl->paused && lastpaused)
	{
		lastpaused = false;
		gSoundSystem.Resume();
	}

	cl_numvisedicts = 0;
	memset(cl_visedicts, 0, sizeof(cl_visedicts));

	// and now that vis entities have been retrieved from server, we close leaf access to goldsrc :3
	// this fixes goldsrc infecting world model with its dirty r_visframecount when saving. hopefully thats
	// the end of my battle with vis frames.
	if (engine_cl->worldmodel && restore_numleafs)
		engine_cl->worldmodel->numleafs = 0;
}


/*
==========================
HUD_VoiceStatus

Called when a player starts or stops talking.
==========================
*/

void DLLEXPORT HUD_VoiceStatus(int entindex, qboolean bTalking)
{
	////	RecClVoiceStatus(entindex, bTalking);

	GetClientVoiceMgr()->UpdateSpeakerStatus(entindex, 0 != bTalking);
}

/*
==========================
HUD_DirectorMessage

Called when a director event message was received
==========================
*/

void DLLEXPORT HUD_DirectorMessage(int iSize, void* pbuf)
{
	//	RecClDirectorMessage(iSize, pbuf);

	gHUD.m_Spectator.DirectorMessage(iSize, pbuf);
}


std::vector<std::string> models_loaded;
int predicted_numgltextures = 1; // from what i've gathered, numgltextures gets incremented once on LoadTransPic, and m_pfnTextureLoad is not called there.

void GL_TextureLog(char* pszName, int dxWidth, int dyHeight, char* pbData) //(is called)
{
	bool model = stristr(pszName, ".mdl");
	if (model)
	{
		std::string model_n_texture = pszName;
		size_t mdlPos = model_n_texture.find(".mdl");

		mdlPos += 4; // move past ".mdl"
		std::string modelPath = model_n_texture.substr(0, mdlPos);
		std::string textureName = model_n_texture.substr(mdlPos);
		if ((std::find(models_loaded.begin(), models_loaded.end(), modelPath)) == models_loaded.end())
		{
			models_loaded.push_back(modelPath);
			gEngfuncs.Con_Printf("[GOLDSRC] loaded model named %s\n", modelPath.c_str());
		}
		gEngfuncs.Con_Printf("[GOLDSRC] loaded texture %s with width of %d and height of %d\n", textureName.c_str(), dxWidth, dyHeight);
		return;
	}

	gEngfuncs.Con_Printf("[GOLDSRC] loaded texture %s with width of %d and height of %d\n", pszName, dxWidth, dyHeight);

	predicted_numgltextures++;
}

void pfnGetClDstAddrs(cldll_func_dst_t* pcldstAddrs)
{
	gEngfuncs.Con_Printf("%s", __func__);
}

void pfnLoadMod(char* pchModule)
{
	gEngfuncs.Con_Printf("%s", __func__);
}

void pfnCloseMod(void)
{
	gEngfuncs.Con_Printf("%s", __func__);
}
int pfnNCall(int ijump, int cnArg, ...)
{
	gEngfuncs.Con_Printf("%s", __func__);
	return 1;
}


void pfnModuleLoaded(void)
{
	gEngfuncs.Con_Printf("%s", __func__);
}

void pfnProcessOutgoingNet(netchan_s* pchan, sizebuf_t* psizebuf) //(called in the bottom of Netchan_Transmit)
{
	// gEngfuncs.Con_DPrintf("%s\n", __func__);
}
qboolean pfnProcessIncomingNet(netchan_s* pchan, sizebuf_t* psizebuf) //(called in the start of Netchan_Process,  if return is false, then it'll just loop.)
{
	//this is the only way we can get access to engine_cls without memory hunting :(
	if (!engine_cls)
	{
		auto netchanaddress = (size_t)pchan;
		auto netchan_offset = offsetof(client_static_s, netchan);
		engine_cls = (client_static_s*)(netchanaddress - netchan_offset);
	}
	// gEngfuncs.Con_DPrintf("%s\n", __func__);
	return 1;
}

void pfnFrameBegin(void) //(called in Host_Frame before everything)
{
	if (engine_cl->worldmodel && restore_numleafs)
		engine_cl->worldmodel->numleafs = 0;
	// gEngfuncs.Con_Printf("%s", __func__);
}
void pfnFrameRender1(void) //(called in SCR_UpdateScreen before everything)
{
	if (engine_cl->worldmodel && restore_numleafs)
		engine_cl->worldmodel->numleafs = 0;
	// gEngfuncs.Con_Printf("%s", __func__);
}
void pfnFrameRender2(void) //(called in SCR_UpdateScreen in the end before GL_EndRendering)
{
	// gEngfuncs.Con_Printf("%s", __func__);

	if (restore_numleafs && engine_cl->worldmodel)
		engine_cl->worldmodel->numleafs = restore_numleafs;

	g_ImGUIManager.Draw();
	//SDL_GL_SwapWindow(hlWindow); let GL_EndRendering swap the window buffer
}


void pfnSetModSHelpers(modshelpers_t* pmodshelpers)
{
	gEngfuncs.Con_Printf("%s", __func__);
}
void pfnSetModCHelpers(modchelpers_t* pmodchelpers)
{
	gEngfuncs.Con_Printf("%s", __func__);
}
void pfnSetEngData(engdata_t* pengdata)
{
	gEngfuncs.Con_Printf("%s", __func__);
}

void pfnConnectClient(int iPlayer) //(called by SV_ConnectClient)
{
	gEngfuncs.Con_Printf("%s", __func__);
}
void pfnRecordIP(unsigned int pnIP)
{
	gEngfuncs.Con_Printf("%s", __func__);
}
void pfnPlayerStatus(unsigned char* pbData, int cbData)
{
	gEngfuncs.Con_Printf("%s", __func__);
}

void pfnSetEngineVersion(int nVersion)
{
	gEngfuncs.Con_Printf("%s", __func__);
}

void InitModFuncs()
{
	g_pModFuncs->m_pfnLoadMod = pfnLoadMod;
	g_pModFuncs->m_pfnCloseMod = pfnCloseMod;
	g_pModFuncs->m_pfnNCall = pfnNCall;

	// API destination functions
	g_pModFuncs->m_pfnGetClDstAddrs = pfnGetClDstAddrs;
	//g_pModFuncs->m_pfnGetEngDstAddrs = pfnGetEngDstAddrs;

	// Miscellaneous functions
	g_pModFuncs->m_pfnModuleLoaded = pfnModuleLoaded; // Called right after the module is loaded

	// Functions for processing network traffic
	g_pModFuncs->m_pfnProcessOutgoingNet = pfnProcessOutgoingNet; // Every outgoing packet gets run through this //(is called)
	g_pModFuncs->m_pfnProcessIncomingNet = pfnProcessIncomingNet; // Every incoming packet gets run through this //(is called)

	// Functions called every frame
	g_pModFuncs->m_pfnFrameBegin = pfnFrameBegin;	  // Called at the beginning of each frame cycle //(is called)
	g_pModFuncs->m_pfnFrameRender1 = pfnFrameRender1; // Called at the beginning of the render loop //(is called)
	g_pModFuncs->m_pfnFrameRender2 = pfnFrameRender2; // Called at the end of the render loop //(is called)

	// Module helper transfer
	g_pModFuncs->m_pfnSetModSHelpers = pfnSetModSHelpers;
	g_pModFuncs->m_pfnSetModCHelpers = pfnSetModCHelpers;
	g_pModFuncs->m_pfnSetEngData = pfnSetEngData;

	// Miscellaneous game stuff
	g_pModFuncs->m_pfnConnectClient = pfnConnectClient; // Called whenever a new client connects //(is called)
	g_pModFuncs->m_pfnRecordIP = pfnRecordIP;			// Secure master has reported a new IP for us
	g_pModFuncs->m_pfnPlayerStatus = pfnPlayerStatus;	// Called whenever we receive a PlayerStatus packet

	// Recent additions
	g_pModFuncs->m_pfnSetEngineVersion = pfnSetEngineVersion; // 1 = patched engine


	g_pModFuncs->m_pfnTextureLoad = GL_TextureLog;
}

/*
====================
HUD_GetStudioModelInterface

Export this function for the engine to use the studio renderer class to render objects.
====================
*/
int DLLEXPORT HUD_GetStudioModelInterface(int version, struct r_studio_interface_s** ppinterface, struct engine_studio_api_s* pstudio)
{
	//	RecClStudioInterface(version, ppinterface, pstudio);

	if (version != STUDIO_INTERFACE_VERSION)
	{
		gEngfuncs.pfnClientCmd("escape\n");
		MessageBox(NULL, "ERROR: \nClient's studio interface version does not match goldsrc's.\n\nPress OK to quit the game.\n", "ERROR", MB_OK);
		exit(-1);
	}

	// Point the engine to our callbacks
	// NOT! goldsrc should never call into studio renderer
	//*ppinterface = &studio;

	// Copy in engine helper functions
	memcpy(&IEngineStudio, pstudio, sizeof(IEngineStudio));

	if (!IEngineStudio.IsHardware())
	{
		gEngfuncs.pfnClientCmd("escape\n");
		MessageBox(NULL, "VIDEO ERROR: This game does not support Software mode!\nInput the -gl parameter in the game's launch options to use OpenGL.\n\nPress OK to quit the game.\n", "ERROR", MB_OK);
		exit(-1);
	}
	if (IEngineStudio.IsHardware() == 2)
	{
		gEngfuncs.pfnClientCmd("escape\n");
		MessageBox(NULL, "VIDEO ERROR: This game does not support DirectX!\nInput the -gl parameter in the game's launch options to use OpenGL.\n\nPress OK to quit the game.\n", "ERROR", MB_OK);
		exit(-1);
	}

	R_Init();


	// Initialize local variables, etc.
	g_StudioRenderer.Init();

	// Success
	return 1;
}




extern "C" void DLLEXPORT F(void* pv)
{
	cldll_func_t* pcldll_func = (cldll_func_t*)pv;
	g_pModFuncs = reinterpret_cast<modfuncs_s*>(pcldll_func->pInitFunc);

	InitModFuncs();

	cldll_func_t cldll_func =
	{
		Initialize,
		HUD_Init,
		HUD_VidInit,
		HUD_Redraw,
		HUD_UpdateClientData,
		HUD_Reset,
		HUD_PlayerMove,
		HUD_PlayerMoveInit,
		HUD_PlayerMoveTexture,
		IN_ActivateMouse,
		IN_DeactivateMouse,
		IN_MouseEvent,
		IN_ClearStates,
		IN_Accumulate,
		CL_CreateMove,
		CL_IsThirdPerson,
		CL_CameraOffset,
		KB_Find,
		CAM_Think,
		V_CalcRefdef,
		HUD_AddEntity,
		HUD_CreateEntities,
		HUD_DrawNormalTriangles,
		HUD_DrawTransparentTriangles,
		HUD_StudioEvent,
		HUD_PostRunCmd,
		HUD_Shutdown,
		HUD_TxferLocalOverrides,
		HUD_ProcessPlayerState,
		HUD_TxferPredictionData,
		Demo_ReadBuffer,
		HUD_ConnectionlessPacket,
		HUD_GetHullBounds,
		HUD_Frame,
		HUD_Key_Event,
		HUD_TempEntUpdate,
		HUD_GetUserEntity,
		HUD_VoiceStatus,
		HUD_DirectorMessage,
		HUD_GetStudioModelInterface,
		HUD_ChatInputPosition,
	};

	*pcldll_func = cldll_func;
}

#include "cl_dll/IGameClientExports.h"

//-----------------------------------------------------------------------------
// Purpose: Exports functions that are used by the gameUI for UI dialogs
//-----------------------------------------------------------------------------
class CClientExports : public IGameClientExports
{
public:
	// returns the name of the server the user is connected to, if any
	const char* GetServerHostName() override
	{
		/*if (gViewPortInterface)
		{
			return gViewPortInterface->GetServerName();
		}*/
		return "";
	}

	// ingame voice manipulation
	bool IsPlayerGameVoiceMuted(int playerIndex) override
	{
		if (GetClientVoiceMgr())
			return GetClientVoiceMgr()->IsPlayerBlocked(playerIndex);
		return false;
	}

	void MutePlayerGameVoice(int playerIndex) override
	{
		if (GetClientVoiceMgr())
		{
			GetClientVoiceMgr()->SetPlayerBlockedState(playerIndex, true);
		}
	}

	void UnmutePlayerGameVoice(int playerIndex) override
	{
		if (GetClientVoiceMgr())
		{
			GetClientVoiceMgr()->SetPlayerBlockedState(playerIndex, false);
		}
	}
};



EXPOSE_SINGLE_INTERFACE(CClientExports, IGameClientExports, GAMECLIENTEXPORTS_INTERFACE_VERSION);
