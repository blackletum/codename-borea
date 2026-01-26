#pragma once

#include "PlatformHeaders.h"
#include "hud.h"
#include "cl_util.h"
#include "opengl_utils/GL_VertexArrayObject.h"

#include <unordered_map>

class GL_BufferHandler;
class GL_ShaderProgram;
class GL_VertexArrayObject;
struct sprite_quad_t;

class CSpriteRenderer
{
public:
	void Init();
	void VidInit();

	void PushEntityToDraw(cl_entity_s* pEnt);
	void ClearDrawList();

	void DrawSpriteEntities();


	void GetSpriteFrames(const model_t* pModel, int& framecount);
	mspriteframe_t* GetSpriteFrame(const model_t* pModel, int frame, float yaw);

private:
	void QuadifySpriteEnt(cl_entity_t* e);

	bool SpriteOccluded(cl_entity_t* e, Vector origin, float* pscale);
	float SpriteGlowBlend(Vector origin, int rendermode, int renderfx, float* pscale);
	bool SpriteHasLightmap(cl_entity_t* e, int texFormat);

	void MakeSpriteQuad(mspriteframe_t* frame, Vector org, Vector v_right, Vector v_up, float scale, sprite_quad_t* spr_quad);
	void DrawSpriteQuads();
	bool CullSpriteModel(cl_entity_t* e, Vector origin);

private:
	float r_blend = 0.0f;
};

extern CSpriteRenderer g_LegacySpriteRenderer;