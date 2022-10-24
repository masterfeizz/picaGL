#include "internal.h"

static inline GPU_BLENDFACTOR pgl_convert_blendfactor(GLenum factor) 
{
	switch(factor)
	{
		case GL_ZERO: 						return GPU_ZERO;
		case GL_ONE:  						return GPU_ONE;
		case GL_SRC_COLOR: 					return GPU_SRC_COLOR;
		case GL_ONE_MINUS_SRC_COLOR: 		return GPU_ONE_MINUS_SRC_COLOR;
		case GL_DST_COLOR: 					return GPU_DST_COLOR;
		case GL_ONE_MINUS_DST_COLOR: 		return GPU_ONE_MINUS_DST_COLOR;
		case GL_SRC_ALPHA: 					return GPU_SRC_ALPHA;
		case GL_ONE_MINUS_SRC_ALPHA: 		return GPU_ONE_MINUS_SRC_ALPHA;
		case GL_DST_ALPHA: 					return GPU_DST_ALPHA;
		case GL_ONE_MINUS_DST_ALPHA: 		return GPU_ONE_MINUS_DST_ALPHA;
		case GL_SRC_ALPHA_SATURATE: 		return GPU_SRC_ALPHA_SATURATE;
		case GL_CONSTANT_COLOR: 			return GPU_CONSTANT_COLOR;
		case GL_ONE_MINUS_CONSTANT_COLOR: 	return GPU_ONE_MINUS_CONSTANT_COLOR;
		case GL_CONSTANT_ALPHA: 			return GPU_CONSTANT_ALPHA;
		case GL_ONE_MINUS_CONSTANT_ALPHA: 	return GPU_ONE_MINUS_CONSTANT_ALPHA;
	}
	
	return GPU_ONE;
}

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

void glAlphaFunc(GLenum func, GLclampf ref)
{
	pgl_state.alpha_test.function = pgl_convert_testfunc(func);
	pgl_state.alpha_test.reference = (GLuint)(ref * 255.0f);

	pgl_state.changes |= pgl_change_alphatest;
}

void glBlendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{ 
	uint8_t r = red   * 255;
	uint8_t b = blue  * 255;
	uint8_t g = green * 255;
	uint8_t a = alpha * 255;

	pgl_state.blend.color = r | (g << 8) | (b << 16) | (a << 24);

	pgl_state.changes |= pgl_change_blend;
}

void glBlendEquation(GLenum mode)
{
	switch(mode)
	{
		case GL_FUNC_SUBTRACT:
			pgl_state.blend.alpha_eq = pgl_state.blend.rgb_eq = GPU_BLEND_SUBTRACT;
			break;
		case GL_FUNC_REVERSE_SUBTRACT:
			pgl_state.blend.alpha_eq = pgl_state.blend.rgb_eq = GPU_BLEND_REVERSE_SUBTRACT;
			break;
		default:
			pgl_state.blend.alpha_eq = pgl_state.blend.rgb_eq = GPU_BLEND_ADD;
			break;
	}

	pgl_state.changes |= pgl_change_blend;
}

void glBlendFunc(GLenum sfactor, GLenum dfactor)
{
	pgl_state.blend.alpha_sfactor = pgl_state.blend.rgb_sfactor = pgl_convert_blendfactor(sfactor);
	pgl_state.blend.alpha_dfactor = pgl_state.blend.rgb_dfactor = pgl_convert_blendfactor(dfactor);

	pgl_state.changes |= pgl_change_blend;
}