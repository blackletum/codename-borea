#pragma once

#include "hud.h"
#include "cl_util.h"

#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"

/*
====================
CBaseTempEntity

purpose: kind of mimicks a server entity, animates and runs thinking/touching function

====================
*/


class CBaseTempEntity
{
public:

	CBaseTempEntity(TEMPENTITY* tempent);
	~CBaseTempEntity();

	virtual void Spawn() {};
	virtual void Think() { StudioFrameAdvance(); };
	virtual void Touch() {};
	virtual void Die() {};

	static void TempEntThink(TEMPENTITY* tempent, float frametime, float currenttime);
	static void TempEntTouch(TEMPENTITY* tempent, pmtrace_t* ptr);
	static void TempEntDie(TEMPENTITY* tempent);

protected:

	void StudioFrameAdvance()
	{
		float flInterval = (engine_cl->time - m_pTempEnt->entity.curstate.animtime);
		if (flInterval <= 0.001)
		{
			m_pTempEnt->entity.curstate.animtime = engine_cl->time;
			return;
		}

		if (!m_pTempEnt->entity.curstate.animtime)
			flInterval = 0.0;

		m_pTempEnt->entity.curstate.frame += flInterval * m_flFrameRate * m_pTempEnt->entity.curstate.framerate;
		m_pTempEnt->entity.curstate.animtime = engine_cl->time;

		if (m_pTempEnt->entity.curstate.frame < 0.0 || m_pTempEnt->entity.curstate.frame >= 256.0)
		{
			if (m_fSequenceLoops)
				m_pTempEnt->entity.curstate.frame -= (int)(m_pTempEnt->entity.curstate.frame / 256.0) * 256.0;
			else
				m_pTempEnt->entity.curstate.frame = (m_pTempEnt->entity.curstate.frame < 0.0) ? 0 : 255;
			m_fSequenceFinished = true;	// just in case it wasn't caught in GetEvents
		}

		return;
	}

	void SetSequence(const char* label)
	{
		studiohdr_t* pstudiohdr;
		mstudioseqdesc_t* pseqdesc;

		if (!m_pTempEnt->entity.model)
			return;

		pstudiohdr = (studiohdr_t*)m_pTempEnt->entity.model->cache.data;
		if (!pstudiohdr)
			return;

		pseqdesc = (mstudioseqdesc_t*)((byte*)pstudiohdr + pstudiohdr->seqindex);

		for (int i = 0; i < pstudiohdr->numseq; i++)
		{
			if (stricmp(pseqdesc[i].label, label) == 0)
			{
				m_pTempEnt->entity.curstate.sequence = i;
				m_pTempEnt->entity.curstate.frame = 0;
				m_pTempEnt->entity.curstate.animtime = engine_cl->time;
				m_pTempEnt->entity.curstate.framerate = 1;

				if (pseqdesc[i].numframes > 1)
				{
					m_flFrameRate = 256 * pseqdesc[i].fps / (pseqdesc[i].numframes - 1);
				}
				else
				{
					m_flFrameRate = 256.0;
				}

				m_fSequenceLoops = pseqdesc[i].flags & STUDIO_LOOPING;

				m_fSequenceFinished = false;

				return;
			}
		}

	}

	int LookupSequence(const char* label)
	{
		studiohdr_t* pstudiohdr;
		mstudioseqdesc_t* pseqdesc;

		if (!m_pTempEnt->entity.model)
			return -1;

		pstudiohdr = (studiohdr_t*)m_pTempEnt->entity.model->cache.data;
		if (!pstudiohdr)
			return -1;

		pseqdesc = (mstudioseqdesc_t*)((byte*)pstudiohdr + pstudiohdr->seqindex);

		for (int i = 0; i < pstudiohdr->numseq; i++)
		{
			if (stricmp(pseqdesc[i].label, label) == 0)
			{
				return i;
			}
		}
	}

	TEMPENTITY* m_pTempEnt;

	// animation needs
	float				m_flFrameRate = 0;			// computed FPS for current sequence
	float				m_flGroundSpeed = 0;		// computed linear movement rate for current sequence
	float				m_flLastEventCheck = 0;		// last time the event list was checked
	bool				m_fSequenceFinished = false;// flag set when StudioAdvanceFrame moves across a frame boundry
	bool				m_fSequenceLoops = false;	// true if the sequence loops

	static std::vector<CBaseTempEntity*> m_vTempEnts;

};







class CTempBloodPuddle : public CBaseTempEntity
{
public:

	CTempBloodPuddle(TEMPENTITY* tempent) : CBaseTempEntity(tempent) { this->Spawn(); };
	~CTempBloodPuddle() {};

	void Spawn() override;
	void Think() override;
	void Die() override;

private:
	float m_fBleedStartTime = 0;
};