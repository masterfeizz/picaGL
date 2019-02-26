#include "internal.h"

static void _configAttribBuffer(uint8_t id, uint64_t format, uint8_t stride, uint8_t count, uint32_t padding)
{
	uint32_t param[2];

	if(id > 0xB) return;

	while(padding > 0)
	{
		if(padding < 4) 
		{
			format |= (0xB + padding) << (count * 4);
			padding = 0;
		}
		else 
		{
			format |= 0xF << (count * 4);
			padding -= 4;
		}
		count++;
	}


	param[0] = format & 0xFFFFFFFF;
	param[1] = (count << 28) | ((stride & 0xFF) << 16) | ((format >> 32) & 0xFFFF);

	GPUCMD_AddIncrementalWrites(GPUREG_ATTRIBBUFFER0_CONFIG1 + (id * 0x03), param, 0x02);
}

static void* _cacheArray(const void *data, GLsizei size)
{
	static uint32_t offset = 0;

	if(size > GEOMETRY_BUFFER_SIZE)
		return NULL;

	if(offset + size > GEOMETRY_BUFFER_SIZE)
		offset = 0;

	void *bufferLoc = pglState->geometryBuffer + offset;

	if(data != NULL)
		memcpy(bufferLoc, data, size);

	offset += size;

	return bufferLoc;
}

void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer)
{
	switch(type)
	{
		case GL_SHORT:
			pglState->vertexArrayPointer.type = GPU_SHORT;
			pglState->vertexArrayPointer.stride = size * 2;
			break;
		case GL_FLOAT:
			pglState->vertexArrayPointer.type = GPU_FLOAT;
			pglState->vertexArrayPointer.stride = size * 4;
			break;
		default:
			printf("%s: unimplemented type: %i\n", __FUNCTION__, type);
			break;
	}

	if(stride)
	{
		pglState->vertexArrayPointer.padding = (stride - pglState->vertexArrayPointer.stride) / 4;
		pglState->vertexArrayPointer.stride = stride;
	}

	pglState->vertexArrayPointer.pointer = pointer;
	pglState->vertexArrayPointer.size = size;
}

void glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer)
{
	switch(type)
	{
		case GL_BYTE:
			pglState->colorArrayPointer.type = GPU_BYTE;
			pglState->colorArrayPointer.stride = size * 1;
			break;
		case GL_UNSIGNED_BYTE:
			pglState->colorArrayPointer.type = GPU_UNSIGNED_BYTE;
			pglState->colorArrayPointer.stride = size * 1;
			break;
		case GL_SHORT:
			pglState->colorArrayPointer.type = GPU_SHORT;
			pglState->colorArrayPointer.stride = size * 2;
			break;
		case GL_FLOAT:
			pglState->colorArrayPointer.type = GPU_FLOAT;
			pglState->colorArrayPointer.stride = size * 4;
			break;
		default:
			printf("%s: unimplemented type: %i\n", __FUNCTION__, type);
			return;
	}

	if(stride)
	{
		pglState->colorArrayPointer.padding = (stride - pglState->colorArrayPointer.stride) / 4;
		pglState->colorArrayPointer.stride  = stride;
	}

	pglState->colorArrayPointer.pointer = pointer;
	pglState->colorArrayPointer.size = size;
}

void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer)
{
	AttribPointer *texcoord_array = &pglState->texCoordArrayPointer[pglState->texUnitActiveClient];

	switch(type)
	{
		case GL_SHORT:
			texcoord_array->type = GPU_SHORT;
			texcoord_array->stride = size * 2;
			break;
		case GL_FLOAT:
			texcoord_array->type = GPU_FLOAT;
			texcoord_array->stride = size * sizeof(GLfloat);
			break;
		default:
			printf("%s: unimplemented type: %i\n", __FUNCTION__, type);
			return;
	}

	if(stride)
	{
		texcoord_array->padding = (stride - texcoord_array->stride) / 4;
		texcoord_array->stride = stride;
	}

	texcoord_array->pointer = pointer;
	texcoord_array->size = size;
}

void glEnableClientState(GLenum array)
{
	switch(array)
	{
		case GL_VERTEX_ARRAY:
			pglState->vertexArrayState = GL_TRUE;
			break;
		case GL_COLOR_ARRAY:
			pglState->colorArrayState = GL_TRUE;
			break;
		case GL_TEXTURE_COORD_ARRAY:
			pglState->texCoordArrayState[pglState->texUnitActive] = GL_TRUE;
			break;
	}
}

void glDisableClientState(GLenum array)
{
	switch(array)
	{
		case GL_VERTEX_ARRAY:
			pglState->vertexArrayState = GL_FALSE;
			break;
		case GL_COLOR_ARRAY:
			pglState->colorArrayState = GL_FALSE;
			break;
		case GL_TEXTURE_COORD_ARRAY:
			pglState->texCoordArrayState[pglState->texUnitActive] = GL_FALSE;
			break;
	}
}

void glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
	uint8_t  buffer_count 			= 0;
	uint16_t attributes_fixed_mask 	= 0xFFF;
	uint64_t attributes_format 		= 0x00;
	uint64_t attributes_permutation = 0x3210;

	AttribPointer *vertex_array 	= &pglState->vertexArrayPointer;
	AttribPointer *color_array 		= &pglState->colorArrayPointer;
	AttribPointer *texcoord_array 	=  pglState->texCoordArrayPointer;

	void *array_cache;

	if(pglState->vertexArrayState == GL_FALSE)
		return;

	pglState_flush();

	array_cache = _cacheArray(vertex_array->pointer, vertex_array->stride * count);

	attributes_format |= GPU_ATTRIBFMT(0, vertex_array->size, vertex_array->type);
	_picaAttribBufferOffset(buffer_count, (uint32_t)array_cache - __ctru_linear_heap);
	_configAttribBuffer(buffer_count, 0x0, vertex_array->stride, 1, vertex_array->padding);

	buffer_count++;

	if(pglState->colorArrayState == GL_FALSE)
	{
		attributes_fixed_mask = 1 << 1;

		GPUCMD_AddWrite(GPUREG_FIXEDATTRIB_INDEX, 1);
		_picaFixedAttribute(pglState->currentColor.r, pglState->currentColor.g, pglState->currentColor.b, pglState->currentColor.a);
	}
	else
	{
		array_cache = _cacheArray(color_array->pointer, color_array->stride * count);

		attributes_format |= GPU_ATTRIBFMT(1, color_array->size, color_array->type);
		_picaAttribBufferOffset(buffer_count, (uint32_t)array_cache - __ctru_linear_heap);
		_configAttribBuffer(buffer_count, 0x1, color_array->stride, 1, color_array->padding);

		buffer_count++;
	}

	if(pglState->texCoordArrayState[0] == GL_FALSE)
	{
		attributes_fixed_mask = 1 << 2;

		GPUCMD_AddWrite(GPUREG_FIXEDATTRIB_INDEX, 2);
		_picaFixedAttribute(1, 1, 0, 0);
	}
	else
	{
		array_cache = _cacheArray(texcoord_array[0].pointer, texcoord_array[0].stride * count);

		attributes_format |= GPU_ATTRIBFMT(2, texcoord_array[0].size, texcoord_array[0].type);
		_picaAttribBufferOffset(buffer_count, (uint32_t)array_cache - __ctru_linear_heap);
		_configAttribBuffer(buffer_count, 0x2, texcoord_array[0].stride, 1, texcoord_array[0].padding);

		buffer_count++;
	}

	if(pglState->texCoordArrayState[1] == GL_FALSE && pglState->texUnitState[1] == GL_FALSE)
	{
		attributes_fixed_mask = 1 << 3;

		GPUCMD_AddWrite(GPUREG_FIXEDATTRIB_INDEX, 3);
		_picaFixedAttribute(1, 1, 0, 0);
	}
	else
	{
		array_cache = _cacheArray(texcoord_array[1].pointer, texcoord_array[1].stride * count);

		attributes_format |= GPU_ATTRIBFMT(3, texcoord_array[1].size, texcoord_array[1].type);
		_picaAttribBufferOffset(buffer_count, (uint32_t)array_cache - __ctru_linear_heap);
		_configAttribBuffer(buffer_count, 0x3, texcoord_array->stride, 1, texcoord_array->padding);

		buffer_count++;
	}

	_picaAttribBuffersFormat(attributes_format, attributes_fixed_mask, attributes_permutation, 4);

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

	_picaDrawArray(primitive_type, first, count);
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
	if(type != GL_UNSIGNED_SHORT)
		return;

	uint32_t  vertex_count = 0;

	for(int i = 0; i < count; i++)
	{
			if(vertex_count < ((GLushort*)indices)[i])
				vertex_count = ((GLushort*)indices)[i];
	}

	vertex_count++;

	glDrawRangeElements( mode, 0, vertex_count, count, type, indices );
}

void glDrawRangeElements( GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices )
{
	uint8_t  buffer_count 			= 0;
	uint16_t attributes_fixed_mask 	= 0xFFF;
	uint64_t attributes_format 		= 0x00;
	uint64_t attributes_permutation = 0x3210;

	AttribPointer *vertex_array 	= &pglState->vertexArrayPointer;
	AttribPointer *color_array 		= &pglState->colorArrayPointer;
	AttribPointer *texcoord_array 	= pglState->texCoordArrayPointer;

	void *array_cache;

	if(type != GL_UNSIGNED_SHORT)
		return;

	uint16_t* index_array  = _cacheArray(indices, sizeof(uint16_t) * count);


	if(pglState->vertexArrayState == GL_FALSE)
		return;

	pglState_flush();

	array_cache = _cacheArray(vertex_array->pointer, vertex_array->stride * end);

	attributes_format |= GPU_ATTRIBFMT(0, vertex_array->size, vertex_array->type);
	_picaAttribBufferOffset(buffer_count, (uint32_t)array_cache - __ctru_linear_heap);
	_configAttribBuffer(buffer_count, 0x0, vertex_array->stride, 1, vertex_array->padding);

	buffer_count++;

	if(pglState->colorArrayState == GL_FALSE)
	{
		attributes_fixed_mask = 1 << 1;

		GPUCMD_AddWrite(GPUREG_FIXEDATTRIB_INDEX, 1);
		_picaFixedAttribute(pglState->currentColor.r, pglState->currentColor.g, pglState->currentColor.b, pglState->currentColor.a);
	}
	else
	{
		array_cache = _cacheArray(color_array->pointer, color_array->stride * end);

		attributes_format |= GPU_ATTRIBFMT(1, color_array->size, color_array->type);
		_picaAttribBufferOffset(buffer_count, (uint32_t)array_cache - __ctru_linear_heap);
		_configAttribBuffer(buffer_count, 0x1, color_array->stride, 1, color_array->padding);

		buffer_count++;
	}

	if(pglState->texCoordArrayState[0] == GL_FALSE)
	{
		attributes_fixed_mask = 1 << 2;

		GPUCMD_AddWrite(GPUREG_FIXEDATTRIB_INDEX, 2);
		_picaFixedAttribute(1, 1, 0, 0);
	}
	else
	{
		array_cache = _cacheArray(texcoord_array[0].pointer, texcoord_array[0].stride * end);

		attributes_format |= GPU_ATTRIBFMT(2, texcoord_array[0].size, texcoord_array[0].type);
		_picaAttribBufferOffset(buffer_count, (uint32_t)array_cache - __ctru_linear_heap);
		_configAttribBuffer(buffer_count, 0x2, texcoord_array[0].stride, 1, texcoord_array[0].padding);

		buffer_count++;
	}

	if(pglState->texCoordArrayState[1] == GL_FALSE && pglState->texUnitState[1] == GL_FALSE)
	{
		attributes_fixed_mask = 1 << 3;

		GPUCMD_AddWrite(GPUREG_FIXEDATTRIB_INDEX, 3);
		_picaFixedAttribute(1, 1, 0, 0);
	}
	else
	{
		array_cache = _cacheArray(texcoord_array[1].pointer, texcoord_array[1].stride * end);

		attributes_format |= GPU_ATTRIBFMT(3, texcoord_array[1].size, texcoord_array[1].type);
		_picaAttribBufferOffset(buffer_count, (uint32_t)array_cache - __ctru_linear_heap);
		_configAttribBuffer(buffer_count, 0x3, texcoord_array->stride, 1, texcoord_array->padding);

		buffer_count++;
	}

	_picaAttribBuffersFormat(attributes_format, attributes_fixed_mask, attributes_permutation, 4);


	GPU_Primitive_t primitive_type;

	switch(mode)
	{
		case GL_TRIANGLES: 
			primitive_type = GPU_GEOMETRY_PRIM; 
			break;
		case GL_TRIANGLE_FAN:
			primitive_type = GPU_TRIANGLE_FAN; 
			break;
		case GL_TRIANGLE_STRIP:
			primitive_type = GPU_TRIANGLE_STRIP;
			break;
		default:
			primitive_type = GPU_GEOMETRY_PRIM;
	}
	
	_picaDrawElements(primitive_type, (uint32_t)index_array - __ctru_linear_heap, count);
}