#pragma once
#include "cl_entity.h"
#include "com_model.h"
#include "crc.h"
#include "dlight.h"
#include "Platform.h"
#include "progs.h"
#include "screenfade.h"
#include "usercmd.h"

#include "net.h"

typedef void* FileHandleEngine_t;

typedef enum cactive_t : int
{
	ca_dedicated = 0,
	ca_disconnected = 1,
	ca_connecting = 2,
	ca_connected = 3,
	ca_uninitialized = 4,
	ca_active = 5
} cactive_t;

struct packet_entities_t
{
	int num_entities;
#ifdef HL25_UPDATE
	unsigned char flags[128];
#else
	unsigned char flags[32]; // SALSATOBIAS: was 128 in half-life post 25th anniversary
#endif
	entity_state_t* entities;
};

struct frame_t
{
	double receivedtime;
	double latency;
	qboolean invalid;
	qboolean choked;
	entity_state_t playerstate[32];
	double time;
	clientdata_t clientdata;
	weapon_data_t weapondata[64];
	packet_entities_t packet_entities;
	unsigned short clientbytes;
	unsigned short playerinfobytes;
	unsigned short packetentitybytes;
	unsigned short tentitybytes;
	unsigned short soundbytes;
	unsigned short eventbytes;
	unsigned short usrbytes;
	unsigned short voicebytes;
	unsigned short msgbytes;
};

struct cmd_t
{
	usercmd_t cmd;
	float senttime;
	float receivedtime;
	float frame_lerp;
	qboolean processedfuncs;
	qboolean heldback;
	int sendsize;
};

struct event_s
{
	unsigned short index;
	char* filename;
	int filesize;
	char* pszScript;
};

struct sfx_s
{
	char name[64];
	cache_user_t cache;
	int servercount;
};

struct soundfade_t
{
	int nStartPercent;
	int nClientSoundFadePercent;
	double soundFadeStartTime;
	int soundFadeOutTime;
	int soundFadeHoldTime;
	int soundFadeInTime;
};

struct downloadtime_t
{
	qboolean bUsed;
	float fTime;
	int nBytesRemaining;
};

struct incomingtransfer_t
{
	qboolean doneregistering;
	int percent;
	qboolean downloadrequested;
	downloadtime_t rgStats[8];
	int nCurStat;
	int nTotalSize;
	int nTotalToTransfer;
	int nRemainingToTransfer;
	float fLastStatusUpdate;
	qboolean custom;
};

struct client_static_s
{
	cactive_t state;
	netchan_t netchan;
	sizebuf_t datagram; // must be at 13240B
	byte datagram_buf[MAX_DATAGRAM];
	double connect_time;
	int connect_retry;
	int challenge;
	byte authprotocol;
	int userid;
	char trueaddress[32];
	float slist_time;
	int signon;
	char servername[260];
	char mapstring[64];
	char spawnparms[2048];
	char userinfo[256];
	float nextcmdtime;
	int lastoutgoingcommand;
	int demonum;
	char demos[32][16];
	qboolean demorecording;
	qboolean demoplayback;
	qboolean timedemo;
	float demostarttime;
	int demostartframe;
	int forcetrack;
	FileHandleEngine_t demofile;
	FileHandleEngine_t demoheader;
	qboolean demowaiting;
	qboolean demoappending;
	char demofilename[260];
	int demoframecount;
	int td_lastframe;
	int td_startframe;
	float td_starttime;
	incomingtransfer_t dl;
	float packet_loss;
	double packet_loss_recalc_time;
	int playerbits;
	soundfade_t soundfade;
	char physinfo[256];
	unsigned char md5_clientdll[16];
	netadr_t game_stream;
	netadr_t connect_stream;
	qboolean passive;
	qboolean spectator;
	qboolean director;
	qboolean fSecureClient;
	qboolean isVAC2Secure;

	// Splitted as 2 vars because of 8byte alignemnt required for uint64_t
	uint32_t GameServerSteamID_1;
	uint32_t GameServerSteamID_2;

	int build_num;
};

struct client_state_s
{
	int max_edicts;
	resource_s resourcesonhand;
	resource_s resourcesneeded;
	resource_s resourcelist[1280];
	int num_resources;
	qboolean need_force_consistency_response;
	char serverinfo[512];
	int servercount;
	int validsequence;
	int parsecount;
	int parsecountmod;
	int stats[32];
	int weapons;
	usercmd_t cmd;
	Vector viewangles;
	Vector punchangle;
	Vector crosshairangle;
	Vector simorg;
	Vector simvel;
	Vector simangles;
	Vector predicted_origins[64];
	Vector predition_error;
	float idealpitch;
	Vector viewheight;
	screenfade_t sf;
	qboolean paused;
	int onground;
	int moving;
	int waterlevel;
	int usehull;
	float maxspeed;
	int pushmsec;
	int light_level;
	int intermission;
	int __padding_for_mtime;
	double mtime[2];
	double time;
	double oldtime;
	frame_t frames[64];
	cmd_t commands[64];
	local_state_t predicted_frames[64];
	int delta_sequence;
	int playernum;
	event_s event_precache[256];
	model_s* model_precache[512];
	int model_precache_count;
	sfx_s* sound_precache[512];
	consistency_s consistency_list[512];
	int num_consistency;
	int highentity;
	char levelname[40];
	int maxclients;
	int gametype;
	int viewentity;
	model_s* worldmodel;
	efrag_s* free_efrags;
	int num_entities;
	int num_statics;
	cl_entity_t viewent;
	int cdtrack;
	int looptrack;
	CRC32_t serverCRC;
	char clientdllmd5[16];
	float weaponstarttime;
	int weaponsequence;
	int fPrecaching;
	dlight_t* pLight;
	player_info_t players[32];
	entity_state_t instanced_baseline[64];
	int instanced_baseline_number;
	CRC32_t mapCRC;
	event_state_t events;
	char downloadUrl[128];
};

extern client_state_s* engine_cl;
extern client_static_s* engine_cls;
