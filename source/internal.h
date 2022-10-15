#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <3ds.h>
#include <GL/picaGL.h>

#include "pica.h"

#define MATRIX_STACK_SIZE 16

#define MAX_BATCHED_DRAWS 1024

#define SCRATCH_TEXTURE_SIZE  256 * 256 * 4

enum
{
	pglDirtyFlag_Viewport     = BIT(0),
	pglDirtyFlag_DepthTest    = BIT(1),
	pglDirtyFlag_AlphaTest    = BIT(2),
	pglDirtyFlag_Cull         = BIT(3),
	pglDirtyFlag_DepthMap     = BIT(4),
	pglDirtyFlag_Blend        = BIT(5),
	pglDirtyFlag_Scissor      = BIT(6),
	pglDirtyFlag_Texture      = BIT(7),
	pglDirtyFlag_TexEnv       = BIT(8),
	pglDirtyFlag_RenderTarget = BIT(9),
	pglDirtyFlag_Matrix       = BIT(10),

	pglDirtyFlag_Mat_ModelView   = BIT(10),
	pglDirtyFlag_Mat_Projection  = BIT(11),

	pglDirtyFlag_Any = 0xFFFF,
};

#define PGL_TEXENV_UNTEXTURED 2
#define PGL_TEXENV_DUMMY 3

#define PGL_NONE      0
#define PGL_ARRAYS    1
#define PGL_IMMEDIATE 2

extern u32 __ctru_linear_heap;
extern u32 __ctru_linear_heap_size;

typedef union {

	struct { float w, z, y, x; };
	float column[4];

} vector4f;

typedef struct {

	vector4f row[4]; 

} mat4f;

typedef struct {

	uint16_t src_rgb, src_alpha;
	uint32_t op_rgb:12, op_alpha:12;
	uint16_t func_rgb, func_alpha;
	uint32_t color;
	uint16_t scale_rgb, scale_alpha;
	
} pgl_texenv_t;

typedef struct {

	void* data;

	GPU_TEXCOLOR format : 4;
	size_t       size   : 28;

	uint32_t border;

	union
	{
		uint32_t dim;
		struct { uint16_t height, width; };
	};

	uint32_t param;

	union
	{
		u32 lod_param;
		struct { uint16_t lod_bias; uint8_t max_level, min_level; };
	};

	uint16_t bpp;
	uint16_t in_vram;

} pgl_texture_t;

typedef struct {

	GLboolean enabled;
	GLint     size;
	GLenum    type;
	GLsizei   stride;
	GLsizei   padding;

	const void* pointer;
	const void* cached_pointer;

	size_t		cached_len;

	uint64_t buffer_config;

} pgl_attrib_info_t;

typedef struct {

	void *color_buffer;
	void *depth_buffer;

	uint16_t width;
	uint16_t height;
	
	GPU_COLORBUF color_format;
	GPU_DEPTHBUF depth_format;

} pgl_rendertarget_t;

typedef struct {

	bool enabled;

	union {
		struct {
			GPU_BLENDEQUATION rgb_eq        : 8;
			GPU_BLENDEQUATION alpha_eq      : 8;
			GPU_BLENDFACTOR   rgb_sfactor   : 4;
			GPU_BLENDFACTOR   rgb_dfactor   : 4;
			GPU_BLENDFACTOR   alpha_sfactor : 4;
			GPU_BLENDFACTOR   alpha_dfactor : 4;
		};
		uint32_t value;
	};

	uint32_t color;

} pgl_alpha_blend_t;

typedef struct {

	uint32_t enabled;

	uint16_t x;
	uint16_t y;
	uint16_t width;
	uint16_t height;

} pgl_scissor_t;

typedef struct {

	int16_t x;
	int16_t y;
	int16_t width;
	int16_t height;

} pgl_viewport_t;

typedef struct {

	float near;
	float far;
	float offset;
	bool  offset_enabled;

} pgl_depthmap_t;

typedef struct {

	uint32_t mode    : 2;
	uint32_t enabled : 30;

} pgl_facecull_t;

typedef union {

	uint32_t value;
	struct
	{
		uint32_t enabled    : 4;
		uint32_t function   : 4;
		uint32_t write_mask : 24;
	};

} pgl_depth_test_t;

typedef union {

	uint32_t value;
	struct
	{
		uint32_t enabled   : 4;
		uint32_t function  : 4;
		uint32_t reference : 24;
	};

} pgl_alpha_test_t;

typedef struct {

	uint32_t            *command_buffer;
	size_t               command_buffer_length;

	shaderProgram_s      default_shader_program;

	gxCmdQueue_s         gx_queue;

	pgl_rendertarget_t   render_target[2];
	pgl_rendertarget_t  *render_target_active;

	pgl_alpha_blend_t    blend;
	pgl_depthmap_t       depthmap;
	pgl_depth_test_t     depth_test;
	pgl_alpha_test_t     alpha_test;
	pgl_facecull_t       face_cull;
	pgl_scissor_t        scissor;
	pgl_viewport_t       viewport;

	pgl_texenv_t         texenv[4];
	
	float                clear_color[4];
	float                clear_depth;

	mat4f                matrix_stack[2][MATRIX_STACK_SIZE];
	size_t               matrix_stack_index[2];
	int                  matrix_mode;
	mat4f               *matrix_current;

	uint32_t             changes;

	bool                 texunit_enabled[2];
	uint16_t             texunit_active;
	uint16_t             texunit_active_client;

	pgl_texture_t       *texture_bound[2];

	pgl_attrib_info_t    vertex_attrib[4];

	void                *scratch_texture;

	void                *vertex_cache;
	size_t               vertex_cache_pos;
	size_t               vertex_cache_size;

	GLuint               batched_draws;

	uint32_t             current_mode;

	bool                 downsample_textures;
	
} pgl_state_t;

extern pgl_state_t pgl_state;

// state.c
void pgl_state_initialize(size_t command_buffer_length, size_t vertex_cache_size);
void pgl_state_flush();
void pgl_state_default();

// queue.c
void pgl_queue_init(void);
void pgl_queue_wait(bool clear);
