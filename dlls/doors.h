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

// doors
#define SF_DOOR_ROTATE_Y			0
#define	SF_DOOR_START_OPEN			1
#define SF_DOOR_ROTATE_BACKWARDS	2
#define SF_DOOR_PASSABLE			8
#define SF_DOOR_ONEWAY				16
#define	SF_DOOR_NO_AUTO_RETURN		32
#define SF_DOOR_ROTATE_Z			64
#define SF_DOOR_ROTATE_X			128
#define SF_DOOR_USE_ONLY			256	// door must be opened by player's use button.
#define SF_DOOR_NOMONSTERS			512	// Monster can't open
#define SF_DOOR_FORCETOUCHABLE		1024 //LRC- Opens when touched, even though it's named and/or "use only"
//LRC - clashes with 'not in deathmatch'. Replaced with 'Target mode' and 'On/Off Mode' fields.
//#define SF_DOOR_SYNCHED				2048 //LRC- sends USE_ON/OFF when it starts to open/close (instead of sending
										 // USE_TOGGLE when fully open/closed); also responds to USE_ON and USE_OFF
										 // 'correctly'.
#define SF_DOOR_SILENT				0x80000000
