#pragma once

struct gl_vertattriblist_t;
class GL_VertexArrayObject;
class GL_BufferHandler;

class GL_Mesh
{
public:
	GL_Mesh(uint32_t numverts, const gl_vertattriblist_t* vertattribute, bool bStatic = true, void* vertdata = nullptr);
	~GL_Mesh();

	void SetDrawMode(uint32_t drawmode);
	void SetIndices(uint32_t* indices, uint32_t numindeces);

	void BindMesh();
	void ModifyMesh(void* vertdata, uint32_t numverts, uint32_t offset);

	void DrawArrays(uint32_t startvert, uint32_t numverts);
	void MultiDrawArrays(int *startverts, int *numverts, uint32_t numdraws);
	void DrawElements(uint32_t startindex, uint32_t numindeces);

	static void UnbindMesh();
	static GL_Mesh* GetBoundMesh();

private:
	GL_VertexArrayObject* m_pVAO;
	GL_BufferHandler* m_pBuffer;
	GL_BufferHandler* m_pIndiceBuffer;
	uint32_t m_uiVertSize;
	uint32_t m_eDrawMode;
};