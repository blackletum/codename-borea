#include "hud.h"
#include "cl_util.h"

#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"

#include "tempents.h"

std::vector<CBaseTempEntity*> CBaseTempEntity::m_vTempEnts;

void CBaseTempEntity::TempEntThink(TEMPENTITY* tempent, float frametime, float currenttime)
{
	auto basetempent = ((CBaseTempEntity*)tempent->entity.topnode);
	assert(basetempent);

	basetempent->Think();
}

void CBaseTempEntity::TempEntDie(TEMPENTITY* tempent)
{
	auto basetempent = ((CBaseTempEntity*)tempent->entity.topnode);
	assert(basetempent);

	basetempent->Die();
	delete basetempent;
}

void CBaseTempEntity::TempEntTouch(TEMPENTITY* tempent, pmtrace_t* ptr)
{
	auto basetempent = ((CBaseTempEntity*)tempent->entity.topnode);
	assert(basetempent);

	basetempent->Touch();
}

CBaseTempEntity::CBaseTempEntity(TEMPENTITY* tempent)
{
	m_vTempEnts.push_back(this);

	m_pTempEnt = tempent;
	tempent->callback = TempEntThink;
	tempent->hitcallback = TempEntTouch;
	tempent->diecallback = TempEntDie;
	tempent->entity.topnode = (mnode_s*)this;
}

CBaseTempEntity::~CBaseTempEntity()
{
	for (int i = 0; i < m_vTempEnts.size(); i++)
	{
		if (m_vTempEnts[i] != this)
			continue;

		m_vTempEnts.erase(m_vTempEnts.begin() + i);
		break;
	}
}


void CTempBloodPuddle::Spawn()
{
	m_pTempEnt->entity.model = IEngineStudio.Mod_ForName("models/bloodpuddle.mdl", false);
	m_pTempEnt->die = std::numeric_limits<float>::max();
	SetSequence("smallidle");
	m_fBleedStartTime = engine_cl->time + 1;
	m_pTempEnt->flags = FTENT_CLIENTCUSTOM | FTENT_NOGRAVITY;
	m_pTempEnt->entity.curstate.scale = 1;
}

void CTempBloodPuddle::Think()
{
	StudioFrameAdvance();

	if (engine_cl->time >= m_fBleedStartTime && m_pTempEnt->entity.curstate.sequence == LookupSequence("getbiggur") && m_fSequenceFinished)
		SetSequence("idle");

	else if (engine_cl->time >= m_fBleedStartTime && m_pTempEnt->entity.curstate.sequence != LookupSequence("idle") && m_pTempEnt->entity.curstate.sequence != LookupSequence("getbiggur"))
	{
		SetSequence("getbiggur");
	}
}
void CTempBloodPuddle::Die()
{
	CBaseTempEntity::Die();
}