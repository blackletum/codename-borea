//==============================================================================//
//			Copyright (c) 2010 - 2011, Richard Roh��, All rights reserved.		//
//==============================================================================//

//==============================================================================//
//							Optimized view frustum culling						//
//								Based on code from Quake						//
//					  Written for Half-Life: Amnesia modification				//
//==============================================================================//

#include "hud.h"
#include "cl_util.h"
#include "const.h"
#include "com_model.h"
#include "frustum.h"

#include "ref_params.h"
#include "bsprenderer.h"

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846 // matches value in gcc v2 math.h
#endif

#define DEG2RAD(a) (a * M_PI) / 180.0F
#define RAD2DEG(x) ((double)(x) * (double)(180.0 / M_PI))

/*
====================
CalcFov

====================
*/
float FrustumCheck::CalcFov(float flFovX, float flWidth, float flHeight)
{
	float a;
	float x, half_fov_y;

	if (flFovX < 1 || flFovX > 179)
	{
		gEngfuncs.Con_Printf("CalcFov(): bad field of view value!\n");
		flFovX = 90;
	}

	x = flWidth / tan(flFovX / 360 * M_PI);

	a = atan(flHeight / x);

	a = a * 360 / M_PI;

	return a;
}

/*
====================
V_AdjustFov
====================
*/
void FrustumCheck::V_AdjustFov(float& fov_x, float& fov_y, float width, float height, bool lock_x)
{
	float x, y;

	if (!gBSPRenderer.gl_widescreen_yfov->value || width * 3 == 4 * height || width * 4 == height * 5)
	{
		fov_x = 2.0f * atan(tan(glm::radians(fov_x) / 2.0f) / (width / height));
		return;
	}

	if (lock_x)
	{
		fov_y = 2 * atan((width * 3) / (height * 4) * tan(fov_y * M_PI / 360.0f * 0.5f)) * 360 / M_PI;
		return;
	}

	y = CalcFov(fov_x, 640, 480);
	x = fov_x;

	fov_x = CalcFov(y, height, width);
	if (fov_x < x)
		fov_x = x;
	else
		fov_y = y;
}

/*
=====================
SetupFrustum

=====================
*/
void FrustumCheck::SetFrustum(Vector vAngles, Vector vOrigin, float flFOV_x, float flFarDist, bool bView)
{
	Vector vVpn, vUp, vRight;
	AngleVectors(vAngles, &vVpn, &vRight, &vUp);

	if (bView)
	{
		float flFOV_y = flFOV_x;

		if (gBSPRenderer.gl_widescreen_yfov->value)
			V_AdjustFov(flFOV_x, flFOV_y, ScreenWidth, ScreenHeight, 0);


		Q_RotatePointAroundVector(m_sFrustum[0].normal, vUp, vVpn, -(90 - flFOV_x / 2));
		Q_RotatePointAroundVector(m_sFrustum[1].normal, vUp, vVpn, 90 - flFOV_x / 2);
		Q_RotatePointAroundVector(m_sFrustum[2].normal, vRight, vVpn, 90 - flFOV_y / 2);
		Q_RotatePointAroundVector(m_sFrustum[3].normal, vRight, vVpn, -(90 - flFOV_y / 2));
	}
	else
	{
		Q_RotatePointAroundVector(m_sFrustum[0].normal, vUp, vVpn, -(90 - flFOV_x / 2));
		Q_RotatePointAroundVector(m_sFrustum[1].normal, vUp, vVpn, 90 - flFOV_x / 2);
		Q_RotatePointAroundVector(m_sFrustum[2].normal, vRight, vVpn, 90 - flFOV_x / 2);
		Q_RotatePointAroundVector(m_sFrustum[3].normal, vRight, vVpn, -(90 - flFOV_x / 2));
	}


	for (int i = 0; i < 4; i++)
	{
		m_sFrustum[i].type = PLANE_ANYZ;
		m_sFrustum[i].dist = DotProduct(vOrigin, m_sFrustum[i].normal);
		m_sFrustum[i].signbits = Q_SignbitsForPlane(&m_sFrustum[i]);
	}

	// Reset this value
	m_iFarClip = FARCLIP_OFF;

	if (bView && !gHUD.m_pFogSettings.affectsky)
		return;

	if (flFarDist)
	{
		Vector vFarPoint;
		VectorCopy(vVpn, vFarPoint);

		vFarPoint[0] *= flFarDist;
		vFarPoint[1] *= flFarDist;
		vFarPoint[2] *= flFarDist;

		VectorAdd(vOrigin, vFarPoint, vFarPoint);

		m_sFrustum[4].normal[0] = vVpn[0] * (-1);
		m_sFrustum[4].normal[1] = vVpn[1] * (-1);
		m_sFrustum[4].normal[2] = vVpn[2] * (-1);

		m_sFrustum[4].type = PLANE_ANYZ;
		m_sFrustum[4].dist = DotProduct(vFarPoint, m_sFrustum[4].normal);
		m_sFrustum[4].signbits = Q_SignbitsForPlane(&m_sFrustum[4]);
		m_iFarClip = FARCLIP_DEPTH;
	}
}



/*
=====================
SetExtraCullBox

=====================
*/
void FrustumCheck::SetExtraCullBox(Vector vMins, Vector vMaxs)
{
	VectorCopy(vMins, m_vExtraCullMins);
	VectorCopy(vMaxs, m_vExtraCullMaxs);
	m_bExtraCull = true;
}

/*
=====================
DisableExtraCullBox

=====================
*/
void FrustumCheck::DisableExtraCullBox(void)
{
	m_bExtraCull = false;
}

/*
=====================
RadialCullBox

=====================
*/
bool FrustumCheck::RadialCullBox(Vector vMins, Vector vMaxs)
{
	if (m_vCullBoxMins[0] > vMaxs[0])
		return true;
	if (m_vCullBoxMins[1] > vMaxs[1])
		return true;
	if (m_vCullBoxMins[2] > vMaxs[2])
		return true;

	if (m_vCullBoxMaxs[0] < vMins[0])
		return true;
	if (m_vCullBoxMaxs[1] < vMins[1])
		return true;
	if (m_vCullBoxMaxs[2] < vMins[2])
		return true;

	return false;
}

/*
=====================
ExtraCullBox

=====================
*/
bool FrustumCheck::ExtraCullBox(Vector vMins, Vector vMaxs)
{
	if (m_vExtraCullMins[0] > vMaxs[0])
		return true;
	if (m_vExtraCullMins[1] > vMaxs[1])
		return true;
	if (m_vExtraCullMins[2] > vMaxs[2])
		return true;

	if (m_vExtraCullMaxs[0] < vMins[0])
		return true;
	if (m_vExtraCullMaxs[1] < vMins[1])
		return true;
	if (m_vExtraCullMaxs[2] < vMins[2])
		return true;

	return false;
}

/*
=====================
CullBox

=====================
*/
bool FrustumCheck::CullBox(Vector vMins, Vector vMaxs)
{
	if (m_bExtraCull)
	{
		if (ExtraCullBox(vMins, vMaxs))
			return true;
	}

	if (m_iFarClip == FARCLIP_DEPTH)
	{
		if (BoxOnPlaneSide(vMins, vMaxs, &m_sFrustum[4]) == 2)
			return true;
	}
	else if (m_iFarClip == FARCLIP_RADIAL)
	{
		if (RadialCullBox(vMins, vMaxs))
			return true;
	}

	for (int i = 0; i < 4; i++)
	{
		if (BoxOnPlaneSide(vMins, vMaxs, &m_sFrustum[i]) == 2)
			return true;
	}

	return false;
}

/*
==================
Q_SignbitsForPlane

==================
*/
int FrustumCheck::Q_SignbitsForPlane(mplane_t* pOut)
{
	int nBits = 0;

	for (int j = 0; j < 3; j++)
	{
		if (pOut->normal[j] < 0)
			nBits |= 1 << j;
	}

	return nBits;
}

/*
==========================
Q_RotatePointAroundVector

==========================
*/
void FrustumCheck::Q_RotatePointAroundVector(Vector& vDest, const Vector vDir, Vector& vPoint, float flDegrees)
{
	Vector q;
	float q3;
	Vector t;
	float t3;

	{
		float hrad;
		float s;

		hrad = DEG2RAD(flDegrees) / 2;
		s = sin(hrad);
		VectorScale(vDir, s, q);
		q3 = cos(hrad);
	}

	CrossProduct(q, vPoint, t);
	VectorMA(t, q3, vPoint, t);
	t3 = DotProduct(q, vPoint);

	CrossProduct(q, t, vDest);
	VectorMA(vDest, t3, q, vDest);
	VectorMA(vDest, q3, t, vDest);
}