#include "internal.h"

#include <stdio.h>

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
		case GPU_RGBA4:  return 2;

		default: 		 return 4;
	}
}

static GPU_TEXCOLOR _determineHardwareFormat(GLenum format)
{
	switch(format)
	{	
		case 4:
		case GL_RGBA8: //return GPU_RGBA8;
		case GL_RGBA:
		case GL_RGBA4: return GPU_RGBA4;
		case 3:
		case GL_RGB8:
		case GL_RGB:
		case GL_RGB5:  return GPU_RGB565;

		default: return GPU_RGBA4;
	}
}

static inline uint16_t rgba8_to_rgba4(uint32_t p)
{
	uint8_t *c = (uint8_t*)&p;
	uint8_t a = c[0] >> 4;
	uint8_t b = c[1] >> 4;
	uint8_t g = c[2] >> 4;
	uint8_t r = c[3] >> 4;
	return (r | (g << 4) | (b << 8) | (a << 12));
}

static inline uint16_t rgba8_to_rgb565(uint32_t p)
{
	uint8_t *c = (uint8_t*)&p;
	uint8_t r = c[0] >> 3;
	uint8_t g = c[1] >> 2;
	uint8_t b = c[2] >> 3;
	return (b | (g << 5) | (r << 11));
}


static inline uint32_t _picaConvertWrap(uint32_t param)
{
	switch(param)
	{
		case GL_CLAMP:
		case GL_CLAMP_TO_EDGE: return GPU_CLAMP_TO_EDGE;
		case GL_REPEAT: return GPU_REPEAT;
		case GL_MIRRORED_REPEAT: return GPU_MIRRORED_REPEAT;
		default: return GPU_REPEAT;
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

	if(size > 0x800)
		ret = vramAlloc(size);

	if(!ret)
		ret = linearAlloc(size); 

	return ret;
}

static inline void _textureDataFree(TextureObject *texture)
{
	if(texture->in_vram)
		vramFree(texture->data);
	else
		linearFree(texture->data);

	texture->data = NULL;
}

static inline uint32_t _textureSwizzleCoord(int x, int y, int width)
{
	uint32_t pos = 	(x & 0x1) << 0 | ((x & 0x2) << 1) | ((x & 0x4) << 2) |
					(y & 0x1) << 1 | ((y & 0x2) << 2) | ((y & 0x4) << 3);

	return ((x >> 3) << 6) + ((y >> 3) * ((width >> 3) << 6)) + pos;
}

static inline void _textureTile(TextureObject *texture, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, const uint32_t* data)
{
	uint32_t texel, offset;
	void *tiled_output = texture->data;

	if(texture->in_vram)
	{
		tiled_output = linearAlloc(texture->width * texture->height * _determineBPP(texture->format));

		if(!tiled_output)
			return;

		if(xoffset || yoffset)
		{
			_queueWaitAndClear();
			GX_TextureCopy(texture->data, 0, tiled_output, 0, texture->width * texture->height * _determineBPP(texture->format), 8);
			_queueRun(false);
			
			_textureDataFree(texture);

			texture->data = tiled_output;
			texture->in_vram = false;
		}
	}

	for(int y = 0; y < height; y++)
	{
		for(int x = 0; x < width; x++)
		{
			offset = _textureSwizzleCoord(x + xoffset, texture->height - 1 - (y + yoffset), texture->width);
			texel = data[x + (y * width)];

			if(texture->format == GPU_RGBA4)
				((uint16_t *)tiled_output)[offset] = rgba8_to_rgba4(texel);
			else if(texture->format == GPU_RGBA8)
				((uint32_t *)tiled_output)[offset] = __builtin_bswap32(texel);
			else if(texture->format == GPU_RGB565)
				((uint16_t *)tiled_output)[offset] = rgba8_to_rgb565(texel);
		}
	}

	GSPGPU_FlushDataCache(tiled_output, texture->width * texture->height * _determineBPP(texture->format));

	if(texture->in_vram)
	{
		_queueWaitAndClear();
		GX_TextureCopy(tiled_output, 0, texture->data, 0, texture->width * texture->height * _determineBPP(texture->format), 8);
		_queueRun(false);
		
		linearFree(tiled_output);
	}
}

void glActiveTexture(GLenum texture)
{
	uint8_t texunit = texture & 0x01;

	if(texunit > 1)
		texunit = 0;

	pglState->texUnitActive = texunit;
}

void glClientActiveTexture( GLenum texture )
{
	uint8_t texunit = texture & 0x01;

	if(texunit > 1)
		texunit = 0;

	pglState->texUnitActiveClient = texunit;
}

void glTexImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid* data)
{
	if(level != 0 || target != GL_TEXTURE_2D || format != GL_RGBA || type != GL_UNSIGNED_BYTE)
		return;

	TextureObject *texture = pglState->textureBound[pglState->texUnitActive];
	texture->used = true;

	if(texture->data)
		_textureDataFree(texture);

	texture->format = _determineHardwareFormat(internalFormat);
	texture->width  = width;
	texture->height = height;
	texture->data   = _textureDataAlloc(width * height * _determineBPP(texture->format));

	if(texture->data == NULL)
		return;

	if(_addressIsVRAM(texture->data))
		texture->in_vram = true;
	else
		texture->in_vram = false;

	if(data)
		_textureTile(texture, 0, 0, width, height, data);

	pglState->textureChanged = GL_TRUE;
	pglState->changes |= STATE_TEXTURE_CHANGE;
}

void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid* data)
{
	if(level != 0 || target != GL_TEXTURE_2D || format != GL_RGBA || type != GL_UNSIGNED_BYTE)
		return;

	TextureObject *texture = pglState->textureBound[pglState->texUnitActive];

	if(texture->data == NULL)
		return;

	_textureTile(texture, xoffset, yoffset, width, height, data);

	pglState->textureChanged = GL_TRUE;
	pglState->changes |= STATE_TEXTURE_CHANGE;
}

void glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
	TextureObject *texture = pglState->textureBound[pglState->texUnitActive];

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

	pglState->changes |= STATE_TEXTURE_CHANGE;
}

void glTexParameteri(GLenum target, GLenum pname, GLint param)
{
	GLfloat fparam = (GLfloat)param;
	glTexParameterf( target, pname, fparam );
}

void glBindTexture(GLenum target, GLuint texture)
{
	TextureObject *texObject;

	texObject = hashTableGet(&pglState->textureTable, texture);

	if(texObject == NULL)
	{
		texObject = malloc(sizeof(TextureObject));

		texObject->param  = GPU_TEXTURE_MAG_FILTER(GPU_LINEAR) | GPU_TEXTURE_WRAP_S(GPU_REPEAT) | GPU_TEXTURE_WRAP_T(GPU_REPEAT);
		texObject->width  = 0;
		texObject->height = 0;

		texObject->format = 0;
		texObject->data   = NULL;
		
		hashTableInsert(&pglState->textureTable, texture, texObject);
	}

	if(pglState->textureBound[pglState->texUnitActive] == texObject)
		return;

	pglState->textureBound[pglState->texUnitActive] = texObject;

	pglState->textureChanged = GL_TRUE;
	pglState->changes |= STATE_TEXTURE_CHANGE;
}

void glGenTextures(GLsizei n, GLuint *textures)
{
	TextureObject *texObject;
	int id;
	for(int i = 0; i < n; i++)
	{
		id = hashTableUniqueKey(&pglState->textureTable);
		texObject = malloc(sizeof(TextureObject));

		texObject->param  = GPU_TEXTURE_MAG_FILTER(GPU_LINEAR) | GPU_TEXTURE_WRAP_S(GPU_REPEAT) | GPU_TEXTURE_WRAP_T(GPU_REPEAT);
		texObject->width  = 0;
		texObject->height = 0;

		texObject->format = 0;
		texObject->data   = NULL;

		hashTableInsert(&pglState->textureTable, id, texObject);

		textures[i] = id;
	}
}

void glDeleteTextures(GLsizei n, const GLuint *textures)
{
	for(int i = 0; i < n; i++)
	{
		TextureObject *texObject = hashTableRemove(&pglState->textureTable, textures[i]);

		if(!texObject)
			continue;

		if(texObject->data)
			_textureDataFree(texObject);

		free(texObject);
	}
}

GLboolean glIsTexture( GLuint texture ) 
{ 
	if(texture == 0)
		return GL_FALSE;

	TextureObject *texObject = hashTableGet(&pglState->textureTable, texture);

	if(texObject)
		return GL_TRUE;

	return GL_FALSE;
}

void glTexImage1D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	glTexImage2D( GL_TEXTURE_2D, level, internalformat, width, 1, border, format, type, pixels );
}

void glTexEnvfv( GLenum target, GLenum pname, const GLfloat *params )
{
	if ((target != GL_TEXTURE_ENV) || (pname != GL_TEXTURE_ENV_COLOR))
		return;

	uint8_t texunit = pglState->texUnitActive;
	TextureEnv *texenv  = &pglState->texenv[texunit];

	uint8_t r = (uint8_t)(params[0] * 255);
	uint8_t g = (uint8_t)(params[1] * 255);
	uint8_t b = (uint8_t)(params[2] * 255);
	uint8_t a = (uint8_t)(params[3] * 255);

	texenv->color = r | (g << 8) | (b << 16) | (a << 24);
	
	pglState->changes |= STATE_TEXTURE_CHANGE;
}

inline void glTexEnvi (GLenum target, GLenum pname, GLint param)
{
	if ((target != GL_TEXTURE_ENV) || (pname != GL_TEXTURE_ENV_MODE))
		return;
	
	uint8_t texunit = pglState->texUnitActive;
	TextureEnv *texenv  = &pglState->texenv[texunit];

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

	pglState->changes |= STATE_TEXTURE_CHANGE;
}

void glTexEnvf (GLenum target, GLenum pname, GLfloat param)
{
	glTexEnvi (target, pname, (int)param);
}