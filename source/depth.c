#include "internal.h"

static inline GPU_TESTFUNC pgl_convert_testfunc(GLenum func)
{
	switch(func)
	{
		case GL_NEVER: 		return GPU_NEVER;
		case GL_EQUAL: 		return GPU_EQUAL;
		case GL_LEQUAL: 	return GPU_LEQUAL;
		case GL_GREATER: 	return GPU_GREATER;
		case GL_NOTEQUAL: 	return GPU_NOTEQUAL;
		case GL_GEQUAL: 	return GPU_GEQUAL;
		case GL_LESS:		return GPU_LESS;
		default:			return GPU_ALWAYS;
	}
}

void glClearDepth(GLclampd depth)
{
	//I don't think this is right...
	pgl_state.clear_depth = (float)-depth;
}

void glDepthFunc(GLenum func)
{
	pgl_state.depth_test.function = pgl_convert_testfunc(func);

	pgl_state.changes |= pglDirtyFlag_DepthTest;
}

void glDepthMask(GLboolean flag)
{
	pgl_state.depth_test.write_mask = (pgl_state.depth_test.write_mask & 0xF) | ((flag & 0x1) << 4);

	pgl_state.changes |= pglDirtyFlag_DepthTest;
}

void glDepthRange(GLclampd nearVal, GLclampd farVal)
{
	pgl_state.depthmap.near = (float)farVal;
	pgl_state.depthmap.far  = (float)nearVal;

	pgl_state.changes |= pglDirtyFlag_DepthMap;
}

void glPolygonOffset(GLfloat factor, GLfloat units) 
{
	//Not correct, but seems to do it for my needs
	pgl_state.depthmap.offset = units / (1 << 12);

	pgl_state.changes |= pglDirtyFlag_DepthMap;
}