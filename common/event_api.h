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

#define EVENT_API_VERSION 1

typedef struct msurface_s msurface_t;
typedef struct mnode_s mnode_t;

typedef struct event_api_s
{
	int version;
	void (*EV_PlaySound)(int ent, float* origin, int channel, const char* sample, float volume, float attenuation, int fFlags, int pitch);
	void (*EV_StopSound)(int ent, int channel, const char* sample);
	[[deprecated("use our own EV_FindModelIndex")]] int (*EV_FindModelIndex)(const char* pmodel);
	[[deprecated("just do (engine_cl->playernum == playernum) instead")]] int (*EV_IsLocal)(int playernum);
	[[deprecated("use our own EV_LocalPlayerDucking")]] int (*EV_LocalPlayerDucking)(void);
	[[deprecated("use our own EV_LocalPlayerViewheight")]] void (*EV_LocalPlayerViewheight)(float*);
	void (*EV_LocalPlayerBounds)(int hull, float* mins, float* maxs);
	[[deprecated("use our own EV_IndexFromTrace")]] int (*EV_IndexFromTrace)(struct pmtrace_s* pTrace);
	[[deprecated("use our own EV_GetPhysent")]] struct physent_s* (*EV_GetPhysent)(int idx);
	void (*EV_SetUpPlayerPrediction)(int dopred, int bIncludeLocalClient);
	[[deprecated("use our own EV_PushPMStates")]] void (*EV_PushPMStates)(void);
	[[deprecated("use our own EV_PopPMStates")]] void (*EV_PopPMStates)(void);
	void (*EV_SetSolidPlayers)(int playernum);
	[[deprecated("use our own EV_SetTraceHull")]] void (*EV_SetTraceHull)(int hull);
	void (*EV_PlayerTrace)(float* start, float* end, int traceFlags, int ignore_pe, struct pmtrace_s* tr);
	[[deprecated("use our own EV_WeaponAnimation")]] void (*EV_WeaponAnimation)(int sequence, int body);
	[[deprecated("use our own EV_PrecacheEvent")]] unsigned short (*EV_PrecacheEvent)(int type, const char* psz);
	[[deprecated("use our own EV_PlaybackEvent")]] void (*EV_PlaybackEvent)(int flags, const struct edict_s* pInvoker, unsigned short eventindex, float delay, float* origin, float* angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2);
	[[deprecated("use our own EV_TraceTexture")]] const char* (*EV_TraceTexture)(int ground, float* vstart, float* vend);
	void (*EV_StopAllSounds)(int entnum, int entchannel);
	[[deprecated("use our own EV_KillEvents")]] void (*EV_KillEvents)(int entnum, const char* eventname);
} event_api_t;

extern void EV_PushPMStates();
extern void EV_PopPMStates();
extern int EV_FindModelIndex(const char* pmodel);

extern int EV_LocalPlayerDucking();
extern void EV_LocalPlayerViewheight(float* viewheight);
extern bool EV_LocalPlayerDucking(float* viewheight);
extern int EV_IndexFromTrace(pmtrace_t* pTrace);
extern physent_t* EV_GetPhysent(int idx);
extern void EV_SetTraceHull(int hull);
extern void EV_WeaponAnimation(int sequence, int body);
extern unsigned short EV_PrecacheEvent(int type, const char* psz);
extern void EV_PlaybackEvent(int flags, const edict_t* pInvoker, unsigned short eventindex, float delay, float* origin, float* angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2);
extern void EV_KillEvents(int entnum, const char* eventname);
extern msurface_t* SurfaceAtPoint(model_t* pModel, mnode_t* node, vec_t* start, vec_t* end);
extern const char* EV_TraceTexture(int ground, float* vstart, float* vend);


extern event_api_t eventapi;
