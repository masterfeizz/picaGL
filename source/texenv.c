#include "internal.h"

void glTexEnvfv( GLenum target, GLenum pname, const GLfloat *params )
{
	if ((target != GL_TEXTURE_ENV) || (pname != GL_TEXTURE_ENV_COLOR))
		return;

	uint8_t texunit = pgl_state.texunit_active;
	pgl_texenv_t *texenv  = &pgl_state.texenv[texunit];

	uint8_t r = (uint8_t)(params[0] * 255);
	uint8_t g = (uint8_t)(params[1] * 255);
	uint8_t b = (uint8_t)(params[2] * 255);
	uint8_t a = (uint8_t)(params[3] * 255);

	texenv->color = r | (g << 8) | (b << 16) | (a << 24);
	
	pgl_state.changes |= pgl_change_texenv;
}

inline void glTexEnvi (GLenum target, GLenum pname, GLint param)
{
	if ((target != GL_TEXTURE_ENV) || (pname != GL_TEXTURE_ENV_MODE))
		return;
	
	uint8_t texunit = pgl_state.texunit_active;
	pgl_texenv_t *texenv  = &pgl_state.texenv[texunit];

	switch (param)
	{
	case GL_ADD:
		texenv->func_rgb = GPU_ADD;
		texenv->src_rgb  = GPU_TEVSOURCES(GPU_TEXTURE0 + texunit, texunit == 0 ? GPU_PRIMARY_COLOR : GPU_PREVIOUS, 0);
		texenv->op_rgb   = GPU_TEVOPERANDS(0, 0, 0);

		texenv->func_alpha = GPU_MODULATE;
		texenv->src_alpha  = GPU_TEVSOURCES(GPU_TEXTURE0 + texunit, texunit == 0 ? GPU_PRIMARY_COLOR : GPU_PREVIOUS, 0);
		texenv->op_alpha   = GPU_TEVOPERANDS(0, 0, 0);
		break;

	case GL_REPLACE:
		texenv->func_rgb = GPU_REPLACE;
		texenv->src_rgb  = GPU_TEVSOURCES(GPU_TEXTURE0 + texunit, 0, 0);
		texenv->op_rgb   = GPU_TEVOPERANDS(0, 0, 0);

		texenv->func_alpha = GPU_REPLACE;
		texenv->src_alpha  = GPU_TEVSOURCES(GPU_TEXTURE0 + texunit, 0, 0);
		texenv->op_alpha   = GPU_TEVOPERANDS(0, 0, 0);
		break;

	case GL_MODULATE:
		texenv->func_rgb = GPU_MODULATE;
		texenv->src_rgb  = GPU_TEVSOURCES(GPU_TEXTURE0 + texunit, texunit == 0 ? GPU_PRIMARY_COLOR : GPU_PREVIOUS, 0);
		texenv->op_rgb   = GPU_TEVOPERANDS(0, 0, 0);

		texenv->func_alpha = GPU_MODULATE;
		texenv->src_alpha  = GPU_TEVSOURCES(GPU_TEXTURE0 + texunit, texunit == 0 ? GPU_PRIMARY_COLOR : GPU_PREVIOUS, 0);
		texenv->op_alpha   = GPU_TEVOPERANDS(0, 0, 0);
		break;

	case GL_DECAL:
		texenv->func_rgb = GPU_INTERPOLATE;
		texenv->src_rgb  = GPU_TEVSOURCES(GPU_TEXTURE0 + texunit, texunit == 0 ? GPU_PRIMARY_COLOR : GPU_PREVIOUS, GPU_TEXTURE0 + texunit);
		texenv->op_rgb   = GPU_TEVOPERANDS(0, 0, GPU_TEVOP_RGB_SRC_ALPHA);

		texenv->func_alpha = GPU_REPLACE;
		texenv->src_alpha  = GPU_TEVSOURCES(GPU_PRIMARY_COLOR, 0, 0);
		texenv->op_alpha   = GPU_TEVOPERANDS(0, 0, 0);
		break;

	case GL_BLEND:
		texenv->func_rgb = GPU_INTERPOLATE;
		texenv->src_rgb  = GPU_TEVSOURCES(GPU_CONSTANT, texunit == 0 ? GPU_PRIMARY_COLOR : GPU_PREVIOUS,  GPU_TEXTURE0 + texunit);
		texenv->op_rgb   = GPU_TEVOPERANDS(0, 0, 0);

		texenv->func_alpha = GPU_MODULATE;
		texenv->src_alpha  = GPU_TEVSOURCES(GPU_TEXTURE0 + texunit, texunit == 0 ? GPU_PRIMARY_COLOR : GPU_PREVIOUS, 0);
		texenv->op_alpha   = GPU_TEVOPERANDS(0, 0, 0);
		break;

	default:
		break;
	}

	pgl_state.changes |= pgl_change_texenv;
}

void glTexEnvf (GLenum target, GLenum pname, GLfloat param)
{
	glTexEnvi (target, pname, (int)param);
}