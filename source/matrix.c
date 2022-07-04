#include "internal.h"

const mat4f pgl_matrix_identity = {{
    { { 0.f, 0.f, 0.f, 1.f } },
    { { 0.f, 0.f, 1.f, 0.f } },
    { { 0.f, 1.f, 0.f, 0.f } },
    { { 1.f, 0.f, 0.f, 0.f } },
}};

static inline void mat4f_multiply(mat4f* out, const mat4f* a, const mat4f* b)
{
	for (size_t i = 0; i < 4; i ++)
	{
		for (size_t j = 0; j < 4; j ++)
		{
			out->row[j].column[i] = a->row[j].x * b->row[0].column[i] +
									a->row[j].y * b->row[1].column[i] +
									a->row[j].z * b->row[2].column[i] +
									a->row[j].w * b->row[3].column[i];
		}
	}
}

void glLoadIdentity(void)
{
	*pgl_state.matrix_current = pgl_matrix_identity;

	pgl_state.changes |= (pglDirtyFlag_Matrix << (pgl_state.matrix_mode));
}

void glTranslatef(GLfloat x,  GLfloat y,  GLfloat z)
{
	static mat4f trans =  pgl_matrix_identity;
	
	mat4f tmp = *pgl_state.matrix_current;

	trans.row[0].w = x;
	trans.row[1].w = y;
	trans.row[2].w = z;

	mat4f_multiply(pgl_state.matrix_current, &tmp, &trans);

	pgl_state.changes |= (pglDirtyFlag_Matrix << (pgl_state.matrix_mode));
}

void glScalef(GLfloat x, GLfloat y, GLfloat z)
{
	mat4f *matrix = pgl_state.matrix_current;

	for (size_t i = 0; i < 4; i ++)
	{
		matrix->row[i].x *= x;
		matrix->row[i].y *= y;
		matrix->row[i].z *= z;
	}

	pgl_state.changes |= (pglDirtyFlag_Matrix << (pgl_state.matrix_mode));
}

void glRotatef(GLfloat angle,  GLfloat x,  GLfloat y,  GLfloat z)
{
	if(angle == 0) return;

	mat4f *matrix = pgl_state.matrix_current;
	mat4f om;

	//angle is in degrees
	angle = angle * (M_PI / 180.0f);

	float s = sinf(angle);
	float c = cosf(angle);
	float t = 1.0f - c;

	//FIXME: we assume vec(x, y, z) is normalized

	om.row[0].x = t * x * x + c;
	om.row[1].x = t * x * y + s * z;
	om.row[2].x = t * x * z - s * y;

	om.row[0].y = t * y * x - s * z;
	om.row[1].y = t * y * y + c;
	om.row[2].y = t * y * z + s * x;

	om.row[0].z = t*z*x + s*y;
	om.row[1].z = t*z*y - s*x;
	om.row[2].z = t*z*z + c;

	for (size_t i = 0; i < 4; ++i)
	{
		x = matrix->row[i].x * om.row[0].x + matrix->row[i].y * om.row[1].x + matrix->row[i].z * om.row[2].x;
		y = matrix->row[i].x * om.row[0].y + matrix->row[i].y * om.row[1].y + matrix->row[i].z * om.row[2].y;
		z = matrix->row[i].x * om.row[0].z + matrix->row[i].y * om.row[1].z + matrix->row[i].z * om.row[2].z;

		matrix->row[i].x = x;
		matrix->row[i].y = y;
		matrix->row[i].z = z;
	}

	pgl_state.changes |= (pglDirtyFlag_Matrix << (pgl_state.matrix_mode));
}

void glMatrixMode(GLenum mode)
{
	if(mode == GL_MODELVIEW)
		pgl_state.matrix_mode = 0;
	else
		pgl_state.matrix_mode = 1;

	size_t index = pgl_state.matrix_stack_index[pgl_state.matrix_mode];

	pgl_state.matrix_current = &pgl_state.matrix_stack[pgl_state.matrix_mode][index];
}

void glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near, GLdouble far)
{
	mat4f *matrix = pgl_state.matrix_current;

	memset(matrix, 0, sizeof(mat4f));

	// Build standard orthogonal projection matrix
	matrix->row[1].x = 2.0f / (left - right);
	matrix->row[1].w = (left + right) / (right - left);
	matrix->row[0].y = 2.0f / (top - bottom);
	matrix->row[0].w = (bottom + top) / (bottom - top);
	matrix->row[2].z = 2.0f / (near - far);
	matrix->row[2].w = (far + near) / (far - near);
	matrix->row[3].w = 1.0f;

	//Adjust depth range from [-1, 1] to [-1, 0]
	matrix->row[2].z = (-matrix->row[2].z * 0.5f);
	matrix->row[2].w = (-matrix->row[2].w * 0.5f) - 0.5f;

	pgl_state.changes |= (pglDirtyFlag_Matrix << (pgl_state.matrix_mode));
}

void glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near, GLdouble far)
{
	mat4f *matrix = pgl_state.matrix_current;

	memset(matrix, 0, sizeof(mat4f));

	matrix->row[0].y = 2 * near / (top - bottom);
	matrix->row[0].z = (right + left) / (right - left);
	matrix->row[1].x = -2 * near / (right - left);
	matrix->row[1].z = (top + bottom) / (top - bottom);
	matrix->row[2].z = (far + near) / (far - near);
	matrix->row[2].w = (2 * far * near) / (far - near);
	matrix->row[3].z = -1.0f;

	//Adjust depth range from [-1, 1], to [-1, 0]
	matrix->row[2].z = (-matrix->row[2].z * 0.5f) + 0.5f;
	matrix->row[2].w = (-matrix->row[2].w * 0.5f);

	pgl_state.changes |= (pglDirtyFlag_Matrix << (pgl_state.matrix_mode));
}

void glPopMatrix(void)
{	
	if(pgl_state.matrix_stack_index[pgl_state.matrix_mode] == 0)
		return;
	
	size_t index = --pgl_state.matrix_stack_index[pgl_state.matrix_mode];

	pgl_state.matrix_current = &pgl_state.matrix_stack[pgl_state.matrix_mode][index];

	pgl_state.changes |= (pglDirtyFlag_Matrix << (pgl_state.matrix_mode));
}

void glPushMatrix(void)
{
	if(pgl_state.matrix_stack_index[pgl_state.matrix_mode] >= (MATRIX_STACK_SIZE - 1))
		return;

	size_t index = ++pgl_state.matrix_stack_index[pgl_state.matrix_mode];

	pgl_state.matrix_stack[pgl_state.matrix_mode][index] = pgl_state.matrix_stack[pgl_state.matrix_mode][index - 1];

	pgl_state.matrix_current = &pgl_state.matrix_stack[pgl_state.matrix_mode][index];
}

void glLoadMatrixf(const GLfloat* m)
{
	mat4f *matrix = pgl_state.matrix_current;
	
	for(int i = 0; i < 4; i++)
	{
		matrix->row[i].x = m[0 + i];
		matrix->row[i].y = m[4 + i];
		matrix->row[i].z = m[8 + i];
		matrix->row[i].w = m[12 + i];
	}

	//Fix projection matrix
	if(pgl_state.matrix_mode == 1)
	{
		//Flip X and Y
		vector4f row = matrix->row[1];
		matrix->row[1].x = -matrix->row[0].x;
		matrix->row[1].y = -matrix->row[0].y;
		matrix->row[1].z = -matrix->row[0].z;
		matrix->row[1].w = -matrix->row[0].w;
		matrix->row[0] = row;

		//Fix depth range
		matrix->row[2].x = 0.5f * (matrix->row[2].x - matrix->row[3].x);
		matrix->row[2].y = 0.5f * (matrix->row[2].y - matrix->row[3].y);
		matrix->row[2].z = 0.5f * (matrix->row[2].z - matrix->row[3].z);
		matrix->row[2].w = 0.5f * (matrix->row[2].w - matrix->row[3].w);
	}

	pgl_state.changes |= (pglDirtyFlag_Matrix << (pgl_state.matrix_mode));
}

void glMultMatrixf(const GLfloat *m)
{
	mat4f in, tmp;
	
	for(int i = 0; i < 4; i++)
	{
		in.row[i].x = m[0 + i];
		in.row[i].y = m[4 + i];
		in.row[i].z = m[8 + i];
		in.row[i].w = m[12 + i];
	}

	mat4f_multiply(&tmp, pgl_state.matrix_current, &in);

	*pgl_state.matrix_current = tmp;

	pgl_state.changes |= (pglDirtyFlag_Matrix << (pgl_state.matrix_mode));
}