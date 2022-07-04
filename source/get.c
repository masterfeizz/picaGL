#include "internal.h"

const GLubyte* glGetString(GLenum name)
{
	switch(name)
	{
		case GL_RENDERER:
			return (GLubyte*)"DMP(R) PICA200";
		case GL_VERSION:
			return (GLubyte*)"1.1";
		case GL_VENDOR:
			return (GLubyte*)"MasterFeizz";
		case GL_EXTENSIONS:
			return (GLubyte*)"GL_ARB_multitexture";
		default: 
			return (GLubyte*)"";
	}
}

void glGetIntegerv(GLenum pname, GLint *params)
{
	switch(pname) 
	{
		case GL_MAX_TEXTURE_SIZE:
			*params = 128; break;
		case GL_MAX_TEXTURE_UNITS_ARB:
			*params = 2; break;
	}
}

void glGetFloatv(GLenum pname, GLfloat* params)
{
	switch(pname) 
	{
		case GL_MODELVIEW_MATRIX:
		{
			for(int i = 0; i < 4; i++)
			{
				mat4f *matrix = &pgl_state.matrix_stack[0][ pgl_state.matrix_stack_index[0] ];

				params[0  + i] = matrix->row[i].x;
				params[4  + i] = matrix->row[i].y;
				params[8  + i] = matrix->row[i].z;
				params[12 + i] = matrix->row[i].w;
			}
			break;
		}
			
	}
}