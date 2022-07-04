#include "internal.h"

static uint64_t attribs_format[2]  = { 0, 0 };

static inline bool pgl_address_in_linear(const void* addr)
{
	u32 vaddr = (u32)addr;
	return (vaddr >= __ctru_linear_heap) && (vaddr < (__ctru_linear_heap + __ctru_linear_heap_size));
}

//Check if we have enough space in the vertex cache
static bool pgl_check_cache_limit(size_t size)
{
	if(pgl_state.vertex_cache_pos + size > VERTEX_BUFFER_SIZE)
		return false;
	else
		return true;
}

static void* pgl_cache_data(const void *data, size_t size)
{
	void *cache = pgl_state.vertex_cache + pgl_state.vertex_cache_pos;

	if(data != NULL) memcpy(cache, data, size);

	pgl_state.vertex_cache_pos += (size + 3) & ~3;

	return cache;
}

static uint64_t pgl_config_attr_buffer(uint64_t format, uint8_t stride, uint8_t count, uint32_t padding)
{
	uint32_t *config = (uint32_t*)&format;

	while(padding > 0)
	{
		if(padding < 4) {
			format |= (0xB + padding) << (count * 4);
			padding = 0;
		} else {
			format |= 0xF << (count * 4);
			padding -= 4;
		}
		count++;
	}

	config[1] |= (count << 28) | ((stride & 0xFF) << 16);

	return format;
}

static void pgl_config_attribute(GLint id, GLint size, GLenum type, GLsizei stride, const GLvoid * pointer)
{
	pgl_attrib_info_t *attrib = &pgl_state.vertex_attrib[id];

	switch(type)
	{
		case GL_BYTE:
			attrib->type = GPU_BYTE;
			attrib->stride = size * 1;
			break;
		case GL_UNSIGNED_BYTE:
			attrib->type = GPU_UNSIGNED_BYTE;
			attrib->stride = size * 1;
			break;
		case GL_SHORT:
			attrib->type = GPU_SHORT;
			attrib->stride = size * 2;
			break;
		case GL_FLOAT:
			attrib->type = GPU_FLOAT;
			attrib->stride = size * 4;
			break;
	}

	if(stride)
	{
		attrib->padding = (stride - attrib->stride) / 4;
		attrib->stride  = stride;
	}
	else
		attrib->padding = 0;

	attrib->pointer = pointer;
	attrib->size = size;

	if( pgl_address_in_linear(pointer) )
	{
		attrib->cached_pointer = pointer;
		attrib->cached_len = 0xFFFFFFFF;
	}
	else
	{
		attrib->cached_pointer = 0;
		attrib->cached_len = 0;
	}

	attrib->buffer_config = pgl_config_attr_buffer(id, attrib->stride, 1, attrib->padding);

	attribs_format[0] &= ~(0xF << (id * 4));
	attribs_format[0] |= GPU_ATTRIBFMT(id, attrib->size, attrib->type);
}

void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer)
{
	if( (type != GL_SHORT) && (type != GL_FLOAT) ) return;

	pgl_config_attribute(0, size, type, stride, pointer);
}

void glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer)
{
	if( (type != GL_SHORT) && (type != GL_FLOAT) && (type != GL_UNSIGNED_BYTE) && (type != GL_BYTE) ) return;

	pgl_config_attribute(1, size, type, stride, pointer);

}

void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer)
{
	if( (type != GL_SHORT) && (type != GL_FLOAT) ) return;

	pgl_config_attribute(2 + pgl_state.texunit_active_client, size, type, stride, pointer);
}

void glEnableClientState(GLenum array)
{
	switch(array)
	{
		case GL_VERTEX_ARRAY:
			attribs_format[1] &= ~( 1 << 16 );
			pgl_state.vertex_attrib[0].enabled = GL_TRUE;
			break;
		case GL_COLOR_ARRAY:
			attribs_format[1] &= ~( 1 << 17 );
			pgl_state.vertex_attrib[1].enabled = GL_TRUE;
			break;
		case GL_TEXTURE_COORD_ARRAY:
			attribs_format[1] &= ~( 1 << (18 + pgl_state.texunit_active_client) );
			pgl_state.vertex_attrib[2 + pgl_state.texunit_active_client].enabled = GL_TRUE;
			break;
	}
}

void glDisableClientState(GLenum array)
{
	switch(array)
	{
		case GL_VERTEX_ARRAY:
			attribs_format[1] |= 1 << 16;
			pgl_state.vertex_attrib[0].enabled = GL_FALSE;
			break;
		case GL_COLOR_ARRAY:
			attribs_format[1] |= 1 << 17;
			pgl_state.vertex_attrib[1].enabled = GL_FALSE;
			break;
		case GL_TEXTURE_COORD_ARRAY:
			attribs_format[1] |= 1 << (18 + pgl_state.texunit_active_client);
			pgl_state.vertex_attrib[2 + pgl_state.texunit_active_client].enabled = GL_FALSE;
			break;
	}
}

void glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
	if(pgl_state.vertex_attrib[0].enabled == GL_FALSE)
		return;

	glDrawRangeElements(mode, first, count, count, GL_UNSIGNED_SHORT, NULL);
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
	if(pgl_state.vertex_attrib[0].enabled == false)
		return;

	if(type != GL_UNSIGNED_SHORT || pgl_state.vertex_attrib[0].enabled == GL_FALSE)
		return;

	uint32_t end = 0;

	for(int i = 0; i < count; i++)
	{
			if(end < ((GLushort*)indices)[i])
				end = ((GLushort*)indices)[i];
	}

	glDrawRangeElements( mode, 0, end + 1, count, type, indices );
}

void glDrawRangeElements( GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices )
{
	static const uint32_t attribs_permutation  = 0x3210;

	if(pgl_state.vertex_attrib[0].enabled == false)
		return;

	if(type != GL_UNSIGNED_SHORT || pgl_state.vertex_attrib[0].enabled == GL_FALSE)
		return;

	size_t cached_vertex_size = 0;

	for(int i = 0; i < 4; i++)
	{
		if(pgl_state.vertex_attrib[i].enabled == false)
			continue;

		if(pgl_state.vertex_attrib[i].cached_len < end)
			cached_vertex_size += pgl_state.vertex_attrib[i].stride * end;
	}

	if(indices)
		cached_vertex_size += count * sizeof(GLushort);

	if(pgl_check_cache_limit(cached_vertex_size) == false)
	{
		glFlush();	
		pgl_state.vertex_cache_pos = 0;
	}

	pgl_state_flush();
	
	uint8_t buffer_count = 0;

	for(int i = 0; i < 4; i++)
	{
		if(pgl_state.vertex_attrib[i].enabled == false)
			continue;

		if(pgl_state.vertex_attrib[i].cached_len < end)
		{
			pgl_state.vertex_attrib[i].cached_pointer = pgl_cache_data(pgl_state.vertex_attrib[i].pointer, pgl_state.vertex_attrib[i].stride * end);
			pgl_state.vertex_attrib[i].cached_len = end;
		}

		pica_attribbuffer_config(buffer_count, (uint32_t)pgl_state.vertex_attrib[i].cached_pointer - __ctru_linear_heap, pgl_state.vertex_attrib[i].buffer_config);

		buffer_count += 1;
	}

	buffer_count -= 1;

	if(pgl_state.current_mode != PGL_ARRAYS)
	{
		//GPUCMD_AddWrite(GPUREG_FIXEDATTRIB_INDEX, 1);
		//pica_fixed_attribute(1.0f, 1.0f, 1.0f, 1.0f);
		GPUCMD_AddWrite(GPUREG_VSH_ATTRIBUTES_PERMUTATION_LOW, attribs_permutation);
	}

	GPUCMD_AddWrite(GPUREG_ATTRIBBUFFERS_FORMAT_LOW,  attribs_format[0]);
	GPUCMD_AddWrite(GPUREG_ATTRIBBUFFERS_FORMAT_HIGH, attribs_format[1] | (buffer_count << 28));

	GPUCMD_AddMaskedWrite(GPUREG_VSH_INPUTBUFFER_CONFIG, 0xB, 0xA0000000 | (buffer_count));
	GPUCMD_AddWrite(GPUREG_VSH_NUM_ATTR, buffer_count);

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
			primitive_type = GPU_TRIANGLES;
	}
	
	if(indices)
	{
		uint16_t* index_array  = pgl_cache_data(indices, sizeof(uint16_t) * count);
		pica_draw_elements(primitive_type, (uint32_t)index_array - __ctru_linear_heap, count);
	}
	else
	{
		pica_draw_arrays(primitive_type, start, count);
	}

	if(++pgl_state.batched_draws > MAX_BATCHED_DRAWS)
		glFlush();

	pgl_state.current_mode = PGL_ARRAYS;
}

void glLockArraysEXT (GLint first, GLsizei count)
{
	size_t cached_vertex_size = 0;

	for(int i = 0; i < 4; i++)
	{
		if(pgl_state.vertex_attrib[i].enabled == false)
			continue;

		if(pgl_state.vertex_attrib[i].cached_len < count)
			cached_vertex_size += pgl_state.vertex_attrib[i].stride * count;
	}

	if(pgl_check_cache_limit(cached_vertex_size) == false)
	{
		glFlush();	
		pgl_state.vertex_cache_pos = 0;
	}

	for(int i = 0; i < 4; i++)
	{

		if(pgl_state.vertex_attrib[i].enabled == false)
			continue;

		if(pgl_state.vertex_attrib[i].cached_len < count)
		{
			pgl_state.vertex_attrib[i].cached_pointer = pgl_cache_data(pgl_state.vertex_attrib[i].pointer, pgl_state.vertex_attrib[i].stride * count);
			pgl_state.vertex_attrib[i].cached_len = count;
		}
	}
	
}