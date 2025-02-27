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
//  hud.h
//
// class CHud declaration
//
// CHud handles the message, calculation, and drawing the HUD
//

#define FOG_LIMIT 30000

//Removed in OP4 of Solokiller, but include anyway
#define RGB_YELLOWISH 0x00FFA000 //255,160,0
#define RGB_REDISH 0x00FF1010 //255,160,0
#define RGB_GREENISH 0x0000A000 //0,160,0

#ifndef _WIN32
#define _cdecl 
#endif

#include "wrect.h"
#include "cl_dll.h"
#include "ammo.h"
#include "triangleapi.h"
#include "r_studioint.h"
#include "com_model.h"
#include <string.h>

#include <SDL2/SDL.h>

//RENDERERS START
#include "frustum.h"
#include "particle_engine.h"

struct fog_settings_t
{
	Vector color;
	int start;
	int end;
	
	bool affectsky;
	bool active;
};
//RENDERERS END
#define DHN_DRAWZERO 1
#define DHN_2DIGITS  2
#define DHN_3DIGITS  4
#define MIN_ALPHA	 100	

#define		HUDELEM_ACTIVE	1

typedef struct {
	int x, y;
} POSITION;

#include "global_consts.h"

typedef struct {
	unsigned char r,g,b,a;
} RGBA;

typedef struct cvar_s cvar_t;

extern int giR, giG, giB;


#define HUD_ACTIVE	1
#define HUD_INTERMISSION 2

#define MAX_PLAYER_NAME_LENGTH		32

#define	MAX_MOTD_LENGTH				1536

//
//-----------------------------------------------------
//
class CHudBase
{
public:
	POSITION  m_pos;
	int   m_type;
	int	  m_iFlags; // active, moving, 
	virtual		~CHudBase() {}
	virtual int Init() {return 0;}
	virtual int VidInit() {return 0;}
	virtual int Draw(float flTime) {return 0;}
	virtual void Think() {}
	virtual void Reset() {}
	virtual void InitHUDData() {}		// called every time a server is connected to



};

struct HUDLIST {
	CHudBase	*p;
	HUDLIST		*pNext;
};



//
//-----------------------------------------------------
//
#include "voice_status.h" // base voice handling class
#include "hud_spectator.h"


//
//-----------------------------------------------------
//
class CHudAmmo: public CHudBase
{
public:
	int Init() override;
	int VidInit() override;
	int Draw(float flTime) override;
	void Think() override;
	void Reset() override;
	int DrawWList(float flTime);
	int MsgFunc_CurWeapon(const char *pszName, int iSize, void *pbuf);
	int MsgFunc_WeaponList(const char *pszName, int iSize, void *pbuf);
	int MsgFunc_AmmoX(const char *pszName, int iSize, void *pbuf);
	int MsgFunc_AmmoPickup( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_WeapPickup( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_ItemPickup( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_HideWeapon( const char *pszName, int iSize, void *pbuf );

	void SlotInput( int iSlot );
	void _cdecl UserCmd_Slot1();
	void _cdecl UserCmd_Slot2();
	void _cdecl UserCmd_Slot3();
	void _cdecl UserCmd_Slot4();
	void _cdecl UserCmd_Slot5();
	void _cdecl UserCmd_Slot6();
	void _cdecl UserCmd_Slot7();
	void _cdecl UserCmd_Slot8();
	void _cdecl UserCmd_Slot9();
	void _cdecl UserCmd_Slot10();
	void _cdecl UserCmd_Close();
	void _cdecl UserCmd_NextWeapon();
	void _cdecl UserCmd_PrevWeapon();

private:
	float m_fFade;
	RGBA  m_rgba;
	WEAPON *m_pWeapon;
	int	m_HUD_bucket0;
	int m_HUD_selection;

};

//
//-----------------------------------------------------
//

class CHudAmmoSecondary: public CHudBase
{
public:
	int Init() override;
	int VidInit() override;
	void Reset() override;
	int Draw(float flTime) override;

	int MsgFunc_SecAmmoVal( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_SecAmmoIcon( const char *pszName, int iSize, void *pbuf );

private:
	enum {
		MAX_SEC_AMMO_VALUES = 4
	};

	int m_HUD_ammoicon; // sprite indices
	int m_iAmmoAmounts[MAX_SEC_AMMO_VALUES];
	float m_fFade;
};


#include "health.h"


#define FADE_TIME 100


//
//-----------------------------------------------------
//
class CHudGeiger: public CHudBase
{
public:
	int Init() override;
	int VidInit() override;
	int Draw(float flTime) override;
	int MsgFunc_Geiger(const char *pszName, int iSize, void *pbuf);
	
private:
	int m_iGeigerRange;

};

//
//-----------------------------------------------------
//

class CHudLensflare : public CHudBase
{
public:
		int Init() override;
		int VidInit() override;
		int Draw(float flTime) override;
		int MsgFunc_Lensflare(const char* pszName, int iSize, void* pbuf);

		int SunEnabled;

private:
		int Sunanglex;
		int Sunangley;

		int Sunadd[5];

		float flPlayerBlend;
		float flPlayerBlend2;
		float flPlayerBlend3;
		float flPlayerBlend4;
		float flPlayerBlend5;
		float flPlayerBlend6;

		float Screenmx;
		float Screenmy;

		float multi[10];

		int scale[10];

		int red[10];
		int green[10];
		int blue[10];

		char text[10];
		float Lensx[10];
		float Lensy[10];

		float Suncoordx;

		float Suncoordy;

		float Sundistx;
		float Sundisty;
};

class CBloom
{
public:
	bool Init(void);
	void Draw(void);
	void DrawQuad(int width, int height, int ofsX = 0, int ofsY = 0);


private:

	// TEXTURES
	unsigned int g_uiScreenTex = 0;
	unsigned int g_uiGlowTex = 0;
};


//
//-----------------------------------------------------
//
class CHudTrain: public CHudBase
{
public:
	int Init() override;
	int VidInit() override;
	int Draw(float flTime) override;
	int MsgFunc_Train(const char *pszName, int iSize, void *pbuf);

private:
	HL_HSPRITE m_hSprite;
	int m_iPos;

};

//
//-----------------------------------------------------
//
class CHudStatusBar : public CHudBase
{
public:
	int Init() override;
	int VidInit() override;
	int Draw( float flTime ) override;
	void Reset() override;
	void ParseStatusString( int line_num );

	int MsgFunc_StatusText( const char *pszName, int iSize, void *pbuf );
	int MsgFunc_StatusValue( const char *pszName, int iSize, void *pbuf );

protected:
	enum { 
		MAX_STATUSTEXT_LENGTH = 128,
		MAX_STATUSBAR_VALUES = 8,
		MAX_STATUSBAR_LINES = 3,
	};

	char m_szStatusText[MAX_STATUSBAR_LINES][MAX_STATUSTEXT_LENGTH];  // a text string describing how the status bar is to be drawn
	char m_szStatusBar[MAX_STATUSBAR_LINES][MAX_STATUSTEXT_LENGTH];	// the constructed bar that is drawn
	int m_iStatusValues[MAX_STATUSBAR_VALUES];  // an array of values for use in the status bar

	int m_bReparseString; // set to TRUE whenever the m_szStatusBar needs to be recalculated

	// an array of colors...one color for each line
	float *m_pflNameColors[MAX_STATUSBAR_LINES];
};

struct extra_player_info_t 
{
	short frags;
	short deaths;
	short playerclass;
	short health; // UNUSED currently, spectator UI would like this
	bool dead; // UNUSED currently, spectator UI would like this
	short teamnumber;
	char teamname[MAX_TEAM_NAME];
	short teamid;
	short flagcaptures;
};

struct team_info_t 
{
	char name[MAX_TEAM_NAME];
	short frags;
	short deaths;
	short ping;
	short packetloss;
	short ownteam;
	short players;
	int already_drawn;
	int scores_overriden;
	int teamnumber;
};

#include "player_info.h"

//
//-----------------------------------------------------
//
class CHudDeathNotice : public CHudBase
{
public:
	int Init() override;
	void InitHUDData() override;
	int VidInit() override;
	int Draw( float flTime ) override;
	int MsgFunc_DeathMsg( const char *pszName, int iSize, void *pbuf );

private:
	int m_HUD_d_skull;  // sprite index of skull icon
};

//
//-----------------------------------------------------
//
class CHudMenu : public CHudBase
{
public:
	int Init() override;
	void InitHUDData() override;
	int VidInit() override;
	void Reset() override;
	int Draw( float flTime ) override;
	int MsgFunc_ShowMenu( const char *pszName, int iSize, void *pbuf );

	void SelectMenuItem( int menu_item );

	int m_fMenuDisplayed;
	int m_bitsValidSlots;
	float m_flShutoffTime;
	int m_fWaitingForMore;
};

//
//-----------------------------------------------------
//
class CHudSayText : public CHudBase
{
public:
	int Init() override;
	void InitHUDData() override;
	int VidInit() override;
	int Draw( float flTime ) override;
	int MsgFunc_SayText( const char *pszName, int iSize, void *pbuf );
	void SayTextPrint( const char *pszBuf, int iBufSize, int clientIndex = -1 );
	void EnsureTextFitsInOneLineAndWrapIfHaveTo( int line );
friend class CHudSpectator;

private:

	struct cvar_s *	m_HUD_saytext;
	struct cvar_s *	m_HUD_saytext_time;
};

//
//-----------------------------------------------------
//
class CHudBattery: public CHudBase
{
public:
	int Init() override;
	int VidInit() override;
	int Draw(float flTime) override;
	int MsgFunc_Battery(const char *pszName,  int iSize, void *pbuf );

	int	  m_iBat;
	
private:
	HL_HSPRITE m_hSprite1;
	HL_HSPRITE m_hSprite2;
	wrect_t *m_prc1;
	wrect_t *m_prc2;	
	int	  m_iBatMax;
	float m_fFade;
	int	  m_iHeight;		// width of the battery innards
};


//
//-----------------------------------------------------
//
class CHudFlashlight: public CHudBase
{
public:
	int Init() override;
	int VidInit() override;
	int Draw(float flTime) override;
	void Reset() override;
	int MsgFunc_Flashlight(const char *pszName,  int iSize, void *pbuf );
	int MsgFunc_FlashBat(const char *pszName,  int iSize, void *pbuf );
	
	void drawNightVision();

private:
	HL_HSPRITE m_hSprite1;
	HL_HSPRITE m_hSprite2;
	HL_HSPRITE m_hBeam;
	HL_HSPRITE m_nvSprite;
	wrect_t *m_prc1;
	wrect_t *m_prc2;
	wrect_t *m_prcBeam;
	float m_flBat;	
	int	  m_iBat;	
	int	  m_fOn;
	float m_fFade;
	int	  m_iWidth;		// width of the battery innards
};

//
//-----------------------------------------------------
//
const int maxHUDMessages = 16;
struct message_parms_t
{
	client_textmessage_t	*pMessage;
	float	time;
	int x, y;
	int	totalWidth, totalHeight;
	int width;
	int lines;
	int lineLength;
	int length;
	int r, g, b;
	int text;
	int fadeBlend;
	float charTime;
	float fadeTime;
};

//
//-----------------------------------------------------
//

class CHudTextMessage: public CHudBase
{
public:
	int Init() override;
	static char *LocaliseTextString( const char *msg, char *dst_buffer, int buffer_size );
	static char *BufferedLocaliseTextString( const char *msg );
	const char *LookupString( const char *msg_name, int *msg_dest = nullptr );
	int MsgFunc_TextMsg(const char *pszName, int iSize, void *pbuf);
};

//
//-----------------------------------------------------
//

class CHudMessage: public CHudBase
{
public:
	int Init() override;
	int VidInit() override;
	int Draw(float flTime) override;
	int MsgFunc_HudText(const char *pszName, int iSize, void *pbuf);
	int MsgFunc_HudTextPro(const char *pszName, int iSize, void *pbuf);
	int MsgFunc_GameTitle(const char *pszName, int iSize, void *pbuf);

	float FadeBlend( float fadein, float fadeout, float hold, float localTime );
	int	XPosition( float x, int width, int lineWidth );
	int YPosition( float y, int height );

	void MessageAdd( const char *pName, float time );
	void MessageAdd(client_textmessage_t * newMessage );
	void MessageDrawScan( client_textmessage_t *pMessage, float time );
	void MessageScanStart();
	void MessageScanNextChar();
	void Reset() override;

private:
	client_textmessage_t		*m_pMessages[maxHUDMessages];
	float						m_startTime[maxHUDMessages];
	message_parms_t				m_parms;
	float						m_gameTitleTime;
	client_textmessage_t		*m_pGameTitle;

	int m_HUD_title_life;
	int m_HUD_title_half;
	int m_HUD_title_opposing;
	int m_HUD_title_force;
};

//
//-----------------------------------------------------
//
#define MAX_SPRITE_NAME_LENGTH	24

class CHudStatusIcons: public CHudBase
{
public:
	int Init() override;
	int VidInit() override;
	void Reset() override;
	int Draw(float flTime) override;
	int MsgFunc_StatusIcon(const char *pszName, int iSize, void *pbuf);
	int MsgFunc_CustomIcon(const char* pszName, int iSize, void* pbuf);

	enum { 
		MAX_ICONSPRITENAME_LENGTH = MAX_SPRITE_NAME_LENGTH,
		MAX_ICONSPRITES = 5,
		MAX_CUSTOMSPRITES = 6,
	};

	
	//had to make these public so CHud could access them (to enable concussion icon)
	//could use a friend declaration instead...
	void EnableIcon( const char *pszIconName, unsigned char red, unsigned char green, unsigned char blue );
	void DisableIcon( const char *pszIconName );

	void EnableCustomIcon(int nIndex, char* pszIconName, unsigned char red, unsigned char green, unsigned char blue, const wrect_t& aRect);
	void DisableCustomIcon(int nIndex);

private:

	typedef struct
	{
		char szSpriteName[MAX_ICONSPRITENAME_LENGTH];
		HL_HSPRITE spr;
		wrect_t rc;
		unsigned char r, g, b;
		int teamnumber; //Not actually used
	} icon_sprite_t;

	icon_sprite_t m_IconList[MAX_ICONSPRITES];
	CHudStatusIcons::icon_sprite_t m_CustomList[MAX_CUSTOMSPRITES];
};

//
//-----------------------------------------------------
//
class CHudBenchmark : public CHudBase
{
public:
	int Init() override;
	int VidInit() override;
	int Draw( float flTime ) override;

	void SetScore( float score );

	void Think() override;

	void StartNextSection( int section );

	int MsgFunc_Bench(const char *pszName, int iSize, void *pbuf);

	void CountFrame( float dt );

	int GetObjects() { return m_nObjects; }

	void SetCompositeScore();

	void Restart();

	int Bench_ScoreForValue( int stage, float raw );

private:
	float	m_fDrawTime;
	float	m_fDrawScore;
	float	m_fAvgScore;

	float   m_fSendTime;
	float	m_fReceiveTime;

	int		m_nFPSCount;
	float	m_fAverageFT;
	float	m_fAvgFrameRate;

	int		m_nSentFinish;
	float	m_fStageStarted;

	float	m_StoredLatency;
	float	m_StoredPacketLoss;
	int		m_nStoredHopCount;
	int		m_nTraceDone;

	int		m_nObjects;

	int		m_nScoreComputed;
	int 	m_nCompositeScore;
};

//
//-----------------------------------------------------
//

//LRC
//methods actually defined in tri.cpp

class CShinySurface
{
	float m_fMinX, m_fMinY, m_fMaxX, m_fMaxY, m_fZ;
	char m_fScale;
	float m_fAlpha; // texture scale and brighness
	HL_HSPRITE m_hsprSprite;
	char m_szSprite[128];

public:
	CShinySurface *m_pNext;

	CShinySurface( float fScale, float fAlpha, float fMinX, float fMaxX, float fMinY, float fMaxY, float fZ, char *szSprite);
	~CShinySurface();

	// draw the surface as seen from the given position
	void Draw(const Vector &org);

	void DrawAll(const Vector &org);
};

class CImguiManager
{
public:
	bool Init();
	bool VidInit();
	void Draw();
	void DrawChapter();
	void DrawSpeeds();

	// chapter selection variables
	bool isMenuOpen = false;
	int page;
	bool skillMode[4];
};

class CDiscordRPCManager
{
public:
	bool Init();
	bool VidInit();
	void Shutdown();
	void Update();

	float runningTime;
	float discordUpdate;
	std::string chapterName;

};

//
//-----------------------------------------------------
//


//LRC - for the moment, skymode has only two settings
#define SKY_OFF 0
#define SKY_ON_DRAWING  2
#define SKY_ON  1

class CHudFlagIcons : public CHudBase
{
public:
	int Init() override;
	int VidInit() override;
	void InitHUDData() override;
	int Draw(float flTime) override;
	void EnableFlag(const char* pszFlagName, unsigned char team_idx, unsigned char red, unsigned char green, unsigned char blue, unsigned char score);
	void DisableFlag(const char* pszFlagName, unsigned char team_idx);

	int MsgFunc_FlagIcon(const char* pszName, int iSize, void* pbuf);
	int MsgFunc_FlagTimer(const char* pszName, int iSize, void* pbuf);

private:
	enum
	{
		MAX_FLAGSPRITENAME_LENGTH = 24,
		MAX_FLAGSPRITES = 4,
	};

	struct flag_sprite_t
	{
		char szSpriteName[MAX_FLAGSPRITENAME_LENGTH];
		HL_HSPRITE spr;
		wrect_t rc;
		unsigned char r;
		unsigned char g;
		unsigned char b;
		unsigned char score;
	};


	flag_sprite_t m_FlagList[MAX_FLAGSPRITES];
	bool m_bIsTimer;
	bool m_bTimerReset;
	float m_flTimeStart;
	float m_flTimeLimit;
};


class CHudPlayerBrowse : public CHudBase
{
public:
	int Init() override;
	int VidInit() override;
	void InitHUDData() override;
	int Draw(float flTime) override;

	int MsgFunc_PlyrBrowse(const char* pszName, int iSize, void* pbuf);

private:
	enum
	{
		MAX_POWERUPSPRITENAME_LENGTH = 15,
	};

	struct powerup_sprite_t
	{
		char szSpriteName[MAX_POWERUPSPRITENAME_LENGTH];
		HL_HSPRITE spr;
		wrect_t rc;
		int r;
		int g;
		int b;
	};

	float m_flDelayFade;
	float m_flDelayFadeSprite;

	powerup_sprite_t m_PowerupSprite;

	char m_szLineBuffer[256];
	char m_szNewLineBuffer[256];

	int m_iTeamNum;
	int m_iNewTeamNum;
	int m_iHealth;
	int m_iArmor;
	bool m_fFriendly;
};

//
//-----------------------------------------------------
//
class CHudScoreboard : public CHudBase
{
public:
	int Init() override;
	void InitHUDData() override;
	int VidInit() override;
	int Draw(float flTime) override;
	int DrawPlayers(int xoffset, float listslot, int nameoffset = 0, char* team = nullptr); // returns the ypos where it finishes drawing
	void UserCmd_ShowScores();
	void UserCmd_HideScores();
	int MsgFunc_ScoreInfo(const char* pszName, int iSize, void* pbuf);
	int MsgFunc_TeamInfo(const char* pszName, int iSize, void* pbuf);
	int MsgFunc_TeamScore(const char* pszName, int iSize, void* pbuf);
	int MsgFunc_PlayerIcon(const char* pszName, int iSize, void* pbuf);
	int MsgFunc_CTFScore(const char* pszName, int iSize, void* pbuf);
	void DeathMsg(int killer, int victim);



	int m_iNumTeams;

	int m_iLastKilledBy;
	int m_fLastKillTime;
	int m_iPlayerNum;
	int m_iShowscoresHeld;

	struct cvar_s* cl_showpacketloss;

	void GetAllPlayersInfo();



};

class CHud
{
private:
	HUDLIST						*m_pHudList;
	HL_HSPRITE						m_hsprLogo;
	int							m_iLogo;
	client_sprite_t				*m_pSpriteList;
	int							m_iSpriteCount;
	int							m_iSpriteCountAllRes;
	float						m_flMouseSensitivity;
	int							m_iConcussionEffect; 

	bool mNightVisionState;

public:

	HL_HSPRITE						m_hsprCursor;
	float m_flTime;	   // the current client time
	float m_fOldTime;  // the time at which the HUD was last redrawn
	double m_flTimeDelta; // the difference between flTime and fOldTime
	Vector	m_vecOrigin;
	Vector	m_vecAngles;
	int		m_iKeyBits;
	int		m_iHideHUDDisplay;
	int		m_iFOV;
	int		m_Teamplay;
	int		m_iRes;
	cvar_t  *m_pCvarStealMouse;
	cvar_t	*m_pCvarDraw;
	cvar_t	*RainInfo;
	Vector	m_vecSkyPos; //LRC
	int		m_iSkyMode;  //LRC
	int		m_iSkyScale;	//AJH Allows parallax for the sky. 0 means no parallax, i.e infinitly large & far away.
	int m_iCameraMode;//G-Cont. clipping thirdperson camera

	//magic nipples - view lag
	float		lagangle_x;
	float		lagangle_y;
	float		lagangle_z;
	float		mouse_x;
	float		mouse_y;
	float		velz;
	float		bobValue[2];
	float		camValue[2];

	Vector		playerSpeed;
	int			wallType;
	float		lerpedRoll;
	bool		isClimbing;
	float		lerpedPitch;
	int			slowmoBar;
	bool		isSlowmo;
	float		slowmoUpdate;
	float		slowmoStrength;
	int		slowmoMode;
	bool	isRunning;
	float	leanAngle;
	ref_params_s* pparams;
	bool	m_bSliding = false;
	float	m_fLight = 0.0f;
	int		m_iScopeType;

	int m_iFontHeight;
	int DrawHudNumber(int x, int y, int iFlags, int iNumber, int r, int g, int b );
	int DrawHudString(int x, int y, int iMaxX, char *szString, int r, int g, int b );
	int DrawHudStringReverse( int xpos, int ypos, int iMinX, char *szString, int r, int g, int b );
	int DrawHudNumberString( int xpos, int ypos, int iMinX, int iNumber, int r, int g, int b );
	int GetNumWidth(int iNumber, int iFlags);
	int viewEntityIndex; // for trigger_viewset
	int viewFlags;
	int numMirrors;
	void DrawBackground(float xmin, float ymin, float xmax, float ymax, char* sprite, Vector color, int mode);

	int GetHudNumberWidth(int number, int width, int flags);
	int DrawHudNumberReverse(int x, int y, int number, int flags, int r, int g, int b);

	int m_iHUDColor; //LRC

	//Borderless Things
	//BOOL brd_isFullscreen;
	SDL_Window* BRD_GetWindow();
	void BRD_SetBorderless(SDL_Window* brd_windowArg);
	//SDL_Window* brd_window;

private:
	// the memory for these arrays are allocated in the first call to CHud::VidInit(), when the hud.txt and associated sprites are loaded.
	// freed in ~CHud()
	HL_HSPRITE *m_rghSprites;	/*[HUD_SPRITE_COUNT]*/			// the sprites loaded from hud.txt
	wrect_t *m_rgrcRects;	/*[HUD_SPRITE_COUNT]*/
	char *m_rgszSpriteNames; /*[HUD_SPRITE_COUNT][MAX_SPRITE_NAME_LENGTH]*/

	struct cvar_s *default_fov;
public:
	HL_HSPRITE GetSprite( int index ) 
	{
		return (index < 0) ? 0 : m_rghSprites[index];
	}

	wrect_t& GetSpriteRect( int index )
	{
		return m_rgrcRects[index];
	}

	
	int GetSpriteIndex( const char *SpriteName );	// gets a sprite index, for use in the m_rghSprites[] array

	CHudAmmo		m_Ammo;
	CHudHealth		m_Health;
	CHudSpectator		m_Spectator;
	CHudGeiger		m_Geiger;
	CHudBattery		m_Battery;
	CHudTrain		m_Train;
	CHudFlashlight	m_Flash;
	CHudMessage		m_Message;
	CHudScoreboard m_Scoreboard;
	CHudStatusBar   m_StatusBar;
	CHudDeathNotice m_DeathNotice;
	CHudSayText		m_SayText;
	CHudMenu		m_Menu;
	CHudAmmoSecondary	m_AmmoSecondary;
	CHudTextMessage m_TextMessage;
	CHudStatusIcons m_StatusIcons;
	CHudBenchmark	m_Benchmark;

	CHudLensflare gLensflare;
	CBloom gBloomRenderer;


	CHudFlagIcons m_FlagIcons;
	CHudPlayerBrowse m_PlayerBrowse;

	void Init();
	void VidInit();
	void Think();
	int Redraw( float flTime, int intermission );
	int UpdateClientData( client_data_t *cdata, float time );
	
	CHud() : m_iSpriteCount(0), m_pHudList(nullptr) {}
	~CHud();			// destructor, frees allocated memory

	// user messages
	int _cdecl MsgFunc_Damage(const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_GameMode(const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_Logo(const char *pszName,  int iSize, void *pbuf);
	int _cdecl MsgFunc_ResetHUD(const char *pszName,  int iSize, void *pbuf);
	void _cdecl MsgFunc_InitHUD( const char *pszName, int iSize, void *pbuf );
	void _cdecl MsgFunc_ViewMode( const char *pszName, int iSize, void *pbuf );
	int _cdecl MsgFunc_SetFOV(const char *pszName,  int iSize, void *pbuf);
	int  _cdecl MsgFunc_Concuss( const char *pszName, int iSize, void *pbuf );
	int  _cdecl MsgFunc_RainData( const char *pszName, int iSize, void *pbuf ); 		//G-Cont
	int  _cdecl MsgFunc_PlayMP3( const char *pszName, int iSize, void *pbuf );		//KILLAR
	int _cdecl MsgFunc_HUDColor(const char *pszName,  int iSize, void *pbuf);		//LRC
	void _cdecl MsgFunc_KeyedDLight( const char *pszName, int iSize, void *pbuf );	//LRC
	void _cdecl MsgFunc_SetSky( const char *pszName, int iSize, void *pbuf );		//LRC
	int  _cdecl MsgFunc_CamData( const char *pszName, int iSize, void *pbuf );		//G-Cont
	int  _cdecl MsgFunc_Inventory( const char *pszName, int iSize, void *pbuf );	//AJH
	void _cdecl MsgFunc_ClampView( const char *pszName, int iSize, void *pbuf );	//LRC 1.8

	// Screen information
	SCREENINFO	m_scrinfo;

	int	m_iWeaponBits;
	int	m_fPlayerDead;
	int m_iIntermission;

	// sprite indexes
	int m_HUD_number_0;


	void AddHudElem(CHudBase *p);

	float GetSensitivity();

//RENDERERS START
	fog_settings_t m_pSkyFogSettings;
	fog_settings_t m_pFogSettings;
	FrustumCheck viewFrustum;

	int  _cdecl MsgFunc_SetFog( const char *pszName, int iSize, void *pbuf );
	int  _cdecl MsgFunc_LightStyle( const char *pszName, int iSize, void *pbuf );
	int  _cdecl MsgFunc_StudioDecal( const char *pszName, int iSize, void *pbuf );
	int  _cdecl MsgFunc_FreeEnt( const char *pszName, int iSize, void *pbuf );

	int  _cdecl MsgFunc_CreateDecal( const char *pszName, int iSize, void *pbuf );
	int  _cdecl MsgFunc_SkyMark_S( const char *pszName, int iSize, void *pbuf );
	int  _cdecl MsgFunc_SkyMark_W( const char *pszName, int iSize, void *pbuf );
	int  _cdecl MsgFunc_DynLight( const char *pszName, int iSize, void *pbuf );
	int  _cdecl MsgFunc_CreateSystem( const char *pszName, int iSize, void *pbuf );

	int  _cdecl MsgFunc_PPGray(const char* pszName, int iSize, void* pbuf);
	int  _cdecl MsgFunc_WpnSkn(const char* pszName, int iSize, void* pbuf);
//RENDERERS END
	bool isNightVisionOn() { return mNightVisionState; }

	void setNightVisionState( bool state );

	void getNightVisionHudItemColor( int& r, int& g, int& b )
	{
		r = 255;
		g = 255;
		b = 255;
	}

	// viewmodel
	entity_state_t m_prevstate;

};

typedef struct viewinfo_s
{
	Vector attachment_forward[4];
	Vector attachment_right[4];
	Vector attachment_up[4];

	Vector bonepos[MAXSTUDIOBONES];
	Vector boneangles[MAXSTUDIOBONES];

	Vector prevbonepos[MAXSTUDIOBONES];
	Vector prevboneangles[MAXSTUDIOBONES];

	studiohdr_t* phdr;
} vminfo_t;

extern viewinfo_s g_viewinfo;

extern CHud gHUD;

extern CImguiManager g_ImGUIManager;
extern CDiscordRPCManager g_DiscordRPC;

extern int g_iPlayerClass;
extern int g_iTeamNumber;
extern int g_iInventory[MAX_ITEMS];	//AJH Inventory system
extern int g_iUser1;
extern int g_iUser2;
extern int g_iUser3;

struct FogSettings
{
	float fogColor[3];
	float startDist;
	float endDist;
};
extern FogSettings g_fog;
extern FogSettings g_fogPreFade;
extern FogSettings g_fogPostFade;
extern float g_fFogFadeDuration;
extern float g_fFogFadeFraction;
