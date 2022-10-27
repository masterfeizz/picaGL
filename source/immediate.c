#include "internal.h"

static bool in_begin = false;

typedef struct {

	float x, y, z;
	float r, g, b, a;
	float s, t;
	float s2, t2;

} pgl_vertex_t;

static pgl_vertex_t vertex;
static size_t       vertex_count;

static GPU_Primitive_t primitive_type;

void glBegin(GLenum mode)
{
	if(mode < GL_TRIANGLES || mode > GL_POLYGON) return;

	if( (pgl_state.vertex_cache_pos + 0xF00) > pgl_state.vertex_cache_size )
	{
		//glFlush();
		pgl_state.vertex_cache_pos = 0;
	}

	switch(mode)
	{
		case GL_TRIANGLES: primitive_type      = GPU_TRIANGLES; break;
		case GL_TRIANGLE_FAN: primitive_type   = GPU_TRIANGLE_FAN; break;
		case GL_TRIANGLE_STRIP: primitive_type = GPU_TRIANGLE_STRIP; break;
		default: primitive_type                = GPU_TRIANGLE_FAN;
	}

	pgl_state_flush();

	if(pgl_state.current_mode != PGL_IMMEDIATE)
	{
		GPUCMD_AddWrite(GPUREG_ATTRIBBUFFERS_FORMAT_LOW,  0x77FB);
		GPUCMD_AddWrite(GPUREG_ATTRIBBUFFERS_FORMAT_HIGH, 3 << 28);

		GPUCMD_AddWrite(GPUREG_VSH_ATTRIBUTES_PERMUTATION_LOW, 0x3210);

		GPUCMD_AddMaskedWrite(GPUREG_VSH_INPUTBUFFER_CONFIG, 0xB, 0xA0000003);
		GPUCMD_AddWrite(GPUREG_VSH_NUM_ATTR, 3);

		GPUCMD_AddWrite(GPUREG_ATTRIBBUFFER0_CONFIG1, 0x3210);
		GPUCMD_AddWrite(GPUREG_ATTRIBBUFFER0_CONFIG2, (sizeof(pgl_vertex_t) << 16) | (4 << 28));

		GPUCMD_AddWrite(GPUREG_ATTRIBBUFFER1_CONFIG2, 0);
		GPUCMD_AddWrite(GPUREG_ATTRIBBUFFER2_CONFIG2, 0);
		GPUCMD_AddWrite(GPUREG_ATTRIBBUFFER3_CONFIG2, 0);

		pica_uniforms_bool(0x0000);

		pgl_state.current_mode = PGL_IMMEDIATE;
	}

	GPUCMD_AddWrite(GPUREG_ATTRIBBUFFER0_OFFSET, (uint32_t)(pgl_state.vertex_cache + pgl_state.vertex_cache_pos) - __ctru_linear_heap);

	vertex_count = 0;

	in_begin = true;
}

void glColor3f(GLfloat red, GLfloat green, GLfloat blue)
{
	glColor4f(red, green, blue, 1.0f);
}

void glColor3ubv(const GLubyte* v)
{
	glColor4f((1.0f/255.0f)*v[0], (1.0f/255.0f)*v[1], (1.0f/255.0f)*v[2], 1.0f);
}

void glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	vertex.r = red;
	vertex.g = green;
	vertex.b = blue;
	vertex.a = alpha;

	if(in_begin == false)
	{
		GPUCMD_AddWrite(GPUREG_FIXEDATTRIB_INDEX, 1);
		pica_fixed_attribute(red, green, blue, alpha);
	}
}

void glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
	glColor4f((1.0f/255)*red, (1.0f/255)*green, (1.0f/255)*blue, (1.0f/255)*alpha);
}

void glColor4ubv(const GLubyte* v)
{
	glColor4f((1.0f/255)*v[0], (1.0f/255)*v[1], (1.0f/255)*v[2], (1.0f/255)*v[3]);
}

void glColor4fv(const GLfloat* v)
{
	glColor4f(v[0], v[1], v[2], v[3]);
}

void glTexCoord2f(GLfloat s, GLfloat t)
{
	vertex.s = s;
	vertex.t = t;
}

void glTexCoord2fv(const GLfloat *v)
{
	glTexCoord2f(v[0], v[1]);
}

inline void glVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
	if(in_begin == false) return;

	vertex.x = x;
	vertex.y = y;
	vertex.z = z;

	pgl_vertex_t *cached_vertex = (pgl_vertex_t*)(pgl_state.vertex_cache + pgl_state.vertex_cache_pos);

	*cached_vertex = vertex;

	vertex_count += 1;

	pgl_state.vertex_cache_pos += sizeof(pgl_vertex_t);

	if((vertex_count % 84) == 0)
	{
		if( (pgl_state.vertex_cache_pos + 0xF00) > pgl_state.vertex_cache_size )
		{
			glEnd();
			glBegin(GL_TRIANGLES + (primitive_type >> 8));
		}
	}
}

void glVertex3fv(const GLfloat* v)
{
	glVertex3f(v[0], v[1], v[2]);
}

void glVertex2f(GLfloat x, GLfloat y)
{
	glVertex3f(x, y, 0);
}

void glEnd(void)
{
	if(in_begin == false) return;

	in_begin = false;

	if(vertex_count < 3) return;

	pica_draw_arrays(primitive_type, 0, vertex_count);
	
	if(++pgl_state.batched_draws > MAX_BATCHED_DRAWS)
		pgl_queue_commands(true);
}

void glMultiTexCoord2f( GLenum target, GLfloat s, GLfloat t )
{
	if(target & 1)
	{
		vertex.s2 = s;
		vertex.t2 = t;
	}
	else
	{
		vertex.s = s;
		vertex.t = t;
	}
}

void glMultiTexCoord2fv( GLenum target, const GLfloat *v )
{
	glMultiTexCoord2f(target, v[0], v[1]);
}

void glArrayElement(GLint i)
{
/*	const void *vertexPointer  = pgl_state.vertexArrayPointer.pointer;
	const void *colorPointer   = pgl_state.colorArrayPointer.pointer;
	const void *texCoordPointer1 = pgl_state.texCoordArrayPointer[0].pointer;
	const void *texCoordPointer2 = pgl_state.texCoordArrayPointer[1].pointer;

	if(pgl_state.colorArrayState == GL_TRUE)
		glColor4ubv(colorPointer + (i * pgl_state.colorArrayPointer.stride));
	if(pgl_state.texCoordArrayState[0] == GL_TRUE)
		glTexCoord2fv(texCoordPointer1 + (i * pgl_state.texCoordArrayPointer[0].stride));
	if(pgl_state.texCoordArrayState[1] == GL_TRUE)
		glMultiTexCoord2fv(GL_TEXTURE1, texCoordPointer2 + (i * pgl_state.texCoordArrayPointer[0].stride));

	glVertex3fv(vertexPointer + (i * pgl_state.vertexArrayPointer.stride));*/
}