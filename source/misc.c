#include "internal.h"

extern mat4f pgl_matrix_identity;

void glClear(GLbitfield mask)
{
	uint32_t write_mask = 0;

	if(mask & GL_COLOR_BUFFER_BIT)
		write_mask |= pgl_state.depth_test.write_mask & 0x0F;
	if(mask & GL_DEPTH_BUFFER_BIT)
		write_mask |= pgl_state.depth_test.write_mask & 0x10;
	
	GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_FLUSH, 1);

	pica_viewport(0, 0, 240, pgl_state.render_target_active->width); 
	
	pica_scissor_test(pgl_state.scissor.enabled ? 0x3 : 0x0, pgl_state.scissor.y, pgl_state.scissor.x, pgl_state.scissor.y + pgl_state.scissor.height, pgl_state.scissor.x + pgl_state.scissor.width);

	pica_uniform_matf(0, (float*)&pgl_matrix_identity);
	pica_uniform_matf(4, (float*)&pgl_matrix_identity);

	pica_uniforms_bool(0x0000); //Colors are floats

	pica_depthmap(0, 1.0, 0);
	pica_logic_op(GPU_LOGICOP_COPY);
	GPUCMD_AddWrite(GPUREG_FRAGOP_ALPHA_TEST, 0);
	pica_depth_color_mask(true, GPU_ALWAYS, write_mask);
	pica_facecull_mode(GPU_CULL_NONE);

	GPUCMD_AddMaskedWrite(GPUREG_TEXENV_UPDATE_BUFFER, 0x5, 0x00); //Disable fog
	GPUCMD_AddIncrementalWrites(GPUREG_TEXENV0_SOURCE, (uint32_t*)&pgl_state.texenv[PGL_TEXENV_UNTEXTURED], 5);
	GPUCMD_AddIncrementalWrites(GPUREG_TEXENV1_SOURCE, (uint32_t*)&pgl_state.texenv[PGL_TEXENV_DUMMY], 5);

	pica_attribbuffers_format(0, 0XFF, 0x10, 2);
	pica_immediate_begin(GPU_TRIANGLE_STRIP);

	pica_fixed_attribute(-1.0,  1.0, pgl_state.clear_depth, 1.0f);
	pica_fixed_attribute(pgl_state.clear_color[0], pgl_state.clear_color[1], pgl_state.clear_color[2], pgl_state.clear_color[3]);
	pica_fixed_attribute(-1.0, -1.0, pgl_state.clear_depth, 1.0f);
	pica_fixed_attribute(pgl_state.clear_color[0], pgl_state.clear_color[1], pgl_state.clear_color[2], pgl_state.clear_color[3]);
	pica_fixed_attribute( 1.0,  1.0, pgl_state.clear_depth, 1.0f);
	pica_fixed_attribute(pgl_state.clear_color[0], pgl_state.clear_color[1], pgl_state.clear_color[2], pgl_state.clear_color[3]);
	pica_fixed_attribute( 1.0, -1.0, pgl_state.clear_depth, 1.0f);
	pica_fixed_attribute(pgl_state.clear_color[0], pgl_state.clear_color[1], pgl_state.clear_color[2], pgl_state.clear_color[3]);

	pica_immediate_end();

	pgl_state.current_mode = 0;
	pgl_state.changes = pgl_change_any;
}

void glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
	pgl_state.clear_color[0] = red;
	pgl_state.clear_color[1] = green;
	pgl_state.clear_color[2] = blue;
	pgl_state.clear_color[3] = alpha;
}

void glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
	uint8_t mask = (red & 0x1) | ((green & 0x1) << 1) | ((blue & 0x1) << 2) | ((alpha & 0x1) << 3);
	
	pgl_state.depth_test.write_mask = (pgl_state.depth_test.write_mask & 0x10) | mask;

	pgl_state.changes |= pgl_change_depthtest;
}

void glCullFace(GLenum mode)
{
	switch(mode)
	{
		case GL_FRONT:
			pgl_state.face_cull.mode = GPU_CULL_FRONT_CCW;
			break;
		default:
			pgl_state.face_cull.mode = GPU_CULL_BACK_CCW;
			break;
	}

	pgl_state.changes |= pgl_change_cull;
}

void glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
	if(pgl_state.render_target_active->width == 800)
	{
		x *= 2;
		width *= 2;
	}

	pgl_state.viewport.x = (pgl_state.render_target_active->width - width) - x;
	pgl_state.viewport.y = y;
	pgl_state.viewport.height = height;
	pgl_state.viewport.width = width;

	pgl_state.changes |= pgl_change_viewport;
}

void glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
	if(pgl_state.render_target_active->width == 800)
	{
		x *= 2;
		width *= 2;
	}

	pgl_state.scissor.x = (pgl_state.render_target_active->width - width) - x;
	pgl_state.scissor.y = y;
	pgl_state.scissor.width = width;
	pgl_state.scissor.height = height;

	pgl_state.changes |= pgl_change_scissor;
}