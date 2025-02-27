/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
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

extern int gmsgShake;
extern int gmsgFade;
extern int gmsgSelAmmo;
extern int gmsgFlashlight;
extern int gmsgFlashBattery;
extern int gmsgResetHUD;
extern int gmsgInitHUD;
extern int gmsgKeyedDLight; //LRC
extern int gmsgKeyedELight;//LRC
extern int gmsgSetSky; // LRC
//extern int gmsgHUDColor; // LRC
extern int gmsgClampView; //LRC 1.8
extern int gmsgPlayMP3; //Killar
extern int gmsgShowGameTitle;
extern int gmsgCurWeapon;
extern int gmsgHealth;
extern int gmsgDamage;
extern int gmsgBattery;
extern int gmsgTrain;
extern int gmsgLogo;
extern int gmsgWeaponList;
extern int gmsgAmmoX;
extern int gmsgHudText;
extern int gmsgDeathMsg;
extern int gmsgScoreInfo;
extern int gmsgTeamInfo;
extern int gmsgTeamScore;
extern int gmsgGameMode;
extern int gmsgMOTD;
extern int gmsgServerName;
extern int gmsgAmmoPickup;
extern int gmsgWeapPickup;
extern int gmsgItemPickup;
extern int gmsgHideWeapon;
extern int gmsgSetCurWeap;
extern int gmsgSayText;
extern int gmsgTextMsg;
extern int gmsgSetFOV;
extern int gmsgShowMenu;
extern int gmsgGeigerRange;
extern int gmsgTeamNames;
extern int gmsgStatusIcon; //LRC
extern int gmsgStatusText;
extern int gmsgStatusValue;
extern int gmsgCamData; // for trigger_viewset
extern int gmsgRainData;
extern int gmsgInventory; //AJH Inventory system

//RENDERERS START
extern int gmsgSetFog;
extern int gmsgLightStyle;
extern int gmsgCreateDecal;
extern int gmsgStudioDecal;
extern int gmsgCreateDLight;
extern int gmsgFreeEnt;
extern int gmsgSkyMark_Sky;
extern int gmsgSkyMark_World;
extern int gmsgCreateSystem;
extern int gmsgPPGray;
extern int gmsgViewmodelSkin;
extern int gmsgLensFlare;
extern int gmsgUseEnt;
extern int gmsgGetLight;
inline int gmsgSendAnim = 0;
//RENDERERS END

extern int gmsgSpectator;
extern int gmsgPlayerBrowse;
extern int gmsgHudColor;
extern int gmsgFlagIcon;
extern int gmsgFlagTimer;
extern int gmsgPlayerIcon;
extern int gmsgVGUIMenu;
extern int gmsgAllowSpec;
extern int gmsgSetMenuTeam;
extern int gmsgCTFScore;
extern int gmsgStatsInfo;
extern int gmsgStatsPlayer;
extern int gmsgTeamFull;
extern int gmsgOldWeapon;
extern int gmsgCustomIcon;
extern int gmsgStamina;
extern int gmsgChapterName;

void LinkUserMessages();
