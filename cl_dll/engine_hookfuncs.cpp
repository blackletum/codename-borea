#include "hud.h"
#include "cl_util.h"
#include "r_efx.h"

#include "com_model.h"
#include "triangleapi.h"

#include "client_state.h"

#include "StudioModelRenderer.h"

#include "particle_engine.h"
#include "bsprenderer.h"

#include "entity_types.h"

// Global engine <-> studio model rendering code interface
extern engine_studio_api_t IEngineStudio;

extern CStudioModelRenderer g_StudioRenderer;

extern int CL_Internal_AddEntity(int type, cl_entity_t* ent);

client_state_s* engine_cl = nullptr;

extern model_t* cl_sprite_dot;
extern model_t* cl_sprite_lightning;
extern model_t* cl_sprite_white;
extern model_t* cl_sprite_glow;
extern model_t* cl_sprite_muzzleflash[3];
extern model_t* cl_sprite_ricochet;
extern model_t* cl_sprite_shell;

std::vector<TEMPENTITY*> gpTempEnts;


int ModelFrameCount(model_t* model)
{
	if (!model)
		return 1;

	if (model->type == mod_sprite)
		return ((msprite_t*)model->cache.data)->numframes;

	if (model->type != mod_studio)
		return 1;
	studiohdr_t* shdr = (studiohdr_t*)IEngineStudio.Mod_Extradata(model);
	if (!shdr)
		return 1;

	int numbodyparts = shdr->numbodyparts;
	int bodypartindex = shdr->bodypartindex;
	if (numbodyparts <= 0)
		return 1;

	int count = 1;
	mstudiobodyparts_t* pbodypart = (mstudiobodyparts_t*)((char*)shdr + shdr->bodypartindex);
	for (int i = 0; i < shdr->numbodyparts; i++, pbodypart++)
	{
		count *= pbodypart->nummodels;
	}
	return count;

}

void SinCos_(float radians, float* sine, float* cosine)
{
	*sine = sin(radians);
	*cosine = cos(radians);
}

#define MAX_TEMPENTS 4096



#define FuncHook(FuncName, RetType, ...)           \
	typedef RetType (*Type##FuncName)(##__VA_ARGS__); \
	static Type##FuncName Orig##FuncName = nullptr;   \
	static RetType Hooked_##FuncName(##__VA_ARGS__)

#define InstallEfxHook(FuncName)                           \
	if (Orig##FuncName == nullptr)                         \
	{                                                      \
		Orig##FuncName = gEngfuncs.pEfxAPI->##FuncName;    \
		gEngfuncs.pEfxAPI->##FuncName = Hooked_##FuncName; \
	}

#define InstallTriApiHook(FuncName)                           \
	if (Orig##FuncName == nullptr)                         \
	{                                                      \
		Orig##FuncName = gEngfuncs.pTriAPI->##FuncName;    \
		gEngfuncs.pTriAPI-> ##FuncName = Hooked_##FuncName; \
	}

#define InstallEventApiHook(FuncName)                      \
	if (Orig##FuncName == nullptr)                         \
	{                                                      \
		Orig##FuncName = gEngfuncs.pEventAPI->##FuncName;    \
		gEngfuncs.pEventAPI->##FuncName = Hooked_##FuncName; \
	}


#define InstallIEngineStudioHook(FuncName)                 \
	if (Orig##FuncName == nullptr)                         \
	{                                                      \
		Orig##FuncName = IEngineStudio.##FuncName;    \
		IEngineStudio.##FuncName = Hooked_##FuncName; \
	}


FuncHook(CL_TempEntAlloc, TEMPENTITY*, float* org, struct model_s* model)
{
	if (gpTempEnts.size() > MAX_TEMPENTS)
	{
		gEngfuncs.Con_DPrintf("Overflow %d temporary ents!\n", MAX_TEMPENTS);
		return nullptr;
	}
	if (!model)
	{
		gEngfuncs.Con_DPrintf("efx.CL_TempEntAlloc: No model\n");
		return nullptr;
	}

	TEMPENTITY* tempent = new TEMPENTITY{};

	memset(&tempent->entity, 0, sizeof(cl_entity_t));
	tempent->flags = 0;
	tempent->die = engine_cl->time + 0.75;
	tempent->entity.model = model;
	tempent->fadeSpeed = 0.5;
	tempent->hitSound = 0;
	tempent->clientIndex = -1;
	tempent->bounceFactor = 1.0;
	tempent->hitcallback = 0;
	tempent->callback = 0;
	tempent->priority = 0;
	tempent->entity.origin = org;

	gpTempEnts.push_back(tempent);

	return tempent;
	//return OrigCL_TempEntAlloc(org, model);
}
FuncHook(CL_TempEntAllocNoModel, TEMPENTITY*, float* org)
{
	if (gpTempEnts.size() > MAX_TEMPENTS)
	{
		gEngfuncs.Con_DPrintf("Overflow %d temporary ents!\n", 1200);
		return nullptr;
	}

	TEMPENTITY* tempent = new TEMPENTITY{};

	memset(&tempent->entity, 0, sizeof(cl_entity_t));
	tempent->flags |= FTENT_NOMODEL;
	tempent->die = engine_cl->time + 0.75;
	tempent->entity.model = nullptr;
	tempent->fadeSpeed = 0.5;
	tempent->hitSound = 0;
	tempent->clientIndex = -1;
	tempent->bounceFactor = 1.0;
	tempent->hitcallback = 0;
	tempent->callback = 0;
	tempent->priority = 0;
	tempent->entity.origin = org;

	gpTempEnts.push_back(tempent);

	return tempent;
}
FuncHook(CL_TempEntAllocHigh, TEMPENTITY*, float* org, struct model_s* model)
{
	if (gpTempEnts.size() > MAX_TEMPENTS)
	{
		gEngfuncs.Con_DPrintf("Overflow %d temporary ents!\n", 1200);
		return nullptr;
	}

	TEMPENTITY* tempent = new TEMPENTITY{};

	memset(&tempent->entity, 0, sizeof(cl_entity_t));
	tempent->flags |= FTENT_NOMODEL;
	tempent->die = engine_cl->time + 0.75;
	tempent->entity.model = nullptr;
	tempent->fadeSpeed = 0.5;
	tempent->hitSound = 0;
	tempent->clientIndex = -1;
	tempent->bounceFactor = 1.0;
	tempent->hitcallback = 0;
	tempent->callback = 0;
	tempent->priority = 0;
	tempent->entity.origin = org;

	gpTempEnts.push_back(tempent);

	return tempent;
}
FuncHook(CL_TentEntAllocCustom, TEMPENTITY*, float* origin, struct model_s* model, int high, void (*callback)(struct tempent_s* ent, float frametime, float currenttime))
{
	TEMPENTITY* customtent;
	if (high)
		customtent = Hooked_CL_TempEntAllocHigh(origin, model);
	else
		customtent = Hooked_CL_TempEntAlloc(origin, model);
	if (customtent)
	{
		customtent->flags |= FTENT_CLIENTCUSTOM;
		customtent->callback = callback;
		customtent->die = engine_cl->time;
	}
	return customtent;
}


FuncHook(R_AllocParticle, particle_t*, void (*callback)(struct particle_s* particle, float frametime))
{
	return OrigR_AllocParticle(callback);
}

FuncHook(R_BlobExplosion, void, float* org)
{
	OrigR_BlobExplosion(org);
}

FuncHook(R_Blood, void, float* org, float* dir, int pcolor, int speed)
{
	OrigR_Blood(org, dir, pcolor, speed);
}
FuncHook(R_BloodSprite, void, float* org, int colorindex, int modelIndex, int modelIndex2, float size)
{
	OrigR_BloodSprite(org, colorindex, modelIndex, modelIndex2, size);
}
FuncHook(R_BloodStream, void, float* org, float* dir, int pcolor, int speed)
{
	OrigR_BloodStream(org, dir, pcolor, speed);
}

#define SHARD_VOLUME		12.0f	// on shard ever n^3 units

FuncHook(R_BreakModel, void, float* pos, float* size, float* dir, float random, float life, int count, int modelIndex, char flags)
{
	if (!modelIndex)
		return;

	model_t* modelbyindex = IEngineStudio.GetModelByIndex(modelIndex);
	if (!modelbyindex)
		return;

	auto RandomLong = gEngfuncs.pfnRandomLong;
	auto RandomFloat = gEngfuncs.pfnRandomFloat;

	int framecount = ModelFrameCount(modelbyindex);

	if(count == 0)
	{
		count = (size[0] * size[1] + size[1] * size[2] + size[2] * size[0]) / (3 * SHARD_VOLUME * SHARD_VOLUME);
	}

	for (int i = 0; i < count; i++)
	{
		Vector vecSpot = pos;

		TEMPENTITY* pTemp = Hooked_CL_TempEntAlloc(vecSpot, modelbyindex);
		if (!pTemp)
			return;

		// keep track of break_type, so we know how to play sound on collision
		pTemp->hitSound = flags & BREAK_TYPEMASK;

		if (modelbyindex->type == mod_sprite)
			pTemp->entity.curstate.frame = RandomLong(0, pTemp->frameMax);
		else if (modelbyindex->type == mod_studio)
			pTemp->entity.curstate.body = RandomLong(0, pTemp->frameMax);

		pTemp->flags |= FTENT_COLLIDEWORLD | FTENT_FADEOUT | FTENT_SLOWGRAVITY;

		if (RandomLong(0, 255) < 200)
		{
			pTemp->flags |= FTENT_ROTATE;
			pTemp->entity.baseline.angles[0] = RandomFloat(-256, 255);
			pTemp->entity.baseline.angles[1] = RandomFloat(-256, 255);
			pTemp->entity.baseline.angles[2] = RandomFloat(-256, 255);
		}

		if ((RandomLong(0, 255) < 100) && flags & BREAK_SMOKE)
			pTemp->flags |= FTENT_SMOKETRAIL;

		if ((flags & BREAK_TYPEMASK == BREAK_GLASS) || flags & BREAK_TRANS)
		{
			pTemp->entity.curstate.rendermode = kRenderTransTexture;
			pTemp->entity.curstate.renderamt = pTemp->entity.baseline.renderamt = 128;
		}
		else
		{
			pTemp->entity.curstate.rendermode = kRenderNormal;
			pTemp->entity.curstate.renderamt = pTemp->entity.baseline.renderamt = 255; // set this for fadeout
		}

		pTemp->entity.baseline.origin[0] = dir[0] + RandomFloat(-random, random);
		pTemp->entity.baseline.origin[1] = dir[1] + RandomFloat(-random, random);
		pTemp->entity.baseline.origin[2] = dir[2] + RandomFloat(0, random);

		pTemp->die = engine_cl->time + life + RandomFloat(0.0f, 1.0f); // Add an extra 0-1 secs of life
	}
}

FuncHook(R_Bubbles, void, float* mins, float* maxs, float height, int modelIndex, int count, float speed)
{
	if (!modelIndex)
		return;

	model_t* modelbyindex = IEngineStudio.GetModelByIndex(modelIndex);
	if (!modelbyindex)
		return;

	auto RandomLong = gEngfuncs.pfnRandomLong;
	auto RandomFloat = gEngfuncs.pfnRandomFloat;

	int framecount = ModelFrameCount(modelbyindex);
	for (int i = 0; i < count; i++)
	{
		Vector origin;

		TEMPENTITY* bubble = Hooked_CL_TempEntAlloc(origin, modelbyindex);
		if (!bubble)
			return;

		origin[0] = RandomLong(mins[0], maxs[0]);
		origin[1] = RandomLong(mins[1], maxs[1]);
		origin[2] = RandomLong(mins[2], maxs[2]);
		TEMPENTITY* pTemp = Hooked_CL_TempEntAlloc(origin, modelbyindex);
		if (!pTemp)
			return;

		pTemp->flags |= FTENT_SINEWAVE;

		pTemp->x = origin[0];
		pTemp->y = origin[1];
		float angle = RandomFloat(-M_PI, M_PI);
		float sine, cosine;
		SinCos_(angle, &sine, &cosine);

		float zspeed = RandomLong(80, 140);
		pTemp->entity.baseline.origin = Vector(speed * cosine, speed * sine, zspeed);
		pTemp->die = engine_cl->time + ((height - (origin[2] - mins[2])) / zspeed) - 0.1f;
		pTemp->entity.curstate.frame = RandomLong(0, pTemp->frameMax);

		// Set sprite scale
		pTemp->entity.curstate.scale = 1.0f / RandomFloat(2.0f, 5.0f);
		pTemp->entity.curstate.rendermode = kRenderTransAlpha;
		pTemp->entity.curstate.renderamt = 255;
	}
}
FuncHook(R_BubbleTrail, void, float* start, float* end, float height, int modelIndex, int count, float speed)
{
	if (!modelIndex)
		return;

	model_t* modelbyindex = IEngineStudio.GetModelByIndex(modelIndex);
	if (!modelbyindex)
		return;

	auto RandomLong = gEngfuncs.pfnRandomLong;
	auto RandomFloat = gEngfuncs.pfnRandomFloat;

	int framecount = ModelFrameCount(modelbyindex);
	for (int i = 0; i < count; i++)
	{
		Vector start_ = start;
		Vector end_ = end;
		float seed = RandomFloat(0, 1);
		Vector _origin = (end_ - start_) * seed + start_;
		TEMPENTITY* bubble = Hooked_CL_TempEntAlloc(_origin, modelbyindex);
		if (!bubble)
			return;

		bubble->flags |= FTENT_SINEWAVE;
		bubble->x = _origin.x;
		bubble->y = _origin.y;

		int seed2 = RandomLong(-3, 3);
		float sine, cosine;
		SinCos_(seed2, &sine, &cosine);
		bubble->entity.baseline.origin.x = sine * speed;
		bubble->entity.baseline.origin.y = cosine * speed;
		int bubbletravel = RandomLong(80, 140);
		bubble->entity.baseline.origin.z = bubbletravel;
		bubble->die = (height - (_origin.z - start_.z)) / bubbletravel + engine_cl->time - 0.1;
		bubble->entity.curstate.frame = RandomLong(0, framecount - 1);
		bubble->entity.curstate.rendermode = kRenderTransAlpha;
		bubble->entity.curstate.renderamt = 255;
		bubble->entity.curstate.scale = 1.0 / RandomFloat(2, 5);
	}
}
FuncHook(R_BulletImpactParticles, void, float* pos)
{
	OrigR_BulletImpactParticles(pos);
}
FuncHook(R_EntityParticles, void, struct cl_entity_s* ent)
{
	OrigR_EntityParticles(ent);
}
FuncHook(R_Explosion, void, float* pos, int model, float scale, float framerate, int flags)
{
	//dont even bother its not used at all
	//if (scale)
	//{
	//	TEMPENTITY* sprite = Hooked_R_DefaultSprite(pos, model, framerate);
	//	Hooked_R_Sprite_Explode(sprite, scale, flags);
	//	if (flags & TE_EXPLOSION)
	//	{
	//
	//	}
	//}
}
FuncHook(R_FizzEffect, void, struct cl_entity_s* pent, int modelIndex, int density)
{
	OrigR_FizzEffect(pent, modelIndex, density);
}
FuncHook(R_FireField, void, float* org, int radius, int modelIndex, int count, int flags, float life)
{
	OrigR_FireField(org, radius, modelIndex, count, flags, life);
}
FuncHook(R_FlickerParticles, void, float* org)
{
	OrigR_FlickerParticles(org);
}
FuncHook(R_FunnelSprite, void, float* org, int modelIndex, int reverse)
{
	OrigR_FunnelSprite(org, modelIndex, reverse);
}
FuncHook(R_Implosion, void, float* end, float radius, int count, float life)
{
	OrigR_Implosion(end, radius, count, life);
}
FuncHook(R_LargeFunnel, void, float* org, int reverse)
{
	OrigR_LargeFunnel(org, reverse);
}
FuncHook(R_LavaSplash, void, float* org)
{
	OrigR_LavaSplash(org);
}
FuncHook(R_MultiGunshot, void, float* org, float* dir, float* noise, int count, int decalCount, int* decalIndices)
{
	OrigR_MultiGunshot(org, dir, noise, count, decalCount, decalIndices);
}
FuncHook(R_MuzzleFlash, void, float* pos1, int type)
{
	TEMPENTITY* pTemp;
	int index;
	float scale;
	
	index = (type % 10) % 3;
	scale = (type / 10) * 0.1f;
	if (scale == 0.0f)
	scale = 0.5f;
	
	if (!cl_sprite_muzzleflash[index])
		return;
	
	// must set position for right culling on render
	pTemp = Hooked_CL_TempEntAlloc(pos1, cl_sprite_muzzleflash[index]);
	if (!pTemp)
	return;
	pTemp->entity.curstate.rendermode = kRenderTransAdd;
	pTemp->entity.curstate.renderamt = 255;
	pTemp->entity.curstate.framerate = 10;
	pTemp->entity.curstate.renderfx = 0;
	pTemp->die = engine_cl->time + (engine_cl->time - engine_cl->oldtime) + 0.02; // die at next frame
	pTemp->entity.curstate.frame = gEngfuncs.pfnRandomLong(0, pTemp->frameMax);
	pTemp->flags |= FTENT_SPRANIMATE | FTENT_SPRANIMATELOOP;
	pTemp->entity.curstate.scale = scale;
	
	if (index == 0)
	pTemp->entity.angles[2] = gEngfuncs.pfnRandomLong(0, 20); // rifle flash
	else
	pTemp->entity.angles[2] = gEngfuncs.pfnRandomLong(0, 359);
	
	CL_Internal_AddEntity(ET_TEMPENTITY, &pTemp->entity);

}

FuncHook(R_ParticleBox, void, float* mins, float* maxs, unsigned char r, unsigned char g, unsigned char b, float life)
{
	//OrigR_ParticleBox(mins, maxs, r, g, b, life);
}
FuncHook(R_ParticleBurst, void, float* pos, int size, int color, float life)
{
	//OrigR_ParticleBurst(pos, size, color, life);
}
FuncHook(R_ParticleExplosion, void, float* org)
{
	//OrigR_ParticleExplosion(org);
}
FuncHook(R_ParticleExplosion2, void, float* org, int colorStart, int colorLength)
{
	//OrigR_ParticleExplosion2(org, colorStart, colorLength);
}
FuncHook(R_ParticleLine, void, float* start, float* end, unsigned char r, unsigned char g, unsigned char b, float life)
{
	//OrigR_ParticleLine(start, end, r, g, b, life);
}
FuncHook(R_PlayerSprites, void, int client, int modelIndex, int count, int size)
{
	if (client <= 0 || client >= gEngfuncs.GetMaxClients())
	{
		gEngfuncs.Con_Printf("Bad ent in R_PlayerSprites!\n");
		return;
	}

	cl_entity_s* player = gEngfuncs.GetEntityByIndex(client);
	model_t* modelbyindex = IEngineStudio.GetModelByIndex(modelIndex);
	if (!modelbyindex)
	{
		gEngfuncs.Con_Printf("No model %d!\n", modelIndex);
		return;
	}

	Vector position;

	int framecount = ModelFrameCount(modelbyindex);

	auto RandomFloat = gEngfuncs.pfnRandomFloat;

	for (int i = 0; i < count; i++)
	{
		VectorCopy(player->origin, position);
		position.x += RandomFloat(-10.0f, 10.0f);
		position.y += RandomFloat(-10.0f, 10.0f);
		position.z += RandomFloat(-20.0f, 36.0f);

		TEMPENTITY* playersprite = Hooked_CL_TempEntAlloc(position, modelbyindex);
		if (!playersprite)
			return;

		playersprite->tentOffset = playersprite->entity.origin - player->origin;
		if (i > 0)
		{
			playersprite->flags |= FTENT_PLYRATTACHMENT;
			playersprite->clientIndex = client;
		}
		else
		{
			Vector dir = (position - player->origin).Normalize() * 60;
			dir.z = RandomFloat(20, 60);
			playersprite->entity.baseline.origin = dir;
		}

		playersprite->entity.curstate.rendermode = kRenderNormal;
		playersprite->entity.curstate.renderamt = 255;
		playersprite->entity.baseline.renderamt = 255;
		playersprite->entity.curstate.renderfx = kRenderFxNoDissipation;

		playersprite->entity.curstate.framerate = RandomFloat(1.0f - (size / 100.0f), 1.0f);
		if (framecount > 1)
		{
			playersprite->flags |= FTENT_SPRANIMATE;
			playersprite->entity.curstate.framerate = 20.0f;
			playersprite->die = engine_cl->time + (framecount * 0.05f);
		}
		else
		{
			playersprite->die = engine_cl->time + 0.35f;
		}

		playersprite->entity.curstate.frame = 0;
	}

}
FuncHook(R_Projectile, void, float* origin, float* velocity, int modelIndex, int life, int owner, void (*hitcallback)(struct tempent_s* ent, struct pmtrace_s* ptr))
{
	model_t* modelbyindex = IEngineStudio.GetModelByIndex(modelIndex);
	if (!modelbyindex)
	{
		gEngfuncs.Con_Printf("Couldn't allocate temp ent in R_Projectile!\n");
		return;
	}
	TEMPENTITY* projectile = Hooked_CL_TempEntAllocHigh(origin, modelbyindex);
	if (!projectile)
		return;

	if (owner < 0 || owner > gEngfuncs.GetMaxClients())
	{
		gEngfuncs.Con_Printf("Bad ent in R_Projectile!\n");
		return;
	}

	projectile->flags |= FTENT_COLLIDEALL | FTENT_PERSIST | FTENT_COLLIDEKILL;
	projectile->entity.baseline.origin = velocity;
	projectile->clientIndex = owner;
	if (projectile->entity.model->type = mod_sprite)
	{
		int framecount = ModelFrameCount(modelbyindex);
		projectile->flags |= FTENT_SPRANIMATE;
		projectile->frameMax = framecount;
		if (framecount > 9)
		{
			projectile->entity.curstate.framerate = framecount / life;
		}
		else
		{
			projectile->entity.curstate.framerate = 10.0;
			projectile->flags |= FTENT_SPRANIMATELOOP;
		}
	}
	else
	{
		projectile->frameMax = 0;
		velocity = Vector(velocity[0], velocity[1], velocity[2]).Normalize();
		Vector orientation = Vector(0, 0, 0);
		VectorAngles(Vector(velocity[0], velocity[1], velocity[2]), orientation);
		projectile->entity.angles = orientation;
	}

	projectile->entity.curstate.rendermode = kRenderNormal;
	projectile->entity.curstate.renderfx = kRenderFxNone;
	projectile->entity.curstate.renderamt = 255;
	projectile->entity.baseline.renderamt = 255;
	projectile->die = engine_cl->time + life;
	projectile->entity.curstate.frame = 0.0;
	projectile->entity.curstate.body = 0;
	projectile->entity.curstate.scale = 1.0;
	projectile->entity.curstate.rendercolor.r = -1;
	projectile->entity.curstate.rendercolor.g = -1;
	projectile->entity.curstate.rendercolor.b = -1;
	projectile->hitcallback = hitcallback;

}
FuncHook(R_RicochetSound, void, float* pos)
{
	OrigR_RicochetSound(pos);
}
FuncHook(R_RicochetSprite, void, float* pos, struct model_s* pmodel, float duration, float scale)
{
	TEMPENTITY* ricochet = Hooked_CL_TempEntAlloc(pos, pmodel);
	if (!ricochet)
		return;

	ricochet->entity.curstate.rendermode = kRenderGlow;
	ricochet->entity.curstate.scale = scale;
	ricochet->entity.curstate.renderfx = 14;
	ricochet->entity.curstate.renderamt = 200;
	ricochet->entity.baseline.renderamt = 200;
	ricochet->flags |= FTENT_FADEOUT;
	ricochet->entity.baseline.origin = Vector(0, 0, 0);
	ricochet->entity.origin = Vector(pos[0], pos[1], pos[2]);
	ricochet->fadeSpeed = 8.0;
	ricochet->die = engine_cl->time;
	ricochet->entity.curstate.frame = 0;
	ricochet->entity.angles.z = 45 * gEngfuncs.pfnRandomLong(0, 7);

}
FuncHook(R_RocketFlare, void, float* pos)
{
	//if (engine_cl->time == engine_cl->oldtime)
	//	return;
	//
	//int framecount = ModelFrameCount(cl_sprite_glow);
	//TEMPENTITY* flare = Hooked_CL_TempEntAlloc(pos, cl_sprite_glow);
	//if (!flare)
	//	return;
	//
	//flare->frameMax = framecount;
	//flare->entity.curstate.rendermode = kRenderGlow;
	//flare->entity.curstate.renderamt = 255;
	//flare->entity.curstate.renderfx = kRenderFxNoDissipation;
	//flare->entity.curstate.framerate = 1.0;
	//flare->entity.origin = pos;
	//flare->die = engine_cl->time + 0.01;
	//flare->entity.curstate.frame = gEngfuncs.pfnRandomLong(0, framecount - 1);
}
FuncHook(R_RocketTrail, void, float* start, float* end, int type)
{
	//OrigR_RocketTrail(start, end, type);
}
FuncHook(R_RunParticleEffect, void, float* org, float* dir, int color, int count)
{
	//OrigR_RunParticleEffect(org, dir, color, count);
}
FuncHook(R_ShowLine, void, float* start, float* end)
{
	//OrigR_ShowLine(start, end);
}

FuncHook(R_SparkStreaks, void, float* pos, int count, int velocityMin, int velocityMax)
{
	//OrigR_SparkStreaks(pos, count, velocityMin, velocityMax);
}

FuncHook(R_SparkEffect, void, float* pos, int count, int velocityMin, int velocityMax)
{
	// OrigR_SparkEffect(pos, count, velocityMin, velocityMax);
	Hooked_R_SparkStreaks(pos, count, velocityMin, velocityMax);
	Hooked_R_RicochetSprite(pos, cl_sprite_ricochet, 0.1, gEngfuncs.pfnRandomFloat(0.5, 1.0));
}
FuncHook(R_SparkShower, void, float* pos)
{
	TEMPENTITY* spark = Hooked_CL_TempEntAllocNoModel(pos);
	if (!spark)
		return;

	spark->entity.baseline.origin.x = gEngfuncs.pfnRandomFloat(-300, 300);
	spark->entity.baseline.origin.y = gEngfuncs.pfnRandomFloat(-300, 300);
	spark->entity.baseline.origin.z = gEngfuncs.pfnRandomFloat(-200, 200);
	spark->flags |= FTENT_SLOWGRAVITY | FTENT_COLLIDEWORLD | FTENT_SPARKSHOWER;
	spark->entity.baseline.angles = Vector(0, 0, 0);
	spark->die = engine_cl->time + 0.5;
	spark->entity.curstate.framerate = gEngfuncs.pfnRandomFloat(0.5, 1.5);
	spark->entity.curstate.frame = engine_cl->time;
}

//called by engine in CL_ParseTEnt on case TE_SPRAY
FuncHook(R_Spray, void, float* pos, float* dir, int modelIndex, int count, int speed, int spread, int rendermode)
{
	model_t* modelbyindex = IEngineStudio.GetModelByIndex(modelIndex);
	if (!modelbyindex)
	{
		gEngfuncs.Con_Printf("No model %d!\n", modelIndex);
		return;
	}

	int framecount = ModelFrameCount(modelbyindex) - 1;
	if (count <= 0)
		return;

	float noise = spread / 100.0;
	float znoise = (spread / 100.0) * 1.5;
	if (znoise > 1.0)
		znoise = 1.0;

	for (int i = 0; i < count; i++)
	{
		TEMPENTITY* spray = Hooked_CL_TempEntAlloc(pos, modelbyindex);
		if (!spray)
			break;

		spray->entity.curstate.renderfx = kRenderFxNoDissipation;
		spray->entity.baseline.renderamt = 255;
		spray->entity.curstate.framerate = 1.0;
		spray->entity.curstate.rendermode = rendermode;
		spray->entity.curstate.renderamt = 255;
		spray->flags |= FTENT_COLLIDEWORLD | FTENT_SLOWGRAVITY;
		if (framecount > 1)
		{
			spray->flags |= FTENT_SPRANIMATE;
			spray->entity.curstate.framerate = 10.0;
			spray->die = framecount * 0.1 + engine_cl->time;
		}
		else
			spray->die = engine_cl->time + 0.35;

		auto RandomFloat = gEngfuncs.pfnRandomFloat;

		spray->entity.baseline.origin = Vector(RandomFloat(-noise, noise) + dir[0], RandomFloat(-noise, noise) + dir[1], RandomFloat(0, znoise) + dir[2]);

		float flHigh = RandomFloat(speed * 0.8, speed * 1.2);
		VectorScale(spray->entity.baseline.origin, flHigh, spray->entity.baseline.origin);
		spray->entity.origin = pos;
		spray->entity.curstate.frame = 0;
		spray->frameMax = framecount;

	}

}
FuncHook(R_Sprite_Explode, void, TEMPENTITY* pTemp, float scale, int flags)
{
	if (!pTemp)
		return;

	pTemp->entity.curstate.scale = scale;
	pTemp->entity.curstate.renderamt = (flags & TE_EXPLFLAG_NOADDITIVE) ? 255 : 180;
	pTemp->entity.curstate.rendermode = (flags & TE_EXPLFLAG_NOADDITIVE) ? kRenderTransAdd : kRenderNormal;
	pTemp->entity.origin.z += 10.0;
	pTemp->entity.curstate.renderfx = kRenderFxNone;
	pTemp->entity.baseline.origin.z = 8.0;
	pTemp->entity.curstate.rendercolor.r = 0;
	pTemp->entity.curstate.rendercolor.g = 0;
	pTemp->entity.curstate.rendercolor.b = 0;
}
FuncHook(R_Sprite_Smoke, void, TEMPENTITY* pTemp, float scale)
{
	if (!pTemp)
		return;

	pTemp->entity.curstate.rendermode = kRenderTransAlpha;
	pTemp->entity.curstate.renderamt = 255;
	pTemp->entity.curstate.renderfx = kRenderFxNone;
	pTemp->entity.baseline.origin.z = 30.0;
	pTemp->entity.curstate.scale = scale;
	pTemp->entity.curstate.rendercolor.r = 0;
	pTemp->entity.curstate.rendercolor.g = 0;
	pTemp->entity.curstate.rendercolor.b = 0;
	pTemp->entity.origin.z = gEngfuncs.pfnRandomLong(20, 35);
}
FuncHook(R_Sprite_Spray, void, float* pos, float* dir, int modelIndex, int count, int speed, int iRand)
{
	OrigR_Sprite_Spray(pos, dir, modelIndex, count, speed, iRand);
}
FuncHook(R_Sprite_Trail, void, int type, float* start, float* end, int modelIndex, int count, float life, float size, float amplitude, int renderamt, float speed)
{
	OrigR_Sprite_Trail(type, start, end, modelIndex, count, life, size, amplitude, renderamt, speed);
}
FuncHook(R_Sprite_WallPuff, void, TEMPENTITY* pTemp, float scale)
{
	if (!pTemp)
		return;

	pTemp->entity.curstate.rendermode = kRenderTransAlpha;
	pTemp->entity.curstate.scale = scale;
	pTemp->entity.curstate.renderamt = 255;
	pTemp->entity.curstate.renderfx = kRenderFxNone;
	pTemp->entity.curstate.rendercolor.r = 0;
	pTemp->entity.curstate.rendercolor.g = 0;
	pTemp->entity.curstate.rendercolor.b = 0;
	pTemp->die = engine_cl->time + 0.01;
	pTemp->entity.curstate.frame = 0.0;
	pTemp->entity.angles[2] = gEngfuncs.pfnRandomLong(0, 359);
}
FuncHook(R_StreakSplash, void, float* pos, float* dir, int color, int count, float speed, int velocityMin, int velocityMax)
{
	OrigR_StreakSplash(pos, dir, color, count, speed, velocityMin, velocityMax);
}
FuncHook(R_TracerEffect, void, float* start, float* end)
{
	OrigR_TracerEffect(start, end);
}
FuncHook(R_UserTracerParticle, void, float* org, float* vel, float life, int colorIndex, float length, unsigned char deathcontext, void (*deathfunc)(struct particle_s* particle))
{
	OrigR_UserTracerParticle(org, vel, life, colorIndex, length, deathcontext, deathfunc);
}
FuncHook(R_TracerParticles, particle_t*, float* org, float* vel, float life)
{
	return OrigR_TracerParticles(org, vel, life);
}
FuncHook(R_TeleportSplash, void, float* org)
{
	OrigR_TeleportSplash(org);
}
FuncHook(R_TempSphereModel, void, float* pos, float speed, float life, int count, int modelIndex)
{
	if (!modelIndex)
		return;

	model_t* spheremodel = IEngineStudio.GetModelByIndex(modelIndex);
	if (!spheremodel)
		return;

	int frameCount;
	frameCount = ModelFrameCount(spheremodel);

	TEMPENTITY* pTemp;

	for (int i = 0; i < count; i++)
	{
		pTemp = Hooked_CL_TempEntAlloc(pos, spheremodel);

		if (!pTemp)
			return;

		pTemp->entity.curstate.body = gEngfuncs.pfnRandomLong(0, frameCount - 1);

		if (gEngfuncs.pfnRandomLong(0, 255) < 10)
			pTemp->flags |= FTENT_SLOWGRAVITY;
		else
			pTemp->flags |= FTENT_GRAVITY;

		if (gEngfuncs.pfnRandomLong(0, 255) <= 219)
		{
			pTemp->flags |= FTENT_ROTATE;

			pTemp->entity.baseline.angles.x = gEngfuncs.pfnRandomFloat(-256.0, -255.0);
			pTemp->entity.baseline.angles.y = gEngfuncs.pfnRandomFloat(-256.0, -255.0);
			pTemp->entity.baseline.angles.z = gEngfuncs.pfnRandomFloat(-256.0, -255.0);
		}

		if (gEngfuncs.pfnRandomLong(0, 255) < 100)
		{
			pTemp->flags |= FTENT_SMOKETRAIL;
		}

		pTemp->flags |= FTENT_FLICKER | FTENT_COLLIDEWORLD;
		pTemp->entity.curstate.rendermode = kRenderNormal;
		pTemp->entity.curstate.effects = i & 31;

		pTemp->entity.baseline.origin.x = gEngfuncs.pfnRandomFloat(-1.0, 1.0);
		pTemp->entity.baseline.origin.y = gEngfuncs.pfnRandomFloat(-1.0, 1.0);
		pTemp->entity.baseline.origin.z = gEngfuncs.pfnRandomFloat(-1.0, 1.0);

		VectorNormalize(pTemp->entity.baseline.origin);

		VectorScale(pTemp->entity.baseline.origin, speed, pTemp->entity.baseline.origin);

		pTemp->die = engine_cl->time + life;
	}
}
FuncHook(R_TempModel, TEMPENTITY*, float* pos, float* dir, float* angles, float life, int modelIndex, int soundtype)
{
	if (!modelIndex)
		return nullptr;

	model_t* model = IEngineStudio.GetModelByIndex(modelIndex);
	if (!model)
	{
		gEngfuncs.Con_Printf("No model %d!\n", modelIndex);
		return nullptr;
	}

	int framecount = ModelFrameCount(model);
	TEMPENTITY* tempent = Hooked_CL_TempEntAlloc(pos, model);
	if (!tempent)
		return nullptr;

	tempent->entity.angles = angles;
	tempent->flags |= FTENT_COLLIDEWORLD | FTENT_GRAVITY;
	tempent->frameMax = framecount;

	auto RandomFloat = gEngfuncs.pfnRandomFloat;
	auto RandomLong = gEngfuncs.pfnRandomLong;

	switch (soundtype)
	{
	case 0:
		tempent->hitSound = 0;
		break;
	case BREAK_GLASS:
		tempent->hitSound = BREAK_TRANS;
		tempent->flags |= FTENT_ROTATE;
		tempent->entity.baseline.angles = Vector(RandomFloat(-512, 511), RandomFloat(-256, 255), RandomFloat(-256, 255));
		break;
	case BREAK_METAL:
		tempent->hitSound = BREAK_2;
		tempent->flags |= FTENT_SMOKETRAIL;
		tempent->entity.baseline.angles = Vector(RandomFloat(-512, 511), RandomFloat(-256, 255), RandomFloat(-256, 255));
		break;
	}

	tempent->entity.baseline.origin = dir;
	tempent->die = life + engine_cl->time;
	if (model->type == mod_sprite)
	{
		tempent->entity.curstate.frame = RandomLong(0, framecount - 1);
	}
	else
	{
		tempent->entity.curstate.body = RandomLong(0, framecount - 1);
	}

	return tempent;

}
FuncHook(R_DefaultSprite, TEMPENTITY*, float* pos, int spriteIndex, float framerate)
{
	if (engine_cl->time == engine_cl->oldtime)
		return 0;

	model_t* modelbyindex = IEngineStudio.GetModelByIndex(spriteIndex);
	if (!spriteIndex || !modelbyindex || modelbyindex->type != mod_sprite)
	{
		gEngfuncs.Con_DPrintf("No Sprite %d!\n", spriteIndex);
		return 0;
	}

	int framecount = ModelFrameCount(modelbyindex);
	TEMPENTITY* sprite = Hooked_CL_TempEntAlloc(pos, modelbyindex);
	if (!sprite)
		return 0;

	sprite->flags |= FTENT_SPRANIMATE;
	sprite->frameMax = framecount;
	sprite->entity.curstate.framerate = framerate ? framerate : 10.0f;
	sprite->die = framecount / sprite->entity.curstate.framerate + engine_cl->time;
	sprite->entity.curstate.frame = 0;
	sprite->entity.origin = pos;
	return sprite;
}
FuncHook(R_TempSprite, TEMPENTITY*, float* pos, const float* dir, float scale, int modelIndex, int rendermode, int renderfx, float a, float life, int flags)
{
	//return OrigR_TempSprite(pos, dir, scale, modelIndex, rendermode, renderfx, a, life, flags);

	TEMPENTITY* pTemp;
	model_t* pmodel;

	if ((pmodel = IEngineStudio.GetModelByIndex(modelIndex)) == NULL)
	{
		gEngfuncs.Con_Printf("No model %d!\n", modelIndex);
		return NULL;
	}

	pTemp = Hooked_CL_TempEntAlloc(pos, pmodel);
	if (!pTemp)
		return NULL;

	int framecount = ModelFrameCount(pmodel);
	pTemp->frameMax = framecount;

	pTemp->entity.curstate.framerate = 10;
	pTemp->entity.curstate.rendermode = rendermode;
	pTemp->entity.curstate.renderfx = renderfx;
	pTemp->entity.curstate.scale = scale;
	pTemp->entity.baseline.renderamt = a * 255;
	pTemp->entity.curstate.renderamt = a * 255;
	pTemp->flags |= flags;

	VectorCopy(dir, pTemp->entity.baseline.origin);

	if (life)
		pTemp->die = engine_cl->time + life;
	else
		pTemp->die = engine_cl->time + (pTemp->frameMax * 0.1f) + 4.0f;
	pTemp->entity.curstate.frame = 0;

	return pTemp;
}
FuncHook(Draw_DecalIndex, int, int id)
{
	return OrigDraw_DecalIndex(id);
}
FuncHook(Draw_DecalIndexFromName, int, char* name)
{
	return OrigDraw_DecalIndexFromName(name);
}
FuncHook(R_DecalShoot, void, int textureIndex, int entity, int modelIndex, float* position, int flags)
{
	//OrigR_DecalShoot(textureIndex, entity, modelIndex, position, flags);
	//no decals in trinity buddy
}
FuncHook(R_AttachTentToPlayer, void, int client, int modelIndex, float zoffset, float life)
{
	TEMPENTITY* pTemp;
	Vector position;
	cl_entity_t* pClient;
	model_t* pModel;

	if (client <= 0 || client > engine_cl->maxclients)
		return;

	pClient = gEngfuncs.GetEntityByIndex(client);

	if (!pClient || pClient->curstate.messagenum != engine_cl->parsecount)
		return;

	if ((pModel = IEngineStudio.GetModelByIndex(modelIndex)) == NULL)
		return;

	VectorCopy(pClient->origin, position);
	position[2] += zoffset;

	pTemp = Hooked_CL_TempEntAllocHigh(position, pModel);
	if (!pTemp)
		return;

	pTemp->entity.curstate.renderfx = kRenderFxNoDissipation;
	pTemp->entity.curstate.framerate = 1;

	pTemp->clientIndex = client;
	pTemp->tentOffset[0] = 0;
	pTemp->tentOffset[1] = 0;
	pTemp->tentOffset[2] = zoffset;
	pTemp->die = engine_cl->time + life;
	pTemp->flags |= FTENT_PLYRATTACHMENT | FTENT_PERSIST;

	// is the model a sprite?
	if (pModel->type == mod_sprite)
	{
		pTemp->flags |= FTENT_SPRANIMATE | FTENT_SPRANIMATELOOP;
		pTemp->entity.curstate.framerate = 10;
	}
	else
	{
		// no animation support for attached clientside studio models.
		pTemp->frameMax = 0;
	}

	pTemp->entity.curstate.frame = 0;
}
FuncHook(R_KillAttachedTents, void, int client)
{
	if (client < 0 || client > engine_cl->maxclients)
	{
		gEngfuncs.Con_Printf("Bad client in R_KillAttachedTents()!\n");
		return;
	}

	for (auto tempent : gpTempEnts)
	{
		if (tempent->flags & FTENT_PLYRATTACHMENT)
		{
			if (client == tempent->clientIndex)
				tempent->die = engine_cl->time;
		}
	}
}
FuncHook(R_BeamCirclePoints, BEAM*, int type, float* start, float* end, int modelIndex, float life, float width, float amplitude, float brightness, float speed, int startFrame, float framerate, float r, float g, float b)
{
	return nullptr;
}
FuncHook(R_BeamEntPoint, BEAM*, int startEnt, float* end, int modelIndex, float life, float width, float amplitude, float brightness, float speed, int startFrame, float framerate, float r, float g, float b)
{
	return nullptr;
}
FuncHook(R_BeamEnts, BEAM*, int startEnt, int endEnt, int modelIndex, float life, float width, float amplitude, float brightness, float speed, int startFrame, float framerate, float r, float g, float b)
{
	return nullptr;
}
FuncHook(R_BeamFollow, BEAM*, int startEnt, int modelIndex, float life, float width, float r, float g, float b, float brightness)
{
	return nullptr;
}
FuncHook(R_BeamKill, void, int deadEntity)
{
	return;
}
FuncHook(R_BeamLightning, BEAM*, float* start, float* end, int modelIndex, float life, float width, float amplitude, float brightness, float speed)
{
	return nullptr;
}
FuncHook(R_BeamPoints, BEAM*, float* start, float* end, int modelIndex, float life, float width, float amplitude, float brightness, float speed, int startFrame, float framerate, float r, float g, float b)
{
	return nullptr;
}
FuncHook(R_BeamRing, BEAM*, int startEnt, int endEnt, int modelIndex, float life, float width, float amplitude, float brightness, float speed, int startFrame, float framerate, float r, float g, float b)
{
	return nullptr;
}
FuncHook(CL_AllocDlight, dlight_t*, int key)
{
	return OrigCL_AllocDlight(key);
}
FuncHook(CL_AllocElight, dlight_t*, int key)
{
	return OrigCL_AllocElight(key);
}
FuncHook(R_GetPackedColor, void, short* packed, short color)
{
	OrigR_GetPackedColor(packed, color);
}
FuncHook(R_LookupColor, short, unsigned char r, unsigned char g, unsigned char b)
{
	return OrigR_LookupColor(r, g, b);
}
FuncHook(R_DecalRemoveAll, void, int textureIndex) // textureIndex points to the decal index in the array, not the actual texture index.
{
	OrigR_DecalRemoveAll(textureIndex);
}
FuncHook(R_FireCustomDecal, void, int textureIndex, int entity, int modelIndex, float* position, int flags, float scale)
{
	OrigR_FireCustomDecal(textureIndex, entity, modelIndex, position, flags, scale);
}

//////////////////////////////////////////
//  TRIAPI
// 
//
//////////////////////////////////////////

float gGlR, gGlG, gGlB, gGlW;
int gRenderMode;

FuncHook(RenderMode, void, int mode)
{
	switch (mode)
	{
	case kRenderNormal:
		glDisable(GL_BLEND);
		glDepthMask(GL_TRUE);
		glShadeModel(GL_FLAT);
		gRenderMode = mode;
		break;
	case kRenderTransColor:
	case kRenderTransTexture:
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		glShadeModel(GL_SMOOTH);
		gRenderMode = mode;
		break;
	case kRenderTransAlpha:
		glEnable(GL_BLEND);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glShadeModel(GL_SMOOTH);
		glDepthMask(GL_FALSE);
		gRenderMode = mode;
		break;
	case kRenderTransAdd:
		glBlendFunc(GL_ONE, GL_ONE);
		glEnable(GL_BLEND);
		glDepthMask(GL_FALSE);
		glShadeModel(GL_SMOOTH);
		gRenderMode = mode;
		break;
	default:
		gRenderMode = mode;
		break;
	}
}
FuncHook(Begin, void, int primitiveCode)
{
	//  VGUI2_ResetCurrentTexture();
	//  glBegin(g_GL_Modes[primitiveCode]);
	//
	OrigBegin(primitiveCode);
}
FuncHook(End, void, void)
{
	glEnd();
}

FuncHook(Color4f, void, float r, float g, float b, float a)
{
	if (gRenderMode == kRenderTransAlpha)
	{
		glColor4ub(r * 255.89999, g * 255.89999, b * 255.89999, a * 255);
	}
	else
	{
		glColor4f(a * r, a * g, a * b, 1.0);
	}
	gGlR = r;
	gGlB = b;
	gGlG = g;
	gGlW = a;
}
FuncHook(Color4ub, void, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	gGlR = float(r) / 255.f;
	gGlG = float(g) / 255.f;
	gGlB = float(b) / 255.f;
	gGlW = float(a) / 255.f;
	glColor4f(gGlR, gGlG, gGlB, gGlW);
}
FuncHook(TexCoord2f, void, float u, float v)
{
	glTexCoord2f(u, v);
}
FuncHook(Vertex3fv, void, float* worldPnt)
{
	glVertex3fv(worldPnt);
}
FuncHook(Vertex3f, void, float x, float y, float z)
{
	glVertex3f(x, y, z);
}
FuncHook(Brightness, void, float brightness)
{
	float r = brightness * gGlR * gGlW;
	float g = brightness * gGlG * gGlW;
	float b = brightness * gGlB * gGlW;
	glColor4f(r, g, b, 1.0);
}
FuncHook(CullFace, void, TRICULLSTYLE style)
{
	if (style)
	{
		if (style == TRI_NONE)
			glDisable(GL_CULL_FACE);
	}
	else
	{
		glEnable(GL_CULL_FACE);
		glCullFace(GL_FRONT);
	}
}
extern mspriteframe_t* R_GetSpriteFrame(const model_t* pModel, int frame, float yaw);

FuncHook(SpriteTexture, int, struct model_s* pSpriteModel, int frame)
{
	OrigBegin(0);
	OrigEnd();

	mspriteframe_t* sprite = R_GetSpriteFrame(pSpriteModel, frame, 0);
	if (!sprite)
		return 0;

	glBindTexture(GL_TEXTURE_2D, sprite->gl_texturenum);

	return 1;
}
FuncHook(WorldToScreen, int, float* world, float* screen) // Returns 1 if it's z clipped
{
	return OrigWorldToScreen(world, screen);
}
FuncHook(Fog, void, float flFogColor[3], float flStart, float flEnd, int bOn) // Works just like GL_FOG, flFogColor is r/g/b.
{
	OrigFog(flFogColor, flStart, flEnd, bOn);
}
FuncHook(ScreenToWorld, void, float* screen, float* world)
{
	OrigScreenToWorld(screen, world);
}
FuncHook(GetMatrix, void, const int pname, float* matrix)
{
	glGetFloatv(pname, matrix);
}
FuncHook(BoxInPVS, int, float* mins, float* maxs)
{
	return OrigBoxInPVS(mins, maxs);
}
FuncHook(LightAtPoint, void, float* pos, float* value)
{
	OrigLightAtPoint(pos, value);
}
FuncHook(Color4fRendermode, void, float r, float g, float b, float a, int rendermode)
{
	if (rendermode == kRenderTransAlpha)
	{
		glColor4f(r, g, b, a / 255.f);
	}
	else
	{
		glColor4f(a * r, a * g, a * b, 1.0);
	}
}
FuncHook(FogParams, void, float flDensity, int iFogSkybox) // Used with Fog()...sets fog density and whether the fog should be applied to the skybox
{
	OrigFogParams(flDensity, iFogSkybox);
}

FuncHook(GetPlayerState, entity_state_t*, int index)
{
	if (index < 0 || index >= engine_cl->maxclients)
		return nullptr;

	return &engine_cl->frames[engine_cl->parsecountmod].playerstate[index];
}

FuncHook(PlayerInfo, player_info_t*, int index)
{
	if (index < 0 || index >= engine_cl->maxclients)
		return nullptr;

	return &engine_cl->players[index];
}

FuncHook(StudioClientEvents, void)
{
}



void Hook_EFX()
{
	InstallEfxHook(R_AllocParticle)
		InstallEfxHook(R_BlobExplosion)
		InstallEfxHook(R_Blood)
		InstallEfxHook(R_BloodSprite)
		InstallEfxHook(R_BloodStream)
		InstallEfxHook(R_BreakModel)
		InstallEfxHook(R_Bubbles)
		InstallEfxHook(R_BubbleTrail)
		InstallEfxHook(R_BulletImpactParticles)
		InstallEfxHook(R_EntityParticles)
		InstallEfxHook(R_Explosion)
		InstallEfxHook(R_FizzEffect)
		InstallEfxHook(R_FireField)
		InstallEfxHook(R_FlickerParticles)
		InstallEfxHook(R_FunnelSprite)
		InstallEfxHook(R_Implosion)
		InstallEfxHook(R_LargeFunnel)
		InstallEfxHook(R_LavaSplash)
		InstallEfxHook(R_MultiGunshot)
		InstallEfxHook(R_MuzzleFlash)
		InstallEfxHook(R_ParticleBox)
		InstallEfxHook(R_ParticleBurst)
		InstallEfxHook(R_ParticleExplosion)
		InstallEfxHook(R_ParticleExplosion2)
		InstallEfxHook(R_ParticleLine)
		InstallEfxHook(R_PlayerSprites)
		InstallEfxHook(R_Projectile)
		InstallEfxHook(R_RicochetSound)
		InstallEfxHook(R_RicochetSprite)
		InstallEfxHook(R_RocketFlare)
		InstallEfxHook(R_RocketTrail)
		InstallEfxHook(R_RunParticleEffect)
		InstallEfxHook(R_ShowLine)
		InstallEfxHook(R_SparkEffect)
		InstallEfxHook(R_SparkShower)
		InstallEfxHook(R_SparkStreaks)
		InstallEfxHook(R_Spray)
		InstallEfxHook(R_Sprite_Explode)
		InstallEfxHook(R_Sprite_Smoke)
		InstallEfxHook(R_Sprite_Spray)
		InstallEfxHook(R_Sprite_Trail)
		InstallEfxHook(R_Sprite_WallPuff)
		InstallEfxHook(R_StreakSplash)
		InstallEfxHook(R_TracerEffect)
		InstallEfxHook(R_UserTracerParticle)
		InstallEfxHook(R_TracerParticles)
		InstallEfxHook(R_TeleportSplash)
		InstallEfxHook(R_TempSphereModel)
		InstallEfxHook(R_TempModel)
		InstallEfxHook(R_DefaultSprite)
		InstallEfxHook(R_TempSprite)
		InstallEfxHook(Draw_DecalIndex)
		InstallEfxHook(Draw_DecalIndexFromName)
		InstallEfxHook(R_DecalShoot)
		InstallEfxHook(R_AttachTentToPlayer)
		InstallEfxHook(R_KillAttachedTents)
		InstallEfxHook(R_BeamCirclePoints)
		InstallEfxHook(R_BeamEntPoint)
		InstallEfxHook(R_BeamEnts)
		InstallEfxHook(R_BeamFollow)
		InstallEfxHook(R_BeamKill)
		InstallEfxHook(R_BeamLightning)
		InstallEfxHook(R_BeamPoints)
		InstallEfxHook(R_BeamRing)
		InstallEfxHook(CL_AllocDlight)
		InstallEfxHook(CL_AllocElight)
		InstallEfxHook(CL_TempEntAlloc)
		InstallEfxHook(CL_TempEntAllocNoModel)
		InstallEfxHook(CL_TempEntAllocHigh)
		InstallEfxHook(CL_TentEntAllocCustom)
		InstallEfxHook(R_GetPackedColor)
		InstallEfxHook(R_LookupColor)
		InstallEfxHook(R_DecalRemoveAll)
		InstallEfxHook(R_FireCustomDecal)
}

void Hook_TriApi()
{
	InstallTriApiHook(RenderMode);
	InstallTriApiHook(Begin);
	InstallTriApiHook(End);
	InstallTriApiHook(Color4f);
	InstallTriApiHook(Color4ub);
	InstallTriApiHook(TexCoord2f);
	InstallTriApiHook(Vertex3fv);
	InstallTriApiHook(Vertex3f);
	InstallTriApiHook(Brightness);
	InstallTriApiHook(CullFace);
	InstallTriApiHook(SpriteTexture);
	InstallTriApiHook(WorldToScreen);
	InstallTriApiHook(Fog);
	InstallTriApiHook(ScreenToWorld);
	InstallTriApiHook(GetMatrix);
	InstallTriApiHook(BoxInPVS);
	InstallTriApiHook(LightAtPoint);
	InstallTriApiHook(Color4fRendermode);
	InstallTriApiHook(FogParams);
}


void Hook_gEngfuncs_Functions()
{
	//
	//credits to meetem for this cl hooking code
	//
	size_t address = (size_t)gEngfuncs.GetViewModel();
	size_t viewent_offset = offsetof(client_state_s, viewent);
	client_state_s* clientState = (client_state_s*)(address - viewent_offset);
	engine_cl = clientState;
	if (engine_cl->viewent.curstate.msg_time != gEngfuncs.GetViewModel()->curstate.msg_time)
	{
		gEngfuncs.pfnClientCmd("escape\n");
		MessageBox(NULL, "FATAL ERROR: engine cl is incorrect!\n\nPress Ok to quit the game.\n", "ERROR", MB_OK);
		exit(-1);
	}

	Hook_EFX();
	Hook_TriApi();
}

void Hook_IEngineStudio_Functions()
{
	InstallIEngineStudioHook(GetPlayerState);
	InstallIEngineStudioHook(PlayerInfo);
	InstallIEngineStudioHook(StudioClientEvents);
}