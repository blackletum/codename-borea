#include "PlatformHeaders.h"
#include "Platform.h"
#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "studio.h"
#include "entity_state.h"
#include "triangleapi.h"
#include "event_api.h"
#include "pm_defs.h"
#include "pm_movevars.h"

//thought it'd be nice to port some of the peventapi functions from goldsrc to here, try and reduce-
//-the amount of calls to the engine

extern playermove_t* pmove;

int oldphysent;
int oldvisent;
bool pushed;

void EV_PushPMStates()
{
	if (pushed)
	{
		gEngfuncs.Con_Printf("CL_PushPMStates called with pushed stack\n");
	}
	else
	{
		oldphysent = pmove->numphysent;
		oldvisent = pmove->numvisent;
		pushed = true;
	}
}

void EV_PopPMStates()
{
	if (pushed)
	{
		pmove->numphysent = oldphysent;
		pmove->numvisent = oldvisent;
		pushed = false;
	}
	else
	{
		gEngfuncs.Con_Printf("CL_PopPMStates called without stack\n");
	}
}

int EV_FindModelIndex(const char* pmodel)
{
	for (int i = 1; i < engine_cl->model_precache_count; ++i)
	{
		if (engine_cl->model_precache[i] && !stricmp(pmodel, engine_cl->model_precache[i]->name))
		{
			return i;
		}
	}

	return 0;
}

int EV_LocalPlayerDucking()
{
	// TODO: define constant - Solokiller
	return engine_cl->usehull == 1;
}

void EV_LocalPlayerViewheight(float* viewheight)
{
	viewheight[0] = engine_cl->viewheight[0];
	viewheight[1] = engine_cl->viewheight[1];
	viewheight[2] = engine_cl->viewheight[2];
}

bool EV_LocalPlayerDucking(float* viewheight)
{
	return engine_cl->usehull == 1;
}

int EV_IndexFromTrace(pmtrace_t* pTrace)
{
	if (pTrace)
		return pmove->physents[pTrace->ent].info;

	return 0;
}

physent_t* EV_GetPhysent(int idx)
{
	if (idx >= 0 && idx < pmove->numphysent)
		return &pmove->physents[idx];

	return nullptr;
}

void EV_SetTraceHull(int hull)
{
	pmove->usehull = hull;
}

void EV_WeaponAnimation(int sequence, int body)
{
	engine_cl->weaponstarttime = 0;
	engine_cl->weaponsequence = sequence;
	engine_cl->viewent.curstate.body = body;
	if (engine_cls->demorecording)
	{
		if (!engine_cls->demofile)
			return;

		gEngfuncs.pfnWeaponAnim(sequence, body); //dont have realtime and host_framecount, for now. :(

		//demo_anim_t demcmd;
		//
		//
		//demcmd.cmd = '\a';
		//demcmd.time = (realtime - g_pcls.demostarttime);
		//demcmd.frame = (host_framecount - g_pcls.demostartframe);
		//demcmd.anim = anim;
		//demcmd.body = body;
		//FS_Write(&demcmd, 17, 1, g_pcls.demofile);
	}
}

unsigned short EV_PrecacheEvent(int type, const char* psz)
{
	for (int i = 1; engine_cl->event_precache[i].filename; ++i)
	{
		if (!stricmp(engine_cl->event_precache[i].filename, psz))
			return i;
	}

	return 0;
}


event_info_t* CL_FindEmptyEvent(void)
{
	int i;
	event_state_t* es;
	event_info_t* ei;

	es = &engine_cl->events;

	// look for first slot where index is != 0
	for (i = 0; i < MAX_EVENT_QUEUE; i++)
	{
		ei = &es->ei[i];
		if (ei->index != 0)
			continue;
		return ei;
	}

	// no slots available
	return NULL;
}

event_info_t* CL_FindUnreliableEvent(void)
{
	event_state_t* es;
	event_info_t* ei;
	int i;

	es = &engine_cl->events;

	for (i = 0; i < MAX_EVENT_QUEUE; i++)
	{
		ei = &es->ei[i];
		if (ei->index != 0)
		{
			// it's reliable, so skip it
			if (ei->flags & FEV_RELIABLE)
				continue;
		}
		return ei;
	}

	// this should never happen
	return NULL;
}

void CL_QueueEvent(int flags, int index, float delay, event_args_t* args)
{
	event_info_t* ei;

	// find a normal slot
	ei = CL_FindEmptyEvent();

	if (!ei)
	{
		if (flags & FEV_RELIABLE)
		{
			ei = CL_FindUnreliableEvent();
		}

		if (!ei)
			return;
	}

	ei->index = index;
	ei->packet_index = 0;
	ei->fire_time = delay ? (engine_cl->time + delay) : 0.0f;
	ei->flags = flags;
	ei->args = *args;
}

void EV_PlaybackEvent(int flags, const edict_t* pInvoker, unsigned short eventindex, float delay, float* origin, float* angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2)
{
	if (engine_cls->demorecording && engine_cls->demofile)
	{
		//demo_event_t demcmd;
		//
		//demcmd.cmd = '\x06';
		//demcmd.time = (realtime - g_pcls.demostarttime);
		//demcmd.frame = (host_framecount - g_pcls.demostartframe);
		//demcmd.flags = flags;
		//demcmd.idx = idx;
		//demcmd.delay = delay;
		//FS_Write(&demcmd, 21, 1, g_pcls.demofile);
		//FS_Write(pargs, 72, 1, g_pcls.demofile);

		gEngfuncs.pfnPlaybackEvent(flags, pInvoker, eventindex, delay, origin, angles, fparam1, fparam2, iparam1, iparam2, bparam1, bparam2);
		return;
	}

	if (!(flags & FEV_SERVER))
	{
		event_args_t event;
		memset(&event, 0, sizeof(event));

		event.origin[0] = origin[0];
		event.origin[1] = origin[1];
		event.origin[2] = origin[2];

		event.angles[0] = angles[0];
		event.angles[1] = angles[1];
		event.angles[2] = angles[2];

		event.velocity[0] = engine_cl->simvel[0];
		event.velocity[1] = engine_cl->simvel[1];
		event.velocity[2] = engine_cl->simvel[2];

		event.fparam1 = fparam1;
		event.fparam2 = fparam2;
		event.iparam1 = iparam1;
		event.iparam2 = iparam2;
		event.bparam1 = bparam1;
		event.bparam2 = bparam2;

		event.entindex = engine_cl->playernum + 1;
		event.ducking = engine_cl->usehull == 1;

		CL_QueueEvent(flags, eventindex, delay, &event);
	}
}

void EV_KillEvents(int entnum, const char* eventname)
{
	for (int i = 0; i < ARRAYSIZE(engine_cl->events.ei); ++i)
	{
		auto& event = engine_cl->events.ei[i];

		if (event.index != 0 &&
			event.args.entindex == entnum &&
			!stricmp(eventname, engine_cl->event_precache[event.index].filename))
		{
			event.index = 0;
			memset(&event.args, 0, 72);
			event.fire_time = 0.0;
			event.flags = 0;
		}
	}
}

msurface_t* SurfaceAtPoint(model_t* pModel, mnode_t* node, vec_t* start, vec_t* end)
{
	mplane_t* plane;
	mnode_t* subnode;
	float front, back, frac, mins[2];
	int continue1, n_surf;
	Vector mid, start_;
	msurface_t* surf;

	start_ = start;

	while (true)

{
		if (node->contents < 0)
			return NULL;

		plane = node->plane;

		front = DotProduct(start_, plane->normal) - plane->dist;
		back = DotProduct(end, plane->normal) - plane->dist;

		if (front < 0)
			continue1 = 1;
		else
			continue1 = 0;

		subnode = node->children[continue1];

		if ((back < 0) == continue1)
			return SurfaceAtPoint(pModel, subnode, start_, end);

		frac = (float)front / (front - back);

		mid[0] = (end[0] - start_[0]) * frac + start_[0];
		mid[1] = (end[1] - start_[1]) * frac + start_[1];
		mid[2] = (end[2] - start_[2]) * frac + start_[2];

		surf = SurfaceAtPoint(pModel, subnode, start_, mid);

		if (surf != NULL)
			return surf;

		if ((back < 0) == continue1)
			return NULL;

		n_surf = node->numsurfaces;
		surf = &pModel->surfaces[node->firstsurface];

		for (int i = 0; i < n_surf; i++, surf++)
		{
			mtexinfo_t* tex = surf->texinfo;

			mins[0] = DotProduct(tex->vecs[0], mid) + tex->vecs[0][3];

			short s = surf->texturemins[0];

			if (mins[0] >= s)
			{
				mins[1] = DotProduct(tex->vecs[1], mid) + tex->vecs[1][3];

				short ds = mins[0] - s;

				if (mins[1] >= surf->texturemins[1] && ds <= surf->extents[0] && mins[1] - surf->texturemins[1] <= surf->extents[1])
					return surf;
			}
		}
		node = node->children[!continue1];
		start_ = mid;
	}

	return SurfaceAtPoint(pModel, node->children[!continue1], mid, end);
}

const char* EV_TraceTexture(int ground, float* vstart, float* vend)
{
	Vector temp, end, right, forward, up, delta;
	msurface_t* psurf;

	if (ground < 0)
		return NULL;
	if (ground >= pmove->numphysent)
		return NULL;
	if (!pmove->physents[ground].model)
		return NULL;

	if (ground < 1)
	{
		VectorCopy(vstart, temp);
		VectorCopy(vend, end);
	}
	else
	{
		VectorAdd(pmove->physents[ground].model->hulls[0].clip_mins, pmove->physents[ground].origin, delta);
		VectorSubtract(vstart, delta, temp);
		VectorSubtract(vend, delta, end);

		if (!VectorCompare(pmove->physents[ground].angles, vec3_origin))
		{
			AngleVectors(pmove->physents[ground].angles, &forward, &right, &up);
			temp[0] = DotProduct(temp, forward);
			temp[1] = -DotProduct(temp, right);
			temp[2] = DotProduct(temp, up);

			end[0] = DotProduct(end, forward);
			end[1] = -DotProduct(end, right);
			end[2] = DotProduct(end, up);
		}
	}

	if (pmove->physents[ground].model->type == mod_brush && pmove->physents[ground].model->nodes)
	{
		psurf = SurfaceAtPoint(pmove->physents[ground].model, &pmove->physents[ground].model->nodes[0], temp, end);
		if (psurf)
			return psurf->texinfo->texture->name;
	}

	return NULL;
}