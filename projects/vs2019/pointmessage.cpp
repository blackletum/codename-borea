// This code was made for HL:E ~ Bacontsu
// trigger_anchor, made to anchor entities to other entities

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "UserMessages.h"

class CPointMessage : public CBaseEntity
{
public:
	int Save(CSave& save) override;
	int Restore(CRestore& restore) override;
	static TYPEDESCRIPTION m_SaveData[];

	void Spawn() override;
	void Think() override;
	void KeyValue(KeyValueData* pkvd) override;

	bool m_bIsDeveloperOnly;
	float m_flMaxDistance;
};


LINK_ENTITY_TO_CLASS(point_message, CPointMessage);
TYPEDESCRIPTION CPointMessage::m_SaveData[] =
{
		DEFINE_FIELD(CPointMessage, m_bIsDeveloperOnly, FIELD_BOOLEAN),
		DEFINE_FIELD(CPointMessage, m_flMaxDistance, FIELD_FLOAT),
};
IMPLEMENT_SAVERESTORE(CPointMessage, CBaseEntity);

void CPointMessage::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "developeronly"))
	{
		m_bIsDeveloperOnly = (bool)atoi(pkvd->szValue);
		pkvd->fHandled = true;
		return;
	}
	else if (FStrEq(pkvd->szKeyName, "radius"))
	{
		m_flMaxDistance = atof(pkvd->szValue);
		pkvd->fHandled = true;
		return;
	}

	CBaseEntity::KeyValue(pkvd);
}

void CPointMessage::Spawn()
{
	pev->movetype = MOVETYPE_NONE;
	pev->solid = SOLID_NOT;
	pev->nextthink = gpGlobals->time + 0.1f;
}

void CPointMessage::Think()
{
	// if distance isnt defined, lets just use 512 units as a distance
	if (m_flMaxDistance == 0.0f)
		m_flMaxDistance = 512.0f;

	// loop through all players
	for (int i = 1; i <= gpGlobals->maxClients; i++)
	{
		CBaseEntity* pPlayer = UTIL_PlayerByIndex(i);
		if (pPlayer && pPlayer->IsNetClient())
		{
			// send this entity to client
			MESSAGE_BEGIN(MSG_ONE, gmsgMessage, pPlayer->pev->origin, pPlayer->pev);
			WRITE_COORD(pev->origin.x);
			WRITE_COORD(pev->origin.y);
			WRITE_COORD(pev->origin.z);
			WRITE_STRING(STRING(pev->message));
			WRITE_BYTE(m_bIsDeveloperOnly);
			WRITE_FLOAT(m_flMaxDistance);
			MESSAGE_END();
		}
	}
}

// func_readable
class CReadable : public CBaseEntity
{
public:

	void Spawn() override;
	void Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value) override;
	void KeyValue(KeyValueData* pkvd) override;

	virtual int ObjectCaps(void)
	{
		return (FCAP_IMPULSE_USE);
	}
	bool activated;
	string_t readable_model;

	//keyvalues for fgd:
	//opensound: string
	//closesound: string

};


LINK_ENTITY_TO_CLASS(func_readable, CReadable);


void CReadable::KeyValue(KeyValueData* pkvd)
{
	if (FStrEq(pkvd->szKeyName, "opensound"))
	{
		pev->noise = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "closesound"))
	{
		pev->noise1 = ALLOC_STRING(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}

	CBaseEntity::KeyValue(pkvd);
}

void CReadable::Spawn()
{
	//if (pev->noise != 0)
	//	PRECACHE_SOUND(STRING(pev->noise));
	//if (pev->noise1 != 0)
	//	PRECACHE_SOUND(STRING(pev->noise1));
	pev->movetype = MOVETYPE_PUSH;
	pev->solid = SOLID_BSP;
	SET_MODEL(ENT(pev), STRING(pev->model));
}

void CReadable::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if(!activated)
	{
		if (!pActivator->IsPlayer())
			ALERT(at_error, "func_readable activator is not a player !\n");

		edict_t* edictplayer = FIND_ENTITY_BY_CLASSNAME(nullptr, "player");
		CBasePlayer* player = (CBasePlayer*)CBaseEntity::Instance(edictplayer);

		// send this entity to activator
		Vector realOrigin = pev->origin + (pev->maxs + pev->mins) / 2;
		MESSAGE_BEGIN(MSG_ONE, gmsgReadable, player->pev->origin, player->pev);
		WRITE_COORD(realOrigin.x);
		WRITE_COORD(realOrigin.y);
		WRITE_COORD(realOrigin.z);
		WRITE_STRING(STRING(pev->message));
		MESSAGE_END();
		activated = true;
		if(pev->noise != 0)
			EMIT_SOUND(pActivator->edict(), CHAN_STREAM, STRING(pev->noise), VOL_NORM, ATTN_NORM);
		return;
	}
	activated = false;
	if (pev->noise1 != 0)
		EMIT_SOUND(pActivator->edict(), CHAN_STREAM, STRING(pev->noise1), VOL_NORM, ATTN_NORM);
}