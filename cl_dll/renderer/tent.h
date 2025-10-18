
#pragma once

class CTempEntity
{
public:
	int TempEntities(const char* pszName, int iSize, void* pBuf);

private:
	void CreateExplosion();
	void HandleCustomDecals();
};

extern CTempEntity gTempEntities;