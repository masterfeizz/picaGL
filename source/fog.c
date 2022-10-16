#include "internal.h"

static inline float pgl_calc_z(float depth, float near, float far)
{
	return far * near / (depth * (far - near) + near);
}

static void pgl_generate_lut()
{
	float data[256];
	float x, val;

	for (int i = 0; i <= 128; i++)
	{
		//This should be calculated based on the perspective matrix
		x = pgl_calc_z(i / 128.0f, 0.1f, 2000.0f);

		if(pgl_state.fog.mode == GL_LINEAR)
		{
			if(x < pgl_state.fog.near)
				val = 1.0f;
			else if(x > pgl_state.fog.far)
				val = 0.0f;
			else
				val = (pgl_state.fog.far - x) / (pgl_state.fog.far - pgl_state.fog.near);
		}
		else if(pgl_state.fog.mode == GL_EXP)
		{
			val = expf(-pgl_state.fog.density * x);
		}
		else
			val = -1.0f;

		if (i < 128)
			data[i] = val;
		if (i > 0)
			data[i + 127] = val - data[i-1];
	}

	for (int i = 0; i < 128; i ++)
	{
		float in = data[i], diff = data[i+128];

		u32 val = 0;
		if (in > 0.0f)
		{
			in *= 0x800;
			val = (in < 0x800) ? (u32)in : 0x7FF;
		}

		u32 val2 = 0;
		if (diff != 0.0f)
		{
			diff *= 0x800;
			if (diff < -0x1000) diff = -0x1000;
			else if (diff > 0xFFF) diff = 0xFFF;
			val2 = (s32)diff & 0x1FFF;
		}

		pgl_state.fog.lut[i] = val2 | (val << 13);
	}
}

void glFogf(GLenum pname, GLfloat param)
{
	switch (pname) {
		case GL_FOG_MODE:
			if(pgl_state.fog.mode == param)
				return;
			pgl_state.fog.mode = param;
			break;
		case GL_FOG_DENSITY:
			if(pgl_state.fog.density == param)
				return;
			pgl_state.fog.density = param;
			break;
		case GL_FOG_START:
			if(pgl_state.fog.near == param)
				return;
			pgl_state.fog.near = param;
			break;
		case GL_FOG_END:
			if(pgl_state.fog.far == param)
				return;
			pgl_state.fog.far = param;
			break;
		default:
			return;
	}
	
	pgl_generate_lut();

	pgl_state.changes |= pglDirtyFlag_Fog;
}

void glFogi(GLenum pname, GLint param)
{
	glFogf(pname, (GLfloat)param);
}

void glFogfv(GLenum pname, const GLfloat *params)
{
	switch (pname) {
		case GL_FOG_COLOR:
			uint8_t r = (uint8_t)(params[0] * 255);
			uint8_t g = (uint8_t)(params[1] * 255);
			uint8_t b = (uint8_t)(params[2] * 255);
			uint8_t a = (uint8_t)(params[3] * 255);

			pgl_state.fog.color = r | (g << 8) | (b << 16) | (a << 24);

			break;
		default:
			return;
	}
}