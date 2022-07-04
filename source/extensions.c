#include "internal.h"

void glActiveTextureARB (GLenum texture)
{
	glActiveTexture(texture);
}

void glClientActiveTextureARB ( GLenum texture )
{
	glClientActiveTexture(texture);
}

void glMultiTexCoord2fARB( GLenum target, GLfloat s, GLfloat t )
{
	glMultiTexCoord2f( target, s, t );
}