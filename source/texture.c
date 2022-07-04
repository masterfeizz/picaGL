
#include <stdio.h>

#include "internal.h"
#include "texture_conv.h"

#define MAX_TEXTURES 4096

#define GPU_UNSUPPORTED 0xF

static pgl_texture_t *textures[MAX_TEXTURES] = { NULL };

static inline bool _addressIsVRAM(const void* addr)
{
	u32 vaddr = (u32)addr;
	return vaddr >= 0x1F000000 && vaddr < 0x1F600000;
}

static size_t _determineBPP(GPU_TEXCOLOR format)
{
	switch (format)
	{
		case GPU_RGBA8:  return 4;

		case GPU_RGB8:   return 3;

		case GPU_RGBA5551:
		case GPU_RGB565:
		case GPU_RGBA4:
		case GPU_LA8:    return 2;

		case GPU_A8:
		case GPU_LA4:    return 1;

		default:         return 4;
	}
}

static GPU_TEXCOLOR _determineHardwareFormat(GLint internalFormat, GLenum format, GLenum type)
{
	switch(format)
	{	
		case GL_RGBA:
			if(type == GL_UNSIGNED_BYTE && internalFormat == GL_RGB)
			{
				if(pgl_state.downsample_textures)
					return GPU_RGB565;
				else
					return GPU_RGB8;
			}
			if(type == GL_UNSIGNED_BYTE)
			{
				if(pgl_state.downsample_textures)
					return GPU_RGBA4;
				else
					return GPU_RGBA8;
			}
			break;

		default: break;
	}

	return GPU_UNSUPPORTED;
}

static inline uint32_t _picaConvertWrap(uint32_t param)
{
	switch(param)
	{
		case GL_CLAMP:
		case GL_CLAMP_TO_EDGE:   return GPU_CLAMP_TO_EDGE;
		case GL_REPEAT:          return GPU_REPEAT;
		case GL_MIRRORED_REPEAT: return GPU_MIRRORED_REPEAT;

		default:                 return GPU_REPEAT;
	}
}

static inline uint32_t _picaConvertFilter(uint32_t param)
{
	switch(param)
	{	
		case GL_LINEAR_MIPMAP_LINEAR:
		case GL_LINEAR: return GPU_LINEAR;
		case GL_NEAREST_MIPMAP_NEAREST:
		case GL_NEAREST: return GPU_NEAREST;
		
		default: return GL_LINEAR;
	}
}

static inline void *_textureDataAlloc(size_t size)
{
	void *ret = NULL;

	if(size > 0x800 && size < SCRATCH_TEXTURE_SIZE)
		ret = vramAlloc(size);

	if(!ret)
		ret = linearAlloc(size); 

	return ret;
}

static inline void _textureDataFree(pgl_texture_t *texture)
{
	if(texture->in_vram)
		vramFree(texture->data);
	else
		linearFree(texture->data);

	texture->data = NULL;
}

void glActiveTexture(GLenum texture)
{
	pgl_state.texunit_active = texture & 0x01;
}

void glClientActiveTexture( GLenum texture )
{
	pgl_state.texunit_active_client = texture & 0x01;
}

void glTexImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid* data)
{
	if(level != 0 || target != GL_TEXTURE_2D) return;

	if(internalFormat == 3) internalFormat = GL_RGB;
	if(internalFormat == 4) internalFormat = GL_RGBA;

	pgl_texture_t *texture = pgl_state.texture_bound[pgl_state.texunit_active];

	if(texture->data)
		_textureDataFree(texture);

	internalFormat = _determineHardwareFormat(internalFormat, format, type);

	if(internalFormat == GPU_UNSUPPORTED) exit(1);

	texture->format = internalFormat;
	texture->bpp 	= _determineBPP(texture->format);
	texture->width  = width;
	texture->height = height;
	texture->data   = _textureDataAlloc(width * height * texture->bpp);
	texture->max_level = 0;

	if(texture->data == NULL)
		return;

	texture->in_vram = _addressIsVRAM(texture->data);

	pgl_state.changes |= pglDirtyFlag_Texture;

	if(!data) return;

	tile_func_t texture_tile = pgl_get_tile_func(texture->format, format, type);

	if(texture_tile == NULL) return;

	void *tiled_output = texture->data;

	if(texture->in_vram)
	{
		pgl_queue_wait(true);
		tiled_output = pgl_state.scratch_texture;
	}

	texture_tile(data, tiled_output, 0, 0, width, height, width, height - 1);

	GSPGPU_FlushDataCache(tiled_output, texture->width * texture->height * texture->bpp);

	if(texture->in_vram)
		GX_TextureCopy(tiled_output, 0, texture->data, 0, texture->width * texture->height * texture->bpp, 8);
}

void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid* data)
{
	if(level != 0 || target != GL_TEXTURE_2D || data == NULL) return;

	pgl_texture_t *texture = pgl_state.texture_bound[pgl_state.texunit_active];

	if(texture->data == NULL) return;

	tile_func_t texture_tile = pgl_get_tile_func(texture->format, format, type);

	if(texture_tile == NULL) return;

	// Move textures out of vram if necessary
	if(texture->in_vram)
	{
		void *new_data = linearAlloc(texture->width * texture->height * texture->bpp);

		if(!new_data) return;

		GX_TextureCopy(texture->data, 0, new_data, 0, texture->width * texture->height * texture->bpp, 8);
		pgl_queue_wait(true);

		_textureDataFree(texture);

		texture->data = new_data;
		texture->in_vram = false;
	}

	texture_tile(data, texture->data, xoffset, yoffset, width, height, texture->width, texture->height - 1);

	GSPGPU_FlushDataCache(texture->data, texture->width * texture->height * texture->bpp);

	pgl_state.changes |= pglDirtyFlag_Texture;
}

void glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
	pgl_texture_t *texture = pgl_state.texture_bound[pgl_state.texunit_active];

	uint32_t tex_param = texture->param;

	switch(pname)
	{
		case GL_TEXTURE_MAG_FILTER:
			tex_param = (tex_param & 0xFFFE) | GPU_TEXTURE_MAG_FILTER(_picaConvertFilter(param));
			break;
		case GL_TEXTURE_MIN_FILTER:
			tex_param = (tex_param & 0xFFFD) | GPU_TEXTURE_MIN_FILTER(_picaConvertFilter(param));
			break;
		case GL_TEXTURE_WRAP_T:
			tex_param = (tex_param & 0xFCFF) | GPU_TEXTURE_WRAP_T(_picaConvertWrap(param));
			break;
		case GL_TEXTURE_WRAP_S:
			tex_param = (tex_param & 0xCFFF) | GPU_TEXTURE_WRAP_S(_picaConvertWrap(param));
			break;
		default:
			return;
	}

	texture->param = tex_param;

	pgl_state.changes |= pglDirtyFlag_Texture;
}

void glTexParameteri(GLenum target, GLenum pname, GLint param)
{
	GLfloat fparam = (GLfloat)param;
	glTexParameterf( target, pname, fparam );
}

void glBindTexture(GLenum target, GLuint texture)
{
	pgl_texture_t *tex;

	tex = textures[texture];

	if(tex == NULL)
	{
		tex = calloc(1, sizeof(pgl_texture_t));
		tex->param  = GPU_TEXTURE_MAG_FILTER(GPU_LINEAR) | GPU_TEXTURE_WRAP_S(GPU_REPEAT) | GPU_TEXTURE_WRAP_T(GPU_REPEAT);
		
		textures[texture] = tex;
	}

	if(pgl_state.texture_bound[pgl_state.texunit_active] == tex)
		return;

	pgl_state.texture_bound[pgl_state.texunit_active] = tex;

	pgl_state.changes |= pglDirtyFlag_Texture;
}

void glGenTextures(GLsizei n, GLuint *texture)
{
	pgl_texture_t *texObject;

	for(int i = 0; i < n; i++)
	{
		for(int j = 0; j < MAX_TEXTURES; j++)
		{
			if(textures[j] == NULL)
			{
				texObject = calloc(1, sizeof(pgl_texture_t));
				texObject->param  = GPU_TEXTURE_MAG_FILTER(GPU_LINEAR) | GPU_TEXTURE_WRAP_S(GPU_REPEAT) | GPU_TEXTURE_WRAP_T(GPU_REPEAT);

				textures[j] = texObject;

				texture[i] = j;

				break;
			}
		}
	}
}

void glDeleteTextures(GLsizei n, const GLuint *texture)
{
	for(int i = 0; i < n; i++)
	{
		pgl_texture_t *texObject = textures[ texture[i] ];

		if(texObject == NULL)
			continue;

		if(texObject->data)
			_textureDataFree(texObject);

		free(texObject);

		textures[ texture[i] ] = NULL;
	}
}

GLboolean glIsTexture( GLuint texture ) 
{ 
	if(texture == 0)
		return GL_FALSE;

	pgl_texture_t *texObject = textures[ texture ];

	if(texObject)
		return GL_TRUE;

	return GL_FALSE;
}

void glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	glTexImage2D( GL_TEXTURE_2D, level, internalformat, width, 1, border, format, type, pixels );
}

void glGenerateMipmap(GLenum target)
{
	if(target != GL_TEXTURE_2D) return;

	pgl_texture_t *texture = pgl_state.texture_bound[pgl_state.texunit_active];

	if(texture->data == NULL) return;
	if(texture->width < 128 || texture->height < 128) return;

	void *new_data = _textureDataAlloc(texture->width * texture->height * texture->bpp * 2);

	if(!new_data) return;

	uint32_t transfer_flags = GX_TRANSFER_IN_FORMAT(texture->format) | GX_TRANSFER_OUT_FORMAT(texture->format) |
							  GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_XY) | BIT(5);

	uint32_t dimension = GX_BUFFER_DIM(texture->height, texture->width);

	GX_TextureCopy(texture->data, 0, new_data, 0, texture->width * texture->height * texture->bpp, 8);

	GX_DisplayTransfer( (uint32_t*)texture->data, dimension,
						(uint32_t*)(new_data + (texture->width * texture->height * texture->bpp)), dimension,
						transfer_flags);

	pgl_queue_wait(true);

	_textureDataFree(texture);

	texture->data = new_data;
	texture->in_vram = _addressIsVRAM(texture->data);
	texture->max_level = 1;

	GSPGPU_FlushDataCache(texture->data, texture->width * texture->height * texture->bpp * 2);

	pgl_state.changes |= pglDirtyFlag_Texture;
}