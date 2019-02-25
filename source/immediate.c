#include "internal.h"

void glBegin(GLenum mode)
{
	GPU_Primitive_t primitive_type;

	switch(mode)
	{
		case GL_TRIANGLES: 
			primitive_type = GPU_TRIANGLES; 
			break;
		case GL_TRIANGLE_FAN:
			primitive_type = GPU_TRIANGLE_FAN; 
			break;
		case GL_TRIANGLE_STRIP:
			primitive_type = GPU_TRIANGLE_STRIP;
			break;
		default:
			primitive_type = GPU_TRIANGLE_FAN;
	}

	pglState_flush();

	if(pglState->texUnitState[1])
		_picaAttribBuffersFormat(0, 0XFFFF, 0x3210, 4);
	else
		_picaAttribBuffersFormat(0, 0XFFFF, 0x210, 3);

	_picaImmediateBegin(primitive_type);
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
	pglState->currentColor.r = red;
	pglState->currentColor.g = green;
	pglState->currentColor.b = blue;
	pglState->currentColor.a = alpha;
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
	pglState->currentTexCoord[0].s = s;
	pglState->currentTexCoord[0].t = t;
}

void glTexCoord2fv(const GLfloat *v)
{
	glTexCoord2f(v[0], v[1]);
}

void glVertex2f(GLfloat x, GLfloat y)
{
	glVertex3f(x, y, 0);
}

void glVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
	_picaFixedAttribute(x, y, z, 0);
	_picaFixedAttribute(pglState->currentColor.r, pglState->currentColor.g, pglState->currentColor.b, pglState->currentColor.a);
	_picaFixedAttribute(pglState->currentTexCoord[0].s, pglState->currentTexCoord[0].t, 0, 0);

	if(pglState->texUnitState[1])
		_picaFixedAttribute(pglState->currentTexCoord[1].s, pglState->currentTexCoord[1].t, 0, 0);
}

void glVertex3fv(const GLfloat* v)
{
	glVertex3f(v[0], v[1], v[2]);
}

void glEnd(void)
{
	_picaImmediateEnd();
}

void glMultiTexCoord2f( GLenum target, GLfloat s, GLfloat t )
{
	uint8_t texunit = target & 0x01;

	pglState->currentTexCoord[texunit].s = s;
	pglState->currentTexCoord[texunit].t = t;
}
