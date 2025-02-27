/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
//=========================================================
// Squadmonster  functions
//=========================================================
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "nodes.h"
#include "monsters.h"
#include "animation.h"
#include "saverestore.h"
#include "talkmonster.h"
#include "squadmonster.h"
#include "COFAllyMonster.h"
#include "COFSquadTalkMonster.h"
#include "plane.h"

//=========================================================
// Save/Restore
//=========================================================
TYPEDESCRIPTION	COFSquadTalkMonster::m_SaveData[] =
{
	DEFINE_FIELD( COFSquadTalkMonster, m_hSquadLeader, FIELD_EHANDLE ),
	DEFINE_ARRAY( COFSquadTalkMonster, m_hSquadMember, FIELD_EHANDLE, MAX_SQUAD_MEMBERS - 1 ),

	// DEFINE_FIELD( COFSquadTalkMonster, m_afSquadSlots, FIELD_INTEGER ), // these need to be reset after transitions!
	DEFINE_FIELD( COFSquadTalkMonster, m_fEnemyEluded, FIELD_BOOLEAN ),
	DEFINE_FIELD( COFSquadTalkMonster, m_flLastEnemySightTime, FIELD_TIME ),

	DEFINE_FIELD( COFSquadTalkMonster, m_iMySlot, FIELD_INTEGER ),


};

IMPLEMENT_SAVERESTORE( COFSquadTalkMonster, CBaseMonster );


//=========================================================
// OccupySlot - if any slots of the passed slots are 
// available, the monster will be assigned to one.
//=========================================================
BOOL COFSquadTalkMonster::OccupySlot( int iDesiredSlots )
{
	int i;
	int iMask;
	int iSquadSlots;

	if( !InSquad() )
	{
		return TRUE;
	}

	if( SquadEnemySplit() )
	{
		// if the squad members aren't all fighting the same enemy, slots are disabled
		// so that a squad member doesn't get stranded unable to engage his enemy because
		// all of the attack slots are taken by squad members fighting other enemies.
		m_iMySlot = bits_SLOT_SQUAD_SPLIT;
		return TRUE;
	}

	COFSquadTalkMonster *pSquadLeader = MySquadLeader();

	if( !( iDesiredSlots ^ pSquadLeader->m_afSquadSlots ) )
	{
		// none of the desired slots are available. 
		return FALSE;
	}

	iSquadSlots = pSquadLeader->m_afSquadSlots;

	for( i = 0; i < NUM_SLOTS; i++ )
	{
		iMask = 1 << i;
		if( iDesiredSlots & iMask ) // am I looking for this bit?
		{
			if( !( iSquadSlots & iMask ) )	// Is it already taken?
			{
				// No, use this bit
				pSquadLeader->m_afSquadSlots |= iMask;
				m_iMySlot = iMask;
				//				ALERT ( at_aiconsole, "Took slot %d - %d\n", i, m_hSquadLeader->m_afSquadSlots );
				return TRUE;
			}
		}
	}

	return FALSE;
}

//=========================================================
// VacateSlot 
//=========================================================
void COFSquadTalkMonster::VacateSlot()
{
	if( m_iMySlot != bits_NO_SLOT && InSquad() )
	{
		//		ALERT ( at_aiconsole, "Vacated Slot %d - %d\n", m_iMySlot, m_hSquadLeader->m_afSquadSlots );
		MySquadLeader()->m_afSquadSlots &= ~m_iMySlot;
		m_iMySlot = bits_NO_SLOT;
	}
}

//=========================================================
// ScheduleChange
//=========================================================
void COFSquadTalkMonster::ScheduleChange()
{
	VacateSlot();
}

//=========================================================
// Killed
//=========================================================
void COFSquadTalkMonster::Killed( entvars_t *pevAttacker, int iGib )
{
	VacateSlot();

	if( InSquad() )
	{
		MySquadLeader()->SquadRemove( this );
	}

	COFAllyMonster::Killed( pevAttacker, iGib );
}

//=========================================================
//
// SquadRemove(), remove pRemove from my squad.
// If I am pRemove, promote m_pSquadNext to leader
//
//=========================================================
void COFSquadTalkMonster::SquadRemove( COFSquadTalkMonster *pRemove )
{
	ASSERT( pRemove != nullptr );
	ASSERT( this->IsLeader() );
	ASSERT( pRemove->m_hSquadLeader == this );

	// If I'm the leader, get rid of my squad
	if( pRemove == MySquadLeader() )
	{
		for( int i = 0; i < MAX_SQUAD_MEMBERS - 1; i++ )
		{
			COFSquadTalkMonster *pMember = MySquadMember( i );
			if( pMember )
			{
				pMember->m_hSquadLeader = nullptr;
				m_hSquadMember[ i ] = nullptr;
			}
		}
	}
	else
	{
		COFSquadTalkMonster *pSquadLeader = MySquadLeader();
		if( pSquadLeader )
		{
			for( int i = 0; i < MAX_SQUAD_MEMBERS - 1; i++ )
			{
				if( pSquadLeader->m_hSquadMember[ i ] == this )
				{
					pSquadLeader->m_hSquadMember[ i ] = nullptr;
					break;
				}
			}
		}
	}

	pRemove->m_hSquadLeader = nullptr;
}

//=========================================================
//
// SquadAdd(), add pAdd to my squad
//
//=========================================================
BOOL COFSquadTalkMonster::SquadAdd( COFSquadTalkMonster *pAdd )
{
	ASSERT( pAdd != nullptr );
	ASSERT( !pAdd->InSquad() );
	ASSERT( this->IsLeader() );

	for( int i = 0; i < MAX_SQUAD_MEMBERS - 1; i++ )
	{
		if( m_hSquadMember[ i ] == nullptr )
		{
			m_hSquadMember[ i ] = pAdd;
			pAdd->m_hSquadLeader = this;
			return TRUE;
		}
	}
	return FALSE;
	// should complain here
}


//=========================================================
// 
// SquadPasteEnemyInfo - called by squad members that have
// current info on the enemy so that it can be stored for 
// members who don't have current info.
//
//=========================================================
void COFSquadTalkMonster::SquadPasteEnemyInfo()
{
	COFSquadTalkMonster *pSquadLeader = MySquadLeader();
	if( pSquadLeader )
		pSquadLeader->m_vecEnemyLKP = m_vecEnemyLKP;
}

//=========================================================
//
// SquadCopyEnemyInfo - called by squad members who don't
// have current info on the enemy. Reads from the same fields
// in the leader's data that other squad members write to,
// so the most recent data is always available here.
//
//=========================================================
void COFSquadTalkMonster::SquadCopyEnemyInfo()
{
	COFSquadTalkMonster *pSquadLeader = MySquadLeader();
	if( pSquadLeader )
		m_vecEnemyLKP = pSquadLeader->m_vecEnemyLKP;
}

//=========================================================
// 
// SquadMakeEnemy - makes everyone in the squad angry at
// the same entity.
//
//=========================================================
void COFSquadTalkMonster::SquadMakeEnemy(CBaseEntity* pEnemy)
{
	if (m_MonsterState == MONSTERSTATE_SCRIPT)
	{
		return;
	}

	if (!InSquad())
	{
		//TODO: pEnemy could be null here
		if (m_hEnemy != nullptr)
		{
			// remember their current enemy
			PushEnemy(m_hEnemy, m_vecEnemyLKP);
		}

		ALERT(at_aiconsole, "Non-Squad friendly grunt adopted enemy of type %s\n", STRING(pEnemy->pev->classname));

		// give them a new enemy
		m_hEnemy = pEnemy;
		m_vecEnemyLKP = pEnemy->pev->origin;
		SetConditions(bits_COND_NEW_ENEMY);
	}

	if (!pEnemy)
	{
		ALERT(at_console, "ERROR: SquadMakeEnemy() - pEnemy is NULL!\n");
		return;
	}

	auto squadLeader = MySquadLeader();

	const bool fLeaderIsFollowing = squadLeader->m_hTargetEnt != nullptr && squadLeader->m_hTargetEnt->IsPlayer();
	const bool fImFollowing = m_hTargetEnt != nullptr && m_hTargetEnt->IsPlayer();

	if (!IsLeader() && fLeaderIsFollowing != fImFollowing)
	{
		ALERT(at_aiconsole, "Squad Member is not leader, and following state doesn't match in MakeEnemy\n");
		return;
	}

	for (auto& squadMemberHandle : squadLeader->m_hSquadMember)
	{
		auto squadMember = squadMemberHandle.Entity<COFSquadTalkMonster>();

		if (squadMember)
		{
			const bool isFollowing = squadMember->m_hTargetEnt != nullptr && squadMember->m_hTargetEnt->IsPlayer();

			// reset members who aren't activly engaged in fighting
			if (fLeaderIsFollowing == isFollowing && squadMember->m_hEnemy != pEnemy && !squadMember->HasConditions(bits_COND_SEE_ENEMY))
			{
				if (squadMember->m_hEnemy != nullptr)
				{
					// remember their current enemy
					squadMember->PushEnemy(squadMember->m_hEnemy, squadMember->m_vecEnemyLKP);
				}

				ALERT(at_aiconsole, "Non-Squad friendly grunt adopted enemy of type %s\n", STRING(pEnemy->pev->classname));

				// give them a new enemy
				squadMember->m_hEnemy = pEnemy;
				squadMember->m_vecEnemyLKP = pEnemy->pev->origin;
				squadMember->SetConditions(bits_COND_NEW_ENEMY);
			}
		}
	}

	//Seems a bit redundant to recalculate this now
	const bool leaderIsStillFollowing = squadLeader->m_hTargetEnt != nullptr && squadLeader->m_hTargetEnt->IsPlayer();

	// reset members who aren't activly engaged in fighting
	if (fLeaderIsFollowing == leaderIsStillFollowing && squadLeader->m_hEnemy != pEnemy && !squadLeader->HasConditions(bits_COND_SEE_ENEMY))
	{
		if (squadLeader->m_hEnemy != nullptr)
		{
			// remember their current enemy
			squadLeader->PushEnemy(squadLeader->m_hEnemy, squadLeader->m_vecEnemyLKP);
		}

		ALERT(at_aiconsole, "Non-Squad friendly grunt adopted enemy of type %s\n", STRING(pEnemy->pev->classname));

		// give them a new enemy
		squadLeader->m_hEnemy = pEnemy;
		squadLeader->m_vecEnemyLKP = pEnemy->pev->origin;
		squadLeader->SetConditions(bits_COND_NEW_ENEMY);
	}

	// reset members who aren't activly engaged in fighting
	if (squadLeader->m_hEnemy != pEnemy && !squadLeader->HasConditions(bits_COND_SEE_ENEMY))
	{
		if (squadLeader->m_hEnemy != nullptr)
		{
			// remember their current enemy
			squadLeader->PushEnemy(squadLeader->m_hEnemy, squadLeader->m_vecEnemyLKP);
		}

		ALERT(at_aiconsole, "Squad Leader friendly grunt adopted enemy of type %s\n", STRING(pEnemy->pev->classname));

		// give them a new enemy
		squadLeader->m_hEnemy = pEnemy;
		squadLeader->m_vecEnemyLKP = pEnemy->pev->origin;
		squadLeader->SetConditions(bits_COND_NEW_ENEMY);
	}
}


//=========================================================
//
// SquadCount(), return the number of members of this squad
// callable from leaders & followers
//
//=========================================================
int COFSquadTalkMonster::SquadCount()
{
	if( !InSquad() )
		return 0;

	COFSquadTalkMonster *pSquadLeader = MySquadLeader();
	int squadCount = 0;
	for( int i = 0; i < MAX_SQUAD_MEMBERS; i++ )
	{
		if( pSquadLeader->MySquadMember( i ) != nullptr )
			squadCount++;
	}

	return squadCount;
}


//=========================================================
//
// SquadRecruit(), get some monsters of my classification and
// link them as a group.  returns the group size
//
//=========================================================
int COFSquadTalkMonster::SquadRecruit( int searchRadius, int maxMembers )
{
	int squadCount;
	int iMyClass = Classify();// cache this monster's class


	// Don't recruit if I'm already in a group
	if( InSquad() )
		return 0;

	if( maxMembers < 2 )
		return 0;

	// I am my own leader
	m_hSquadLeader = this;
	squadCount = 1;

	CBaseEntity *pEntity = nullptr;

	if( !FStringNull( pev->netname ) )
	{
		// I have a netname, so unconditionally recruit everyone else with that name.
		pEntity = UTIL_FindEntityByString( pEntity, "netname", STRING( pev->netname ) );
		while( pEntity )
		{
			COFSquadTalkMonster *pRecruit = pEntity->MySquadTalkMonsterPointer();

			if( pRecruit )
			{
				if( !pRecruit->InSquad() && pRecruit->Classify() == iMyClass && pRecruit != this )
				{
					// minimum protection here against user error.in worldcraft. 
					if( !SquadAdd( pRecruit ) )
						break;
					squadCount++;
				}
			}

			pEntity = UTIL_FindEntityByString( pEntity, "netname", STRING( pev->netname ) );
		}
	}
	else
	{
		while( ( pEntity = UTIL_FindEntityInSphere( pEntity, pev->origin, searchRadius ) ) != nullptr )
		{
			COFSquadTalkMonster *pRecruit = pEntity->MySquadTalkMonsterPointer();

			if( pRecruit && pRecruit != this && pRecruit->IsAlive() && !pRecruit->m_pCine )
			{
				// Can we recruit this guy?
				if( !pRecruit->InSquad() && pRecruit->Classify() == iMyClass &&
					( ( iMyClass != CLASS_ALIEN_MONSTER ) || FStrEq( STRING( pev->classname ), STRING( pRecruit->pev->classname ) ) ) &&
					FStringNull( pRecruit->pev->netname ) )
				{
					TraceResult tr;
					UTIL_TraceLine( pev->origin + pev->view_ofs, pRecruit->pev->origin + pev->view_ofs, ignore_monsters, pRecruit->edict(), &tr );// try to hit recruit with a traceline.
					if( tr.flFraction == 1.0 )
					{
						if( !SquadAdd( pRecruit ) )
							break;

						squadCount++;
					}
				}
			}
		}
	}

	// no single member squads
	if( squadCount == 1 )
	{
		m_hSquadLeader = nullptr;
	}

	return squadCount;
}

//=========================================================
// CheckEnemy
//=========================================================
int COFSquadTalkMonster::CheckEnemy( CBaseEntity *pEnemy )
{
	int iUpdatedLKP;

	iUpdatedLKP = COFAllyMonster::CheckEnemy( m_hEnemy );

	// communicate with squad members about the enemy IF this individual has the same enemy as the squad leader.
	if( InSquad() && ( CBaseEntity * ) m_hEnemy == MySquadLeader()->m_hEnemy )
	{
		if( iUpdatedLKP )
		{
			// have new enemy information, so paste to the squad.
			SquadPasteEnemyInfo();
		}
		else
		{
			// enemy unseen, copy from the squad knowledge.
			SquadCopyEnemyInfo();
		}
	}

	return iUpdatedLKP;
}

//=========================================================
// StartMonster
//=========================================================
void COFSquadTalkMonster::StartMonster()
{
	COFAllyMonster::StartMonster();

	if( ( m_afCapability & bits_CAP_SQUAD ) && !InSquad() )
	{
		if( !FStringNull( pev->netname ) )
		{
			// if I have a groupname, I can only recruit if I'm flagged as leader
			if( !( pev->spawnflags & SF_SQUADMONSTER_LEADER ) )
			{
				return;
			}
		}

		// try to form squads now.
		int iSquadSize = SquadRecruit( 1024, 4 );

		if( iSquadSize )
		{
			ALERT( at_aiconsole, "Squad of %d %s formed\n", iSquadSize, STRING( pev->classname ) );
		}
	}

	m_flLastHitByPlayer = gpGlobals->time;
	m_iPlayerHits = 0;
}

//=========================================================
// NoFriendlyFire - checks for possibility of friendly fire
//
// Builds a large box in front of the grunt and checks to see 
// if any squad members are in that box. 
//=========================================================
BOOL COFSquadTalkMonster::NoFriendlyFire()
{
	if( !InSquad() )
	{
		return TRUE;
	}

	CPlane	backPlane;
	CPlane  leftPlane;
	CPlane	rightPlane;

	Vector	vecLeftSide;
	Vector	vecRightSide;
	Vector	v_left;

	//!!!BUGBUG - to fix this, the planes must be aligned to where the monster will be firing its gun, not the direction it is facing!!!

	if( m_hEnemy != nullptr )
	{
		UTIL_MakeVectors( UTIL_VecToAngles( m_hEnemy->Center() - pev->origin ) );
	}
	else
	{
		// if there's no enemy, pretend there's a friendly in the way, so the grunt won't shoot.
		return FALSE;
	}

	//UTIL_MakeVectors ( pev->angles );

	vecLeftSide = pev->origin - ( gpGlobals->v_right * ( pev->size.x * 1.5 ) );
	vecRightSide = pev->origin + ( gpGlobals->v_right * ( pev->size.x * 1.5 ) );
	v_left = gpGlobals->v_right * -1;

	leftPlane.InitializePlane( gpGlobals->v_right, vecLeftSide );
	rightPlane.InitializePlane( v_left, vecRightSide );
	backPlane.InitializePlane( gpGlobals->v_forward, pev->origin );

	/*
		ALERT ( at_console, "LeftPlane: %f %f %f : %f\n", leftPlane.m_vecNormal.x, leftPlane.m_vecNormal.y, leftPlane.m_vecNormal.z, leftPlane.m_flDist );
		ALERT ( at_console, "RightPlane: %f %f %f : %f\n", rightPlane.m_vecNormal.x, rightPlane.m_vecNormal.y, rightPlane.m_vecNormal.z, rightPlane.m_flDist );
		ALERT ( at_console, "BackPlane: %f %f %f : %f\n", backPlane.m_vecNormal.x, backPlane.m_vecNormal.y, backPlane.m_vecNormal.z, backPlane.m_flDist );
	*/

	COFSquadTalkMonster *pSquadLeader = MySquadLeader();
	for( int i = 0; i < MAX_SQUAD_MEMBERS; i++ )
	{
		COFSquadTalkMonster *pMember = pSquadLeader->MySquadMember( i );
		if( pMember && pMember != this )
		{

			if( backPlane.PointInFront( pMember->pev->origin ) &&
				leftPlane.PointInFront( pMember->pev->origin ) &&
				rightPlane.PointInFront( pMember->pev->origin ) )
			{
				// this guy is in the check volume! Don't shoot!
				return FALSE;
			}
		}
	}

	return TRUE;
}

//=========================================================
// GetIdealState - surveys the Conditions information available
// and finds the best new state for a monster.
//=========================================================
MONSTERSTATE COFSquadTalkMonster::GetIdealState()
{
	int	iConditions;

	iConditions = IScheduleFlags();

	// If no schedule conditions, the new ideal state is probably the reason we're in here.
	switch( m_MonsterState )
	{
	case MONSTERSTATE_IDLE:
	case MONSTERSTATE_ALERT:
		if( HasConditions( bits_COND_NEW_ENEMY ) && InSquad() )
		{
			SquadMakeEnemy( m_hEnemy );
		}
		break;
	}

	return COFAllyMonster::GetIdealState();
}

//=========================================================
// FValidateCover - determines whether or not the chosen
// cover location is a good one to move to. (currently based
// on proximity to others in the squad)
//=========================================================
BOOL COFSquadTalkMonster::FValidateCover( const Vector &vecCoverLocation )
{
	if( !InSquad() )
	{
		return TRUE;
	}

	if( SquadMemberInRange( vecCoverLocation, 128 ) )
	{
		// another squad member is too close to this piece of cover.
		return FALSE;
	}

	return TRUE;
}

//=========================================================
// SquadEnemySplit- returns TRUE if not all squad members
// are fighting the same enemy. 
//=========================================================
BOOL COFSquadTalkMonster::SquadEnemySplit()
{
	if( !InSquad() )
		return FALSE;

	COFSquadTalkMonster	*pSquadLeader = MySquadLeader();
	CBaseEntity		*pEnemy = pSquadLeader->m_hEnemy;

	for( int i = 0; i < MAX_SQUAD_MEMBERS; i++ )
	{
		COFSquadTalkMonster *pMember = pSquadLeader->MySquadMember( i );
		if( pMember != nullptr && pMember->m_hEnemy != nullptr && pMember->m_hEnemy != pEnemy )
		{
			return TRUE;
		}
	}
	return FALSE;
}

//=========================================================
// FValidateCover - determines whether or not the chosen
// cover location is a good one to move to. (currently based
// on proximity to others in the squad)
//=========================================================
BOOL COFSquadTalkMonster::SquadMemberInRange( const Vector &vecLocation, float flDist )
{
	if( !InSquad() )
		return FALSE;

	COFSquadTalkMonster *pSquadLeader = MySquadLeader();

	for( int i = 0; i < MAX_SQUAD_MEMBERS; i++ )
	{
		COFSquadTalkMonster *pSquadMember = pSquadLeader->MySquadMember( i );
		if( pSquadMember && ( vecLocation - pSquadMember->pev->origin ).Length2D() <= flDist )
			return TRUE;
	}
	return FALSE;
}


extern Schedule_t	slChaseEnemyFailed[];

Schedule_t *COFSquadTalkMonster::GetScheduleOfType( int iType )
{
	switch( iType )
	{

	case SCHED_CHASE_ENEMY_FAILED:
		{
			return &slChaseEnemyFailed[ 0 ];
		}

	default:
		return COFAllyMonster::GetScheduleOfType( iType );
	}
}

void COFSquadTalkMonster::FollowerUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// Don't allow use during a scripted_sentence
	if (m_useTime > gpGlobals->time)
		return;

	if (pCaller != nullptr && pCaller->IsPlayer())
	{
		// Pre-disaster followers can't be used
		if (pev->spawnflags & SF_MONSTER_PREDISASTER)
		{
			DeclineFollowing();
		}
		else if (CanFollow())
		{
			//Player can form squads of up to 6 NPCs
			LimitFollowers(pCaller, 6);

			if (m_afMemory & bits_MEMORY_PROVOKED)
				ALERT(at_console, "I'm not following you, you evil person!\n");
			else
			{
				StartFollowing(pCaller);
				SetBits(m_bitsSaid, bit_saidHelloPlayer);	// Don't say hi after you've started following
			}
		}
		else
		{
			StopFollowing(TRUE);
		}
	}
}

COFSquadTalkMonster* COFSquadTalkMonster::MySquadMedic()
{
	for( auto& member : m_hSquadMember )
	{
		auto pMember = member.Entity<COFSquadTalkMonster>();

		if( pMember && FClassnameIs( pMember->pev, "monster_human_medic_ally" ) )
		{
			return pMember;
		}
	}

	return nullptr;
}

COFSquadTalkMonster* COFSquadTalkMonster::FindSquadMedic( int searchRadius )
{
	for( CBaseEntity* pEntity = nullptr; ( pEntity = UTIL_FindEntityInSphere( pEntity, pev->origin, searchRadius ) ); )
	{
		auto pMonster = pEntity->MySquadTalkMonsterPointer();

		if( pMonster
			&& pMonster != this
			&& pMonster->IsAlive()
			&& !pMonster->m_pCine
			&& FClassnameIs( pMonster->pev, "monster_human_medic_ally" ) )
		{
			return pMonster;
		}
	}

	return nullptr;
}

BOOL COFSquadTalkMonster::HealMe( COFSquadTalkMonster* pTarget )
{
	return false;
}

int COFSquadTalkMonster::TakeDamage(entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType)
{
	if (m_MonsterState == MONSTERSTATE_SCRIPT)
	{
		return COFAllyMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
	}

	//If this attack deals enough damage to instakill me...
	if (pev->deadflag == DEAD_NO && flDamage >= pev->max_health)
	{
		//Tell my squad mates...
		auto pSquadLeader = MySquadLeader();

		for (int i = 0; i < MAX_SQUAD_MEMBERS; i++)
		{
			COFSquadTalkMonster* pSquadMember = pSquadLeader->MySquadMember(i);

			//If they're alive and have no enemy...
			if (pSquadMember && pSquadMember->IsAlive() && !pSquadMember->m_hEnemy)
			{
				//If they're not being eaten by a barnacle and the attacker is a player...
				if (m_MonsterState != MONSTERSTATE_PRONE && (pevAttacker->flags & FL_CLIENT))
				{
					//Friendly fire!
					pSquadMember->Remember(bits_MEMORY_PROVOKED);
				}
				//Attacked by an NPC...
				else
				{
					g_vecAttackDir = ((pevAttacker->origin + pevAttacker->view_ofs) - (pSquadMember->pev->origin + pSquadMember->pev->view_ofs)).Normalize();

					const Vector vecStart = pSquadMember->pev->origin + pSquadMember->pev->view_ofs;
					const Vector vecEnd = pevAttacker->origin + pevAttacker->view_ofs + (g_vecAttackDir * m_flDistLook);
					TraceResult tr;

					UTIL_TraceLine(vecStart, vecEnd, dont_ignore_monsters, pSquadMember->edict(), &tr);

					//If they didn't see any enemy...
					if (tr.flFraction == 1.0)
					{
						//Hunt for enemies
						m_IdealMonsterState = MONSTERSTATE_HUNT;
					}
					//They can see an enemy
					else
					{
						//Make the enemy an enemy of my squadmate
						pSquadMember->m_hEnemy = CBaseEntity::Instance(tr.pHit);
						pSquadMember->m_vecEnemyLKP = pevAttacker->origin;
						pSquadMember->SetConditions(bits_COND_NEW_ENEMY);
					}
				}
			}
		}
	}

	m_flWaitFinished = gpGlobals->time;

	return COFAllyMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}
