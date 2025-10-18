//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#ifndef PITCH
// MOVEMENT INFO
// up / down
#define	PITCH	0
// left / right
#define	YAW		1
// fall over
#define	ROLL	2
#endif

#define FDotProduct( a, b ) (fabs((a[0])*(b[0])) + fabs((a[1])*(b[1])) + fabs((a[2])*(b[2])))

void	AngleMatrix(const float* angles, matrix3x4_t& matrix);
bool	VectorCompare(const float* v1, const float* v2);
void	CrossProduct(const float* v1, const float* v2, float* cross);
void	VectorTransform(const float* in1, const matrix3x4_t& in2, float* out);
void	ConcatTransforms(const matrix3x4_t& in1, const matrix3x4_t& in2, matrix3x4_t& out);
void	MatrixCopy(const matrix3x4_t& in, matrix3x4_t& out);
void	QuaternionMatrix(vec4_t quaternion, matrix3x4_t& matrix);
void	QuaternionSlerp(vec4_t p, vec4_t q, float t, vec4_t qt);
void	AngleQuaternion(float* angles, vec4_t quaternion);
