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
#include "hud.h"
#include "cl_util.h"

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "weapons/CEagle.h"
#include "weapons/fists.h"
#include "weapons/CM249.h"
#include "weapons/CDisplacer.h"
#include "weapons/CShockRifle.h"
#include "weapons/CSporeLauncher.h"
#include "weapons/CSniperRifle.h"
#include "weapons/CKnife.h"
#include "weapons/CPenguin.h"

#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "entity_types.h"
#include "usercmd.h"
#include "pm_defs.h"
#include "pm_materials.h"

#include "eventscripts.h"
#include "ev_hldm.h"

#include "r_efx.h"
#include "event_api.h"
#include "event_args.h"
#include "in_defs.h"

#include <string.h>

#include "r_studioint.h"
#include "com_model.h"

extern engine_studio_api_t IEngineStudio;

static int tracerCount[ 32 ];

#include "pm_shared.h"

void V_PunchAxis( int axis, float punch );
void VectorAngles( const float *forward, float *angles );

extern cvar_t *cl_lw;

//RENDERERS START
#include "r_studioint.h"
#include "com_model.h"
#include "bsprenderer.h"
#include "particle_engine.h"
#include "com_weapons.h"

#include "studio.h"
#include "StudioModelRenderer.h"

extern CStudioModelRenderer g_StudioRenderer;
extern engine_studio_api_t IEngineStudio;

//RENDERERS START
char *EV_HLDM_HDDecal( pmtrace_t *ptr, physent_t *pe, float *vecSrc, float *vecEnd )
{
	if(gEngfuncs.PM_PointContents( ptr->endpos, nullptr ) == CONTENT_SKY)
		return nullptr;

	// hit the world, try to play sound based on texture material type
	textureType_s chTextureType;
	int entity;
	char *pStart;
	char *pTextureName;
	char texname[ 64 ];
	char szbuffer[ 64 ];
	static char decalname[ 32 ];

	entity = EV_IndexFromTrace( ptr );

	if ( pe && pe->solid == SOLID_BSP )
	{
		// Nothing
		if ( vecSrc == nullptr && vecEnd == nullptr )
		{
			// hit body
			chTextureType = 0;
		}
		else
		{

			// get texture from entity or world (world is ent(0))
			pTextureName = (char*)EV_TraceTexture(ptr->ent, vecSrc, vecEnd);
			pStart = pTextureName;

			if(pTextureName && strcmp("black", pTextureName))
			{
				strcpy( texname, pTextureName );
				pTextureName = texname;

				// strip leading '-0' or '+0~' or '{' or '!'
				if (*pTextureName == '-' || *pTextureName == '+')
				{
					pTextureName += 2;
				}

				if (*pTextureName == '{' || *pTextureName == '!' || *pTextureName == '~' || *pTextureName == ' ')
				{
					pTextureName++;
				}
				
				// '}}'
				strcpy( szbuffer, pTextureName );
				szbuffer[ CBTEXTURENAMEMAX - 1 ] = 0;
				strupr(szbuffer); //Make String Uppercase TODO: STANDARDIZE. THIS IS WINDOWS ONLY
				// get texture type
				//chTextureType = PM_FindTextureTypeID( szbuffer );	
				chTextureType = g_TypedTextureMap[szbuffer];
			}
			else
			{
				return FALSE;
			}
		}
	}

	if(pStart[0] == '{')
		return nullptr;

	cl_entity_t *pHit = gEngfuncs.GetEntityByIndex(EV_IndexFromTrace(ptr));

	/*
	for(auto particleGroup : g_texTypeImpactTypeVector)
	{	
		// TODO: GO FULL BRANCHLESS
		bool rendModCheck = true;
		bool rendAmtCheck = true;
		bool classnoCheck = true;

		
		//if (particleGroup.renderMode < 0)
		//	rendModCheck = ((particleGroup.renderMode == NOCHECK) || (pHit->curstate.rendermode != particleGroup.renderMode));
		//else
		//	rendModCheck = ((particleGroup.renderMode == NOCHECK) || (pHit->curstate.rendermode != -1 * particleGroup.renderMode));
		//
		//if (particleGroup.renderAmt < 0)
		//	rendAmtCheck = ((particleGroup.renderAmt == NOCHECK) || (pHit->curstate.renderamt != particleGroup.renderAmt));
		//else
		//	rendAmtCheck = ((particleGroup.renderAmt == NOCHECK) || (pHit->curstate.renderamt != -1 * particleGroup.renderAmt));
		//
		//if (particleGroup.classnumber < 0)
		//	classnoCheck = ((particleGroup.classnumber == NOCHECK) || (pe->classnumber != particleGroup.classnumber));
		//else
		//	classnoCheck = ((particleGroup.classnumber == NOCHECK) || (pe->classnumber != -1 * particleGroup.classnumber));
		

		textureType_s tt;
		
		if (rendModCheck && rendAmtCheck && classnoCheck)
		{
			for (const auto& it : particleGroup.impactTypes)
			{
				for (const auto& [tKey, tValue] : g_TextureTypeMap)
				{
					if (tValue.texType == it.materialTypeAlias && tValue.texTypeID == chTextureType)
					{
						tt = g_TextureTypeMap[tKey];
					}
				}
				sprintf(decalname, it.decalGroupName.c_str());
				gParticleEngine.CreateCluster(const_cast<char*>(it.scriptFile.c_str()), ptr->endpos, ptr->plane.normal, 0);
			}
		}
		
	}
	*/

	impactType_s iType = g_texTypeImpactTypeVector[0].impactTypes[0];

	for (auto& it : g_texTypeImpactTypeVector[0].impactTypes)
	{
		if (chTextureType.texType == it.materialTypeAlias)
			iType = it;
	}

	sprintf(decalname, iType.decalGroupName.c_str());
	gParticleEngine.CreateCluster(const_cast<char*>(iType.scriptFile.c_str()), ptr->endpos, ptr->plane.normal, 0);

	return decalname;
}
//RENDERERS END

// play a strike sound based on the texture that was hit by the attack traceline.  VecSrc/VecEnd are the
// original traceline endpoints used by the attacker, iBulletType is the type of bullet that hit the texture.
// returns volume of strike instrument (crowbar) to play
float EV_HLDM_PlayTextureSound( int idx, pmtrace_t *ptr, float *vecSrc, float *vecEnd, int iBulletType )
{
	// hit the world, try to play sound based on texture material type
	textureType_s chTextureType;
	float fvol = 0.5;
	float fvolbar = 0.5;
	float fattn = ATTN_NORM;
	int entity;
	char *pTextureName;
	char texname[ 64 ];
	char szbuffer[ 64 ];

	entity = EV_IndexFromTrace( ptr );

	// FIXME check if playtexture sounds movevar is set
	//

	//chStepType = 0;

	// Player
	if ( entity >= 1 && entity <= engine_cl->maxclients )
	{
		// hit body
		chTextureType = g_TextureTypeMap["CHAR_TEX_FLESH"];
	}
	else if ( entity == 0 )
	{
		// get texture from entity or world (world is ent(0))
		pTextureName = (char *)EV_TraceTexture( ptr->ent, vecSrc, vecEnd );
		
		if ( pTextureName )
		{
			strcpy( texname, pTextureName );
			pTextureName = texname;

			// strip leading '-0' or '+0~' or '{' or '!'
			if (*pTextureName == '-' || *pTextureName == '+')
			{
				pTextureName += 2;
			}

			if (*pTextureName == '{' || *pTextureName == '!' || *pTextureName == '~' || *pTextureName == ' ')
			{
				pTextureName++;
			}
			
			// '}}'
			strcpy( szbuffer, pTextureName );
			szbuffer[ CBTEXTURENAMEMAX - 1 ] = 0;
			strupr(szbuffer); //Make String Uppercase TODO: STANDARDIZE. THIS IS WINDOWS ONLY
			// get texture type
			//chTextureType = PM_FindTextureTypeID( szbuffer );	
			chTextureType = g_TextureTypeMap[szbuffer];
		}
	}
	
	fvol = chTextureType.impactVolume;
	fvolbar = chTextureType.weaponVolume;
	fattn = chTextureType.impactAttenuation;
	

	// play material hit sound
	gEngfuncs.pEventAPI->EV_PlaySound( 0, ptr->endpos, CHAN_STATIC, g_StepTypeMap[chTextureType.texStep].stepSounds[gEngfuncs.pfnRandomLong(0, g_StepTypeMap[chTextureType.texStep].stepSounds.size() - 1)].c_str(), fvol, fattn, 0, 96 + gEngfuncs.pfnRandomLong(0, 0xf));

	return fvolbar;
}

char *EV_HLDM_DamageDecal( physent_t *pe )
{
	static char decalname[ 32 ];
	int idx;

	if ( pe->classnumber == 1 )
	{
		idx = gEngfuncs.pfnRandomLong( 0, 2 );
		sprintf( decalname, "{break%i", idx + 1 );
	}
	else if ( pe->rendermode != kRenderNormal )
	{
		sprintf( decalname, "{bproof1" );
	}
	else
	{
		idx = gEngfuncs.pfnRandomLong( 0, 4 );
		sprintf( decalname, "{shot%i", idx + 1 );
	}
	return decalname;
}

void EV_HLDM_GunshotDecalTrace( pmtrace_t *pTrace, char *decalName )
{
	int iRand;
	physent_t *pe;
	
	//RENDERERS START
	if( gParticleEngine.m_pCvarDrawParticles->value <= 0 )
		gEngfuncs.pEfxAPI->R_BulletImpactParticles( pTrace->endpos );
	//RENDERERS END


	iRand = gEngfuncs.pfnRandomLong(0,0x7FFF);
	if ( iRand < (0x7fff/2) )// not every bullet makes a sound.
	{
		switch( iRand % 5)
		{
		case 0:	gEngfuncs.pEventAPI->EV_PlaySound( -1, pTrace->endpos, 0, "weapons/ric1.wav", 1.0, ATTN_NORM, 0, PITCH_NORM ); gParticleEngine.CreateSystem("spark.txt", pTrace->endpos, vec3_origin, 0); break;
		case 1:	gEngfuncs.pEventAPI->EV_PlaySound( -1, pTrace->endpos, 0, "weapons/ric2.wav", 1.0, ATTN_NORM, 0, PITCH_NORM ); gParticleEngine.CreateSystem("spark.txt", pTrace->endpos, vec3_origin, 0); break;
		case 2:	gEngfuncs.pEventAPI->EV_PlaySound( -1, pTrace->endpos, 0, "weapons/ric3.wav", 1.0, ATTN_NORM, 0, PITCH_NORM ); gParticleEngine.CreateSystem("spark.txt", pTrace->endpos, vec3_origin, 0); break;
		case 3:	gEngfuncs.pEventAPI->EV_PlaySound( -1, pTrace->endpos, 0, "weapons/ric4.wav", 1.0, ATTN_NORM, 0, PITCH_NORM ); gParticleEngine.CreateSystem("spark.txt", pTrace->endpos, vec3_origin, 0); break;
		case 4:	gEngfuncs.pEventAPI->EV_PlaySound( -1, pTrace->endpos, 0, "weapons/ric5.wav", 1.0, ATTN_NORM, 0, PITCH_NORM ); gParticleEngine.CreateSystem("spark.txt", pTrace->endpos, vec3_origin, 0); break;
		}
	}

	pe = EV_GetPhysent( pTrace->ent );

//RENDERERS START
	// Only decal brush models such as the world etc.
	if (  decalName && decalName[0] && pe && ( pe->solid == SOLID_BSP || pe->movetype == MOVETYPE_PUSHSTEP ) )
	{
		if(pTrace->allsolid || pTrace->fraction == 1.0)
			return;

		gBSPRenderer.CreateDecal(pTrace->endpos, pTrace->plane.normal, decalName, FALSE);
	}
//RENDERERS END
}
//RENDERERS START
void EV_HLDM_DecalGunshot( pmtrace_t *pTrace, int iBulletType, float *vecSrc, float *vecEnd )
{
	physent_t *pe;

	pe = EV_GetPhysent( pTrace->ent );

	if ( pe && pe->solid == SOLID_BSP )
	{
		switch( iBulletType )
		{
		case BULLET_PLAYER_9MM:
		case BULLET_MONSTER_9MM:
		case BULLET_PLAYER_MP5:
		case BULLET_MONSTER_MP5:
		case BULLET_PLAYER_BUCKSHOT:
		case BULLET_PLAYER_357:
		case BULLET_PLAYER_556:
		case BULLET_PLAYER_762:
		case BULLET_PLAYER_EAGLE:
		default:
			// smoke and decal
			EV_HLDM_GunshotDecalTrace( pTrace, EV_HLDM_HDDecal( pTrace, pe, vecSrc, vecEnd ) );
			break;

		}
	}
}
//RENDERERS END

int EV_HLDM_CheckTracer( int idx, float *vecSrc, float *end, float *forward, float *right, int iBulletType, int iTracerFreq, int *tracerCount )
{
	int tracer = 0;
	int i;
	qboolean player = idx >= 1 && idx <= engine_cl->maxclients ? true : false;

	if ( iTracerFreq != 0 && ( (*tracerCount)++ % iTracerFreq) == 0 )
	{
		Vector vecTracerSrc;

		if ( player )
		{
			Vector offset( 0, 0, -4 );

			// adjust tracer position for player
			for ( i = 0; i < 3; i++ )
			{
				vecTracerSrc[ i ] = vecSrc[ i ] + offset[ i ] + right[ i ] * 2 + forward[ i ] * 16;
			}
		}
		else
		{
			VectorCopy( vecSrc, vecTracerSrc );
		}
		
		if ( iTracerFreq != 1 )		// guns that always trace also always decal
			tracer = 1;

		switch( iBulletType )
		{
		case BULLET_PLAYER_MP5:
		case BULLET_MONSTER_MP5:
		case BULLET_MONSTER_9MM:
		case BULLET_MONSTER_12MM:
		case BULLET_PLAYER_556:
		case BULLET_PLAYER_762:
		case BULLET_PLAYER_EAGLE:
		default:
			EV_CreateTracer( vecTracerSrc, end );
			break;
		}
	}

	return tracer;
}


/*
================
FireBullets

Go to the trouble of combining multiple pellets into a single damage call.
================
*/
void EV_HLDM_FireBullets( int idx, float *forward, float *right, float *up, int cShots, float *vecSrc, float *vecDirShooting, float flDistance, int iBulletType, int iTracerFreq, int *tracerCount, float flSpreadX, float flSpreadY )
{
	int i;
	pmtrace_t tr;
	int iShot;
	int tracer;

	for (int i = 0; i < 3; i++)
	{
		vecSrc[i] += gHUD.leanAngle * right[i];
	}
	
	for ( iShot = 1; iShot <= cShots; iShot++ )	
	{
		Vector vecDir, vecEnd;
			
		float x, y, z;
		//We randomize for the Shotgun.
		if ( iBulletType == BULLET_PLAYER_BUCKSHOT )
		{
			do {
				x = gEngfuncs.pfnRandomFloat(-0.5,0.5) + gEngfuncs.pfnRandomFloat(-0.5,0.5);
				y = gEngfuncs.pfnRandomFloat(-0.5,0.5) + gEngfuncs.pfnRandomFloat(-0.5,0.5);
				z = x*x+y*y;
			} while (z > 1);

			for ( i = 0 ; i < 3; i++ )
			{
				vecDir[i] = vecDirShooting[i] + x * flSpreadX * right[ i ] + y * flSpreadY * up [ i ];
				vecEnd[i] = vecSrc[ i ] + flDistance * vecDir[ i ];
			}
		}//But other guns already have their spread randomized in the synched spread.
		else
		{

			for ( i = 0 ; i < 3; i++ )
			{
				vecDir[i] = vecDirShooting[i] + flSpreadX * right[ i ] + flSpreadY * up [ i ];
				vecEnd[i] = vecSrc[ i ] + flDistance * vecDir[ i ];
			}
		}

		gEngfuncs.pEventAPI->EV_SetUpPlayerPrediction( false, true );
	
		// Store off the old count
		EV_PushPMStates();
	
		// Now add in all of the players.
		gEngfuncs.pEventAPI->EV_SetSolidPlayers ( idx - 1 );	

		EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( vecSrc, vecEnd, PM_STUDIO_BOX, -1, &tr );

		tracer = EV_HLDM_CheckTracer( idx, vecSrc, tr.endpos, forward, right, iBulletType, iTracerFreq, tracerCount );

		// do damage, paint decals
		if ( tr.fraction != 1.0 )
		{
			switch(iBulletType)
			{
			default:
			case BULLET_PLAYER_9MM:		
				
				EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
				//RENDERERS START
				EV_HLDM_DecalGunshot( &tr, iBulletType, vecSrc, vecEnd );
				//RENDERERS END
			
					break;
			case BULLET_PLAYER_MP5:		
				
				if ( !tracer )
				{
					EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
					//RENDERERS START
					EV_HLDM_DecalGunshot( &tr, iBulletType, vecSrc, vecEnd );
					//RENDERERS END
				}
				break;
			case BULLET_PLAYER_BUCKSHOT:
				
				//RENDERERS START
				EV_HLDM_DecalGunshot( &tr, iBulletType, vecSrc, vecEnd );
				//RENDERERS END
			
				break;
			case BULLET_PLAYER_357:
				
				EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
				//RENDERERS START
				EV_HLDM_DecalGunshot( &tr, iBulletType, vecSrc, vecEnd );
				//RENDERERS END
				
				break;

			case BULLET_PLAYER_EAGLE:
				EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
				//RENDERERS START
				EV_HLDM_DecalGunshot(&tr, iBulletType, vecSrc, vecEnd);
				//RENDERERS END
				break;

			case BULLET_PLAYER_762:
				EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
				//RENDERERS START
				EV_HLDM_DecalGunshot(&tr, iBulletType, vecSrc, vecEnd);
				//RENDERERS END
				break;

			case BULLET_PLAYER_556:
				EV_HLDM_PlayTextureSound( idx, &tr, vecSrc, vecEnd, iBulletType );
				//RENDERERS START
				EV_HLDM_DecalGunshot(&tr, iBulletType, vecSrc, vecEnd);
				//RENDERERS END
				break;
			}
		}

		EV_PopPMStates();
	}
}

//======================
//	    GLOCK START
//======================
void EV_FireGlock1( event_args_t *args )
{
	if( gHUD.KickStage > 0 ) // fuck this shit
		return;
	
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;
	int empty;

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc, vecAiming;
	Vector up, right, forward;
	
	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	empty = args->bparam1;
	AngleVectors( angles, &forward, &right, &up );

	shell = EV_FindModelIndex ("models/shell.mdl");// brass shell

	if ( EV_IsLocal( idx ) )
	{
		EV_MuzzleFlash();
	//	EV_WeaponAnimation( empty ? GLOCK_SHOOT_EMPTY : GLOCK_SHOOT, 2 );

		V_PunchAxis( 0, -2.0 );
	}

	EV_GetDefaultShellInfo(args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 20, -8, 8);

	//EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL, right ); 

//	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/pl_gun3.wav", gEngfuncs.pfnRandomFloat(0.92, 1.0), ATTN_NORM, 0, 98 + gEngfuncs.pfnRandomLong( 0, 3 ) );

	EV_GetGunPosition( args, vecSrc, origin );
	
	VectorCopy( forward, vecAiming );

	EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_9MM, 0, nullptr, args->fparam1, args->fparam2 );
}

void EV_FireGlock2( event_args_t *args )
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;
	
	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc, vecAiming;
	Vector vecSpread;
	Vector up, right, forward;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, &forward, &right, &up );

	shell = EV_FindModelIndex ("models/shell.mdl");// brass shell

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		EV_WeaponAnimation( GLOCK_SHOOT, 2 );

		V_PunchAxis( 0, -2.0 );
	}

	EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 20, -12, 4 );

	EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL); 

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/pl_gun3.wav", gEngfuncs.pfnRandomFloat(0.92, 1.0), ATTN_NORM, 0, 98 + gEngfuncs.pfnRandomLong( 0, 3 ) );

	EV_GetGunPosition( args, vecSrc, origin );
	
	VectorCopy( forward, vecAiming );

	EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_9MM, 0, &tracerCount[idx-1], args->fparam1, args->fparam2 );
	
}
//======================
//	   GLOCK END
//======================

//======================
//	  SHOTGUN START
//======================
void EV_FireShotGunDouble( event_args_t *args )
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;

	int j;
	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc, vecAiming;
	Vector vecSpread;
	Vector up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, &forward, &right, &up );

	shell = EV_FindModelIndex ("models/shotgunshell.mdl");// brass shell

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		EV_WeaponAnimation( SHOTGUN_FIRE2, 2 );
		V_PunchAxis( 0, -10.0 );
	}

	for ( j = 0; j < 2; j++ )
	{
		EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 32, -12, 12 );

		EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHOTSHELL); 
	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/m67_fire.wav", gEngfuncs.pfnRandomFloat(0.98, 1.0), ATTN_NORM, 0, 85 + gEngfuncs.pfnRandomLong( 0, 0x1f ) );

	EV_GetGunPosition( args, vecSrc, origin );
	VectorCopy( forward, vecAiming );

	if ( engine_cl->maxclients > 1 )
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 8, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT, 0, &tracerCount[idx-1], 0.17365, 0.04362 );
	}
	else
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 12, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT, 0, &tracerCount[idx-1], 0.08716, 0.08716 );
	}
}

void EV_FireShotGunSingle( event_args_t *args )
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;
	
	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc, vecAiming;
	Vector vecSpread;
	Vector up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, &forward, &right, &up );

	shell = EV_FindModelIndex ("models/shotgunshell.mdl");// brass shell

	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
		EV_WeaponAnimation( SHOTGUN_FIRE, 2 );

		V_PunchAxis( 0, -5.0 );
	}

	EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 32, -12, 12 );

	EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHOTSHELL);

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/m67_fire.wav", gEngfuncs.pfnRandomFloat(0.95, 1.0), ATTN_NORM, 0, 93 + gEngfuncs.pfnRandomLong( 0, 0x1f ) );

	EV_GetGunPosition( args, vecSrc, origin );
	VectorCopy( forward, vecAiming );

	if ( engine_cl->maxclients > 1 )
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 4, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT, 0, &tracerCount[idx-1], 0.08716, 0.04362 );
	}
	else
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 6, vecSrc, vecAiming, 2048, BULLET_PLAYER_BUCKSHOT, 0, &tracerCount[idx-1], 0.08716, 0.08716 );
	}
}
//======================
//	   SHOTGUN END
//======================

//======================
//	    MP5 START
//======================
void EV_FireMP5( event_args_t *args )
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;

	Vector ShellVelocity;
	Vector ShellOrigin;
	int shell;
	Vector vecSrc, vecAiming;
	Vector up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, &forward, &right, &up );

	shell = EV_FindModelIndex ("models/shell.mdl");// brass shell
	
	if ( EV_IsLocal( idx ) )
	{
		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
	//	EV_WeaponAnimation( MP5_FIRE1 + gEngfuncs.pfnRandomLong(0,2), 2 );

		V_PunchAxis( 0, gEngfuncs.pfnRandomFloat( -2, 2 ) );
	}

	EV_GetDefaultShellInfo( args, origin, velocity, ShellVelocity, ShellOrigin, forward, right, up, 20, -12, 4 );

	EV_EjectBrass ( ShellOrigin, ShellVelocity, angles[ YAW ], shell, TE_BOUNCE_SHELL);

	/* Aynekko: disable this
	switch( gEngfuncs.pfnRandomLong( 0, 1 ) )
	{
	case 0:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/hks1.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong( 0, 0xf ) );
		break;
	case 1:
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/hks2.wav", 1, ATTN_NORM, 0, 94 + gEngfuncs.pfnRandomLong( 0, 0xf ) );
		break;
	}*/

	EV_GetGunPosition( args, vecSrc, origin );
	VectorCopy( forward, vecAiming );

	if ( engine_cl->maxclients > 1 )
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_MP5, 2, &tracerCount[idx-1], args->fparam1, args->fparam2 );
	}
	else
	{
		EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_MP5, 2, &tracerCount[idx-1], args->fparam1, args->fparam2 );
	}
}

//======================
//	   PHYTON START 
//	     ( .357 )
//======================
void EV_FirePython( event_args_t *args )
{
	int idx;
	Vector origin;
	Vector angles;
	Vector velocity;

	Vector vecSrc, vecAiming;
	Vector up, right, forward;
	float flSpread = 0.01;

	idx = args->entindex;
	VectorCopy( args->origin, origin );
	VectorCopy( args->angles, angles );
	VectorCopy( args->velocity, velocity );

	AngleVectors( angles, &forward, &right, &up );
	
	if ( EV_IsLocal( idx ) )
	{
		// Python uses different body in multiplayer versus single player
		int multiplayer = engine_cl->maxclients == 1 ? 0 : 1;

		const auto body = multiplayer ? 1 : 0;

		// Add muzzle flash to current weapon model
		EV_MuzzleFlash();
	//	EV_WeaponAnimation( PYTHON_FIRE1, body );

		V_PunchAxis( 0, -10.0 );
		
		SetLocalBody( WEAPON_PYTHON, body );
	}

	gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_WEAPON, "weapons/d29_fire.wav", gEngfuncs.pfnRandomFloat(0.8, 0.9), ATTN_NORM, 0, PITCH_NORM );
	
	EV_GetGunPosition( args, vecSrc, origin );
	
	VectorCopy( forward, vecAiming );

	EV_HLDM_FireBullets( idx, forward, right, up, 1, vecSrc, vecAiming, 8192, BULLET_PLAYER_357, 0, nullptr, args->fparam1, args->fparam2 );
}
//======================
//	    PHYTON END 
//	     ( .357 )
//======================

//======================
//	PIPE WRENCH START
//======================
int g_iClub;

//Only predict the miss sounds, hit sounds are still played 
//server side, so players don't get the wrong idea.
void EV_Pipewrench( event_args_t *args )
{
	const int idx = args->entindex;
	Vector origin = args->origin;
	const int iBigSwing = args->bparam1;
	const int hitSomething = args->bparam2;

	if (!EV_IsLocal(idx))
	{
		return;
	}

	//Play Swing sound
	if (iBigSwing)
	{
		if (hitSomething)
		{
			EV_WeaponAnimation(PIPEWRENCH_BIG_SWING_HIT, 0);
		}
		else
		{
			EV_WeaponAnimation(PIPEWRENCH_BIG_SWING_MISS, 0);
		}

		gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/pwrench_big_miss.wav", 1, ATTN_NORM, 0, PITCH_NORM);
	}
	else
	{
		if (hitSomething)
		{
			switch (g_iClub % 3)
			{
			case 0:
				EV_WeaponAnimation(PIPEWRENCH_ATTACK1HIT, 0); break;
			case 1:
				EV_WeaponAnimation(PIPEWRENCH_ATTACK2HIT, 0); break;
			case 2:
				EV_WeaponAnimation(PIPEWRENCH_ATTACK3HIT, 0); break;
			}
		}
		else
		{
			switch (g_iClub % 3)
			{
			case 0:
				EV_WeaponAnimation(PIPEWRENCH_ATTACK1MISS, 0); break;
			case 1:
				EV_WeaponAnimation(PIPEWRENCH_ATTACK2MISS, 0); break;
			case 2:
				EV_WeaponAnimation(PIPEWRENCH_ATTACK3MISS, 0); break;
			}

			switch (g_iClub % 2)
			{
			case 0: gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/pwrench_miss1.wav", 1, ATTN_NORM, 0, PITCH_NORM); break;
			case 1: gEngfuncs.pEventAPI->EV_PlaySound(idx, origin, CHAN_WEAPON, "weapons/pwrench_miss2.wav", 1, ATTN_NORM, 0, PITCH_NORM); break;
			}
		}

		++g_iClub;
	}
}
//======================
//	 PIPE WRENCH END 
//======================

void EV_TrainPitchAdjust( event_args_t *args )
{
	int idx;
	Vector origin;

	unsigned short us_params;
	int noise;
	float m_flVolume;
	int pitch;
	int stop;
	
	char sz[ 256 ];

	idx = args->entindex;
	
	VectorCopy( args->origin, origin );

	us_params = (unsigned short)args->iparam1;
	stop	  = args->bparam1;

	m_flVolume	= (float)(us_params & 0x003f)/40.0;
	noise		= (int)(((us_params) >> 12 ) & 0x0007);
	pitch		= (int)( 10.0 * (float)( ( us_params >> 6 ) & 0x003f ) );

	switch ( noise )
	{
	case 1: strcpy( sz, "plats/ttrain1.wav"); break;
	case 2: strcpy( sz, "plats/ttrain2.wav"); break;
	case 3: strcpy( sz, "plats/ttrain3.wav"); break; 
	case 4: strcpy( sz, "plats/ttrain4.wav"); break;
	case 5: strcpy( sz, "plats/ttrain6.wav"); break;
	case 6: strcpy( sz, "plats/ttrain7.wav"); break;
	default:
		// no sound
		strcpy( sz, "" );
		return;
	}

	if ( stop )
	{
		gEngfuncs.pEventAPI->EV_StopSound( idx, CHAN_STATIC, sz );
	}
	else
	{
		gEngfuncs.pEventAPI->EV_PlaySound( idx, origin, CHAN_STATIC, sz, m_flVolume, ATTN_NORM, SND_CHANGE_PITCH, pitch );
	}
}

int EV_TFC_IsAllyTeam( int iTeam1, int iTeam2 )
{
	return 0;
}