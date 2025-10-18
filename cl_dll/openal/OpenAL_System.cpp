#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <exception>
#include <type_traits>
#include <filesystem>

#include "libnyquist/Decoders.h"

#include "hud.h"
#include "cl_util.h"
#include "extdll.h"
#include "util.h"
#include "parsemsg.h"
#include "view.h"

#include "OpenAL_System.h"

#include "filesystem_utils.h"

std::map<std::string, ALuint> audioBuffers;

CSoundSystem gSoundSystem;
nqr::NyquistIO decoder;
extern Vector v_origin, v_angles, v_cl_angles, v_sim_org, v_lastAngles, v_client_aimangles;
extern int g_ViewEntity;

bool CSoundSystem::Init()
{
	m_Device = alcOpenDevice(nullptr); // Open default device
	if (!m_Device)
	{
		gEngfuncs.Con_DPrintf("Failed to open audio device.\n");
		return -1;
	}

	m_Context = alcCreateContext(m_Device, nullptr);
	if (!m_Context || !alcMakeContextCurrent(m_Context))
	{
		gEngfuncs.Con_DPrintf("Failed to set audio context.\n");
		if (m_Context)
			alcDestroyContext(m_Context);
		alcCloseDevice(m_Device);
		return -1;
	}

	alDistanceModel(AL_LINEAR_DISTANCE);

	// GoldSource uses units, where 16 units == 1 foot.
	// See https://developer.valvesoftware.com/wiki/Dimensions
	constexpr double feetPerMeter = 3.281;
	constexpr double footInMeters = 1 / feetPerMeter;
	constexpr double metersPerUnit = footInMeters / 16;
	alListenerf(AL_METERS_PER_UNIT, static_cast<float>(metersPerUnit));

	alDopplerFactor(0.f);

	m_SupportsHRTF = alcIsExtensionPresent(m_Device, "ALC_SOFT_HRTF") != ALC_FALSE;

	if (m_SupportsHRTF)
	{
		gEngfuncs.Con_DPrintf("HRTF is supported");
	}
	else
	{
		gEngfuncs.Con_DPrintf("HRTF is not supported");
	}

	gEngfuncs.Con_DPrintf("OpenAL initialized successfully!\n");

	// Clean up

	return 1;
}

void CSoundSystem::PrecacheSound_rawdata(const char* filepath, uint8_t* audiobuffer, int buffersize, int samplerate)
{
	if (audioBuffers.find(filepath) != audioBuffers.end())
		return; // already cached


	if (AL_FALSE == alIsExtensionPresent("AL_EXT_FLOAT32"))
	{
		return;
	}

	ALuint buffer;
	alGenBuffers(1, &buffer);
	const ALenum format = AL_FORMAT_STEREO16;
	alBufferData(buffer, format, audiobuffer,
		buffersize,
		samplerate);

	if (const auto error = alGetError(); error != AL_NO_ERROR)
	{
		const char* errorstring = alGetString(error);
		gEngfuncs.Con_Printf("OpenAL error %s %d while initializing buffer for %s",
			alGetString(error), error, filepath);
		return;
	}
	audioBuffers[filepath] = buffer;
}

void CSoundSystem::PrecacheSound(const char* filepath)
{
	std::string dummy = filepath;
	std::replace(dummy.begin(), dummy.end(), '\\', '/');

	if (audioBuffers.find(dummy) != audioBuffers.end())
		return; // already cached

	if (strstr(filepath, ".wav") || strstr(filepath, ".ogg") || strstr(filepath, ".mp3"))
	{
		nqr::AudioData audioData;
		try
		{
			decoder.Load(&audioData, filepath);
		}
		catch (const std::exception& e)
		{
			gEngfuncs.Con_Printf("Error loading sound file {}: {}", filepath, e.what());
			return;
		}

		if (AL_FALSE == alIsExtensionPresent("AL_EXT_FLOAT32"))
		{
			return;
		}

		ALuint buffer;
		alGenBuffers(1, &buffer);
		const ALenum format = audioData.channelCount == 1 ? AL_FORMAT_MONO_FLOAT32 : AL_FORMAT_STEREO_FLOAT32;
		alBufferData(buffer, format, audioData.samples.data(),
			audioData.samples.size() * sizeof(float),
			audioData.sampleRate);

		if (const auto error = alGetError(); error != AL_NO_ERROR)
		{
			const char* errorstring = alGetString(error);
			gEngfuncs.Con_Printf("OpenAL error %s %d while initializing buffer for %s",
				alGetString(error), error, filepath);
			return;
		}
		audioBuffers[dummy] = buffer;
	}
}

void CSoundSystem::StartSound_rawdata(const char* filepath /*mp4 file path*/, uint8_t* audiobuffer, int buffersize, int samplerate, float volume)
{
	ALuint source;
	AudioSource audiosource;

	PrecacheSound_rawdata(filepath, audiobuffer, buffersize, samplerate);

	ALuint& buffer = audioBuffers[filepath];

	alGenSources(1, &source);

	alSourcei(source, AL_BUFFER, buffer);

	alSourcef(source, AL_GAIN, volume / 100);
	alSourcef(source, AL_PITCH, 1);
	alSourcef(source, AL_REFERENCE_DISTANCE, 1);
	alSourcei(source, AL_SOURCE_RELATIVE, AL_FALSE);
	alSourcei(source, AL_LOOPING, AL_FALSE);
	alSourcef(source, AL_ROLLOFF_FACTOR, 0.0f);
	alSourcePlay(source);
	audiosource.sourceID = source;
	audiosource.bufferID = audioBuffers[filepath];
	audiosource.is_music = false;
	audiosources.push_back(audiosource);

	ALint state;
	alGetSourcei(source, AL_SOURCE_STATE, &state);
	if (state != AL_PLAYING) {
		gEngfuncs.Con_Printf("OpenAL source is not playing: state = %d\n", state);
	}

	int error = alGetError();
	if (error != AL_NO_ERROR) {
		gEngfuncs.Con_Printf("OpenAL error number %d\n", error);
	}
	//gEngfuncs.Con_Printf("sourceid: %d\n bufferid: %d\n fullpath: %s\n entindex: %d\n channelindex: %d\n", 
	//					audiosource.sourceID, audiosource.bufferID, fullpath,  audiosource.entindex, audiosource.channelindex);
}

void CSoundSystem::StartSound(const char* filepath)
{
	nqr::AudioData audioData;

	std::string fullpath = filepath;

	PrecacheSound(fullpath.c_str());


	ALuint source;
	AudioSource audiosource;

	alGenSources(1, &source);

	alSourcei(source, AL_BUFFER, audioBuffers[fullpath]);
	alSourcef(source, AL_GAIN, 1);
	alSourcef(source, AL_PITCH, 1);
	alSourcef(source, AL_REFERENCE_DISTANCE, 1);
	alSourcei(source, AL_SOURCE_RELATIVE, AL_FALSE);
	alSourcei(source, AL_LOOPING, AL_FALSE);
	alSourcef(source, AL_ROLLOFF_FACTOR, 0.0f);
	alSourcePlay(source);
	audiosource.sourceID = source;
	audiosource.bufferID = audioBuffers[fullpath];
	audiosources.push_back(audiosource);
	//gEngfuncs.Con_Printf("sourceid: %d\n bufferid: %d\n fullpath: %s\n entindex: %d\n channelindex: %d\n", 
	//					audiosource.sourceID, audiosource.bufferID, fullpath,  audiosource.entindex, audiosource.channelindex);
}

bool CSoundSystem::MakeCurrent()
{
	if (ALC_FALSE == alcMakeContextCurrent(m_Context))
	{
		gEngfuncs.Con_Printf("Couldn't make OpenAL context current");
		return false;
	}

	return true;
}

void CSoundSystem::Pause()
{
	if (!MakeCurrent())
	{
		return;
	}

	if (m_Paused)
	{
		return;
	}

	m_Paused = true;

	for (auto& channel : audiosources)
	{
		alSourcePause(channel.sourceID);
	}
}

void CSoundSystem::Resume()
{
	if (!MakeCurrent())
	{
		return;
	}

	if (!m_Paused)
	{
		return;
	}

	m_Paused = false;

	for (auto& channel : audiosources)
	{
		alSourcePlay(channel.sourceID);
	}
}

extern cvar_t* ffmpeg_soundvolume;

void CSoundSystem::Update()
{
	bool volume_changed;
	static float volume = 0;

	ffmpeg_soundvolume->value = std::clamp(ffmpeg_soundvolume->value, 0.f, 100.f);

	volume_changed = volume != ffmpeg_soundvolume->value;
	volume = ffmpeg_soundvolume ? ffmpeg_soundvolume->value : 0;
	for (auto it = audiosources.begin(); it != audiosources.end();)
	{
		ALint state;
		alGetSourcei(it->sourceID, AL_SOURCE_STATE, &state);

		if (volume_changed);
			alSourcef(it->sourceID, AL_GAIN, volume / 100);

		if (state == AL_STOPPED)
		{
			alDeleteSources(1, &it->sourceID);
			it = audiosources.erase(it); // Remove from the collection
		}
		else
		{
			++it;
		}
	}

}

void CSoundSystem::StopSounds(bool music_too)
{
	for (auto it = audiosources.begin(); it != audiosources.end();)
	{
		if (it->is_music && !music_too)
		{
			it++;
			continue;
		}
		alSourceStop(it->sourceID);
		alDeleteSources(1, &it->sourceID);
		it = audiosources.erase(it);
	}

}

void CSoundSystem::ConfigureHRTF(bool enabled)
{
	ALCint numHRTF;
	alcGetIntegerv(m_Device, ALC_NUM_HRTF_SPECIFIERS_SOFT, 1, &numHRTF);

	if (numHRTF == 0)
	{
		gEngfuncs.Con_Printf("No HRTF implementations found");
		return;
	}

	ALCint attribs[3]{ 0 };

	int i = 0;
	attribs[i++] = ALC_HRTF_SOFT;
	attribs[i++] = enabled ? ALC_TRUE : ALC_FALSE;

	attribs[i++] = 0;

	typedef ALCboolean(ALC_APIENTRY* LPALCRESETDEVICESOFT)(ALCdevice*, const ALCint*);
	LPALCRESETDEVICESOFT alcResetDeviceSOFT = (LPALCRESETDEVICESOFT)alcGetProcAddress(m_Device, "alcResetDeviceSOFT");
	if (!alcResetDeviceSOFT)
	{
		gEngfuncs.Con_Printf("Failed to load alcResetDeviceSOFT");
		return;
	}
	if (ALC_FALSE == alcResetDeviceSOFT(m_Device, attribs))
	{
		gEngfuncs.Con_Printf("Failed to reset device: %s", alcGetString(m_Device, alcGetError(m_Device)));
	}
}