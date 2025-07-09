/***
 *
 *	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
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

#pragma once

#include <variant>

#include "Platform.h"


#include <AL/al.h>
#include <AL/alc.h>

#define AL_ALEXT_PROTOTYPES
#include <AL/alext.h>

#include "r_efx.h"

struct AudioSource
{
	ALuint sourceID;
	ALuint bufferID;
	bool is_music;
};

class CSoundSystem
{
public:
	bool Init();
	void VidInit();
	void StartSound(const char* filepath);
	void StartSound_rawdata(const char* filepath, uint8_t* audiobuffer, int buffersize, int samplerate);
	bool MakeCurrent();
	void Pause();
	void Resume();
	void Update();
	void ConfigureHRTF(bool enabled);
	void PrecacheSound(const char* filepath);
	void PrecacheSound_rawdata(const char* filepath, uint8_t* audiobuffer, int buffersize, int samplerate);
	void StopSounds(bool music_too);
	std::vector<AudioSource> audiosources;
	ALCdevice* m_Device;
	ALCcontext* m_Context;
	bool m_SupportsHRTF;
	bool m_CachedHRTFEnabled;
	bool m_Paused;

};