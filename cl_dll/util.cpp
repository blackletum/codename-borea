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
//
// util.cpp
//
// implementation of class-less helper functions
//

#include <cstdio>
#include <cstdlib>

#include "hud.h"
#include "cl_util.h"
#include <string.h>
#include "filesystem_utils.h"

HSPRITE_GOLDSRC LoadSprite(const char *pszName)
{
	int i;
	char sz[256]; 

	if (ScreenWidth < 640)
		i = 320;
	else
		i = 640;

	sprintf(sz, pszName, i);

	return SPR_Load(sz);
}

void GetFallbackDir(char* falldir)
{
    std::vector<std::byte> pfilebuffer = FileSystem_LoadFileIntoBuffer("liblist.gam", FileContentFormat::Text);
    char *pfile = (char*)pfilebuffer.data();
    char token[1024];

    if (pfilebuffer.empty())
    {
        return;
    }

    while (pfile = gEngfuncs.COM_ParseFile(pfile, token))
    {
        if (!stricmp(token, "fallback_dir"))
        {
            pfile = gEngfuncs.COM_ParseFile(pfile, token);
            strcpy(falldir, token);
            break;
        }
    }
}

//==========================
//	strUpper
//
//==========================
char* strUpper(char* str)
{
    char* temp;

    for (temp = str; *temp; temp++)
        *temp = toupper(*temp);

    return str;
}

// stub functions
void SET_MODEL(edict_t* e, const char* model) {}
int PRECACHE_MODEL(const char* s) { return 0; }