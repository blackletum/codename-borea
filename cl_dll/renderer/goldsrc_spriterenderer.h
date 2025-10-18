#pragma once

#include "PlatformHeaders.h"
#include "hud.h"
#include "cl_util.h"

#include <unordered_map>

class GL_BufferHandler;
class GL_ShaderProgram;
class GL_VertexArrayObject;

class CSpriteRenderer
{
public:
	void Init();
	void VidInit();

	void PushEntityToDraw(cl_entity_s* pEnt);
	void ClearDrawList() { m_vSpriteDrawList.clear(); }

	void DrawSpriteEntities();


	void GetSpriteFrames(const model_t* pModel, int& framecount);
	mspriteframe_t* GetSpriteFrame(const model_t* pModel, int frame, float yaw);

private:

	struct sprite_vertex_t
	{
		Vector point;  // 12 bytes
		color32 color; // x, y, z, a //4 bytes
		int padding[4];
	};

	struct sprite_quad_t
	{
		sprite_vertex_t vert[4];
		byte rendermode;
	};

	void QuadifySpriteEnt(cl_entity_t* e);

	bool SpriteOccluded(cl_entity_t* e, Vector origin, float* pscale);
	float SpriteGlowBlend(Vector origin, int rendermode, int renderfx, float* pscale);
	bool SpriteHasLightmap(cl_entity_t* e, int texFormat);

	void MakeSpriteQuad(mspriteframe_t* frame, Vector org, Vector v_right, Vector v_up, float scale, sprite_quad_t* spr_quad);
	void DrawSpriteQuads();
	bool CullSpriteModel(cl_entity_t* e, Vector origin);

	std::vector<cl_entity_s*> m_vSpriteDrawList;
	std::unordered_map<int, std::vector<sprite_quad_t>> m_vSpriteQuadList;

	GL_BufferHandler* m_pSpriteQuadBuffer;
	GL_ShaderProgram* m_pSpriteShader;
	GL_VertexArrayObject* m_pSpriteVAO;

	cl_entity_s* m_pCurrentEntity;

private:
	float r_blend = 0.0f;
};

extern CSpriteRenderer g_LegacySpriteRenderer;