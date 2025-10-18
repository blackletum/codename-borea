//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

// eventscripts.h

#pragma once

#include "pm_shared.h"

#define IS_FIRSTPERSON_SPEC (g_iUser1 == OBS_IN_EYE || (g_iUser1 && (gHUD.m_Spectator.m_pip->value == INSET_IN_EYE)))

// Some of these are HL/TFC specific?
void EV_EjectBrass(float* origin, float* velocity, float rotation, int model, int soundtype);
void EV_GetGunPosition(struct event_args_s* args, float* pos, float* origin);
void EV_GetDefaultShellInfo(struct event_args_s* args, float* origin, float* velocity, float* ShellVelocity, float* ShellOrigin, float* forward, float* right, float* up, float forwardScale, float upScale, float rightScale);
void EV_CreateTracer(float* start, float* end);

struct cl_entity_s* GetEntity(int idx);
void EV_MuzzleFlash();

//ported from goldsrc

__forceinline struct cl_entity_s* GetViewEntity() noexcept { return &engine_cl->viewent; };

__forceinline bool EV_IsLocal(int idx) noexcept { return (IS_FIRSTPERSON_SPEC) ? (g_iUser2 == idx) : (engine_cl->playernum == idx - 1) != 0; };
__forceinline bool EV_IsPlayer(int playernum) noexcept { return (playernum > 0 && playernum <= engine_cl->maxclients); };