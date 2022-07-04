#pragma once

//Borrowed from citra
static inline uint32_t morton_interleave(uint32_t x, uint32_t y)
{
    static uint32_t xlut[] = {0x00, 0x01, 0x04, 0x05, 0x10, 0x11, 0x14, 0x15};
    static uint32_t ylut[] = {0x00, 0x02, 0x08, 0x0a, 0x20, 0x22, 0x28, 0x2a};

    return xlut[x % 8] + ylut[y % 8];
}

#define BLOCK_HEIGHT 8

static inline uint32_t get_morton_offset(uint32_t x, uint32_t y)
{
	uint32_t coarse_x = x & ~7;

	uint32_t i = morton_interleave(x, y);

	uint32_t offset = coarse_x * BLOCK_HEIGHT;

	return (i + offset);
}

static void texture_tile_rgba8(const void *src_v, void *dst_v, int xoffset, int yoffset, size_t width, size_t height, size_t pitch, size_t inv_h)
{
	uint32_t offset, output_y, coarse_y;

	const uint32_t *src = (uint32_t*)src_v;
	uint32_t       *dst = (uint32_t*)dst_v;

	for(int y = 0; y < height; y++)
	{
		output_y = inv_h - (y + yoffset);
		coarse_y = output_y & ~7;

		for(int x = 0; x < width; x++)
		{
			offset = get_morton_offset(x + xoffset, output_y) + coarse_y * pitch;
			dst[offset] = __builtin_bswap32(src[ x + (y * width) ]);
		}
	}
}

static void texture_tile_rgba8_to_rgb8(const void *src_v, void *dst_v, int xoffset, int yoffset, size_t width, size_t height, size_t pitch, size_t inv_h)
{
	uint32_t pixel, offset, output_y, coarse_y;

	const uint32_t *src = (uint32_t*)src_v;
	uint8_t        *dst = (uint8_t*)dst_v;

	for(int y = 0; y < height; y++)
	{
		output_y = inv_h - (y + yoffset);
		coarse_y = output_y & ~7;

		for(int x = 0; x < width; x++)
		{
			offset = (get_morton_offset(x + xoffset, output_y) + coarse_y * pitch) * 3;
			pixel = src[ x + (y * width) ];

			dst[offset + 2] = (pixel >>  0) & 0xff;
			dst[offset + 1] = (pixel >>  8) & 0xff;
			dst[offset + 0] = (pixel >> 16) & 0xff;
		}
	}
}

static void texture_tile_rgba8_to_rgb565(const void *src_v, void *dst_v, int xoffset, int yoffset, size_t width, size_t height, size_t pitch, size_t inv_h)
{
	uint32_t pixel, offset, output_y, coarse_y;

	const uint32_t *src = (uint32_t*)src_v;
	uint16_t       *dst = (uint16_t*)dst_v;

	for(int y = 0; y < height; y++)
	{
		output_y = inv_h - (y + yoffset);
		coarse_y = output_y & ~7;

		for(int x = 0; x < width; x++)
		{
			offset = get_morton_offset(x + xoffset, output_y) + coarse_y * pitch;
			pixel = src[ x + (y * width) ];

			dst[offset] = 	((pixel & 0xF80000) >> 19) | 
							((pixel & 0x00FC00) >>  5) | 
							((pixel & 0x0000F8) <<  8);
		}
	}
}

static void texture_tile_rgba8_to_rgba4(const void *src_v, void *dst_v, int xoffset, int yoffset, size_t width, size_t height, size_t pitch, size_t inv_h)
{
	uint32_t pixel, offset, output_y, coarse_y;

	const uint32_t *src = (uint32_t*)src_v;
	uint16_t       *dst = (uint16_t*)dst_v;

	for(int y = 0; y < height; y++)
	{
		output_y = inv_h - (y + yoffset);
		coarse_y = output_y & ~7;

		for(int x = 0; x < width; x++)
		{
			offset = get_morton_offset(x + xoffset, output_y) + coarse_y * pitch;
			pixel = src[ x + (y * width) ];

			dst[offset] = 	((pixel & 0xF0000000) >> 28) |
							((pixel & 0x00F00000) >> 16) |
							((pixel & 0x0000F000) >> 4) |
							((pixel & 0x000000F0) << 8);

		}
	}
}

typedef void (*tile_func_t)(const void*, void*, int, int, size_t, size_t, size_t, size_t);

static tile_func_t pgl_get_tile_func(GPU_TEXCOLOR hw_format, GLenum format, GLenum type)
{
	switch(hw_format)
	{
		case GPU_RGBA8: 	if(format == GL_RGBA && type == GL_UNSIGNED_BYTE) return texture_tile_rgba8;
							return NULL;

		case GPU_RGBA4:		if(format == GL_RGBA && type == GL_UNSIGNED_BYTE) return texture_tile_rgba8_to_rgba4;
							return NULL;

		case GPU_RGB8:		if(format == GL_RGBA && type == GL_UNSIGNED_BYTE) return texture_tile_rgba8_to_rgb8;
							return NULL;

		case GPU_RGB565:	if(format == GL_RGBA && type == GL_UNSIGNED_BYTE) return texture_tile_rgba8_to_rgb565;
							return NULL;

		default:		return NULL;
	}
}