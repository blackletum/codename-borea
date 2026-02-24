#pragma once

#include "GL_CommonInclude.h"

struct gl_vertattrib_t
{
	uint32_t index;		  // attribute index in shader
	uint32_t numelements; //
	uint32_t type;		  // GL_FLOAT, GL_UNSIGNED_SHORT, etc
	uint32_t structsize;  // sizeof(struct);
	uint32_t offset;	  // ex: offsetof(struct, vertpos);

	bool integer_attrib; // ex: ivec4 or vec4 (glvertexattribI or glvertexattribI)
	bool normalize;
};

struct gl_vertattriblist_t
{
	const gl_vertattrib_t* attribs;
	const int numattribs;
};

#define GL_DECLARE_ATTRIBLIST()										\
	static const gl_vertattriblist_t* GetAttribLayout()				\


#define GL_BEGIN_ATTRIBLIST(structure)								\
	const gl_vertattriblist_t* structure::GetAttribLayout() {		\
		typedef structure _struct;									\
		static const gl_vertattrib_t _struct_attribinfo[] = {		



#define GL_DEFINE_ATTRIB(index, numelements, type, member) \
	{index, numelements, type, sizeof(_struct), offsetof(_struct, member), false, true},
#define GL_DEFINE_NORMALIZEDATTRIB(index, numelements, type, member) \
	{index, numelements, type, sizeof(_struct), offsetof(_struct, member), false, true},
#define GL_DEFINE_INTEGERATTRIB(index, numelements, type, member) \
	{index, numelements, type, sizeof(_struct), offsetof(_struct, member), true, false},



#define GL_END_ATTRIBLIST()								\
		};												\
														\
		static const gl_vertattriblist_t attriblist = {	\
				_struct_attribinfo,						\
				std::size(_struct_attribinfo)			\
		};												\
		return &attriblist;								\
	};

class GL_VertexArrayObject // should this be shortened to VAO ? dunno, maybe not, want my code to be clear
{
public:
	GL_VertexArrayObject();
	~GL_VertexArrayObject();

	#ifdef _DEBUG
	void BindVAO();

	static void ResetVAOBinding();

	#else
	__forceinline void BindVAO() { glBindVertexArray(m_uiVAOIndex); m_pCurrentBoundVAO = this; }

	static __forceinline void ResetVAOBinding() { glBindVertexArray(0); m_pCurrentBoundVAO = nullptr; };

	#endif

	void SetVertexAttributes(const gl_vertattriblist_t* attriblist);


	static __forceinline GL_VertexArrayObject* GetBoundVAO() { return m_pCurrentBoundVAO; };

	GLuint m_uiVAOIndex = 0;

private:

	static GL_VertexArrayObject* m_pCurrentBoundVAO;
};