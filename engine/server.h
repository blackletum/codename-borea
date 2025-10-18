/*
 *
 *    This program is free software; you can redistribute it and/or modify it
 *    under the terms of the GNU General Public License as published by the
 *    Free Software Foundation; either version 2 of the License, or (at
 *    your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful, but
 *    WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software Foundation,
 *    Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *    In addition, as a special exception, the author gives permission to
 *    link the code of this program with the Half-Life Game Engine ("HL
 *    Engine") and Modified Game Libraries ("MODs") developed by Valve,
 *    L.L.C ("Valve").  You must obey the GNU General Public License in all
 *    respects for all of the code used other than the HL Engine and MODs
 *    from Valve.  If you modify this file, you may extend this exception
 *    to your version of the file, but you are not obligated to do so.  If
 *    you do not wish to do so, delete this exception statement from your
 *    version.
 *
 */

#pragma once

// TODO: I think this defines must be in /common/
const int NUM_EDICTS = 900;
const int MAX_NAME = 32;
#include "crc.h"
#include "dll_state.h"
#include "entity_state.h"
#include "net.h"
#include "progs.h"
#include "filter.h"
#include "pm_defs.h"
#include "client_state.h"

const int DEFAULT_SOUND_PACKET_VOLUME = 255;
const float DEFAULT_SOUND_PACKET_ATTENUATION = 1.0f;
const int DEFAULT_SOUND_PACKET_PITCH = 100;


const int NUM_BASELINES = 64;

#define BIT(n) (1 << (n))

#define MAX_QPATH 64 // Must match value in quakedefs.h
#define MAX_RESOURCE_LIST 1280

//
// per-level limits
//
#define MAX_EDICTS 900 // FIXME: ouch! ouch! ouch!
#define MAX_LIGHTSTYLES 64
#define MAX_MODELS 512 // these are sent over the net as bytes
#define MAX_SOUNDS 512 // so they cannot be blindly increased
#define MAX_USER_MSG_DATA 192
#define MAX_EVENTS 256

#define MAX_USERMSGS 128 // TODO: ???

#define MAX_SOUNDS_HASHLOOKUP_SIZE (MAX_SOUNDS * 2 - 1)

#define MAX_GENERIC_INDEX_BITS 9
#define MAX_GENERIC (1 << MAX_GENERIC_INDEX_BITS)

// Sound flags
enum
{
	SND_FL_VOLUME = BIT(0),		  // send volume
	SND_FL_ATTENUATION = BIT(1),  // send attenuation
	SND_FL_LARGE_INDEX = BIT(2),  // send sound number as short instead of byte
	SND_FL_PITCH = BIT(3),		  // send pitch
	SND_FL_SENTENCE = BIT(4),	  // set if sound num is actually a sentence num
	SND_FL_STOP = BIT(5),		  // stop the sound
	SND_FL_CHANGE_VOL = BIT(6),	  // change sound vol
	SND_FL_CHANGE_PITCH = BIT(7), // change sound pitch
	SND_FL_SPAWNING = BIT(8)	  // we're spawning, used in some cases for ambients (not sent across network)
};

// Message send destination flags
enum
{
	MSG_FL_NONE = 0,		   // No flags
	MSG_FL_BROADCAST = BIT(0), // Broadcast?
	MSG_FL_PVS = BIT(1),	   // Send to PVS
	MSG_FL_PAS = BIT(2),	   // Send to PAS
	MSG_FL_ONE = BIT(7),	   // Send to single client
};

const int RESOURCE_INDEX_BITS = 12;

typedef struct extra_baselines_s
{
	int number;
	int classname[NUM_BASELINES];
	entity_state_t baseline[NUM_BASELINES];
} extra_baselines_t;

typedef enum redirect_e
{
	RD_NONE = 0,
	RD_CLIENT = 1,
	RD_PACKET = 2,
} redirect_t;

typedef enum server_state_e
{
	ss_dead = 0,
	ss_loading = 1,
	ss_active = 2,
} server_state_t;

typedef struct server_s
{
	qboolean active;
	qboolean paused;
	qboolean loadgame;
	double time;
	double oldtime;
	int lastcheck;
	double lastchecktime;
	char name[64];
	char oldname[64];
	char startspot[64];
	char modelname[64];
	struct model_s* worldmodel;
	CRC32_t worldmapCRC;
	unsigned char clientdllmd5[16];
	resource_t resourcelist[MAX_RESOURCE_LIST];
	int num_resources;
	consistency_t consistency_list[MAX_CONSISTENCY_LIST];
	int num_consistency;
	const char* model_precache[MAX_MODELS];
	struct model_s* models[MAX_MODELS];
	unsigned char model_precache_flags[MAX_MODELS];
	struct event_s event_precache[MAX_EVENTS];
	const char* sound_precache[MAX_SOUNDS];
	short int sound_precache_hashedlookup[MAX_SOUNDS_HASHLOOKUP_SIZE];
	qboolean sound_precache_hashedlookup_built;
	const char* generic_precache[MAX_GENERIC];
	char generic_precache_names[MAX_GENERIC][64];
	int num_generic_names;
	const char* lightstyles[MAX_LIGHTSTYLES];
	int num_edicts;
	int max_edicts;
	edict_t* edicts;
	struct entity_state_s* baselines;
	extra_baselines_t* instance_baselines;
	server_state_t state;
	sizebuf_t datagram;
	unsigned char datagram_buf[MAX_DATAGRAM];
	sizebuf_t reliable_datagram;
	unsigned char reliable_datagram_buf[MAX_DATAGRAM];
	sizebuf_t multicast;
	unsigned char multicast_buf[1024];
	sizebuf_t spectator;
	unsigned char spectator_buf[1024];
	sizebuf_t signon;
	unsigned char signon_data[32768];
} server_t;