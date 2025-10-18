#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "r_efx.h"
#include "../renderer/rendererdefs.h"
#include "../renderer/bsprenderer.h"

#include "../renderer/tent.h"

CTempEntity gTempEntities;

extern void RestoreDecals(const char* savefile);
extern void SaveDecals(const char* savefile);

int CTempEntity::TempEntities(const char* pszName, int iSize, void* pBuf)
{
	BEGIN_READ(pBuf, iSize);

	byte type = READ_BYTE();
	switch (type)
	{
	case TE_EXPLOSION: CreateExplosion(); break;
	case TE_DECALS_SAVE_AND_RESTORE: HandleCustomDecals(); break;
	default:
		gEngfuncs.Con_DPrintf("ERROR: TempEnt: tempent type is unknown! (%u)\n", int(type));
		break;
	}

	return 1;
}

void CTempEntity::CreateExplosion()
{
	// Read 3 longs because the server will send 3 longs, duh
	Vector position = {float(READ_LONG()), float(READ_LONG()), float(READ_LONG())};
	int spriteIndex = READ_SHORT();
	float scale = READ_BYTE() / 10.0f;
	float framerate = READ_BYTE();
	int flags = READ_BYTE();

	Vector position2;
	position2.x = position.x;
	position2.y = position.y;
	position2.z = position.z;
	Vector* pos = &position2;

	// It just so happens that the TE explosion flags map directly to the engine API explosion flags, so we can pass them as-is
	gEngfuncs.pEfxAPI->R_Explosion(position, 0, scale, framerate, flags);

	cl_dlight_t* dl = gBSPRenderer.CL_AllocDLight(0);

	dl->origin = position;
	dl->radius = 200;
	dl->color.x = (float)255 / 255;
	dl->color.y = (float)208 / 255;
	dl->color.z = (float)79 / 255;
	dl->die = engine_cl->time + 0.01;
	dl->decay = 500;

	cl_dlight_t* dl2 = gBSPRenderer.CL_AllocDLight(0);

	dl2->origin = position;
	dl2->radius = 150;
	dl2->color.x = (float)255 / 255;
	dl2->color.y = (float)208 / 255;
	dl2->color.z = (float)79 / 255;
	dl2->die = engine_cl->time + 0.7;
	dl2->decay = 300;
}

void CTempEntity::HandleCustomDecals()
{
	const char* savefile = READ_STRING();
	int type = READ_LONG();
	if (type == 0)
	{
		SaveDecals(savefile);
	}
	else
	{
		RestoreDecals(savefile);
	}
}