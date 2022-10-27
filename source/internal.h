#ifndef __PGL_INTERNAL_H__
#define __PGL_INTERNAL_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <3ds.h>
#include <GL/picaGL.h>

#include "types.h"
#include "pica.h"

#define MAX_BATCHED_DRAWS 2048

#define SCRATCH_TEXTURE_SIZE  256 * 256 * 4

#define PGL_TEXENV_UNTEXTURED 2
#define PGL_TEXENV_DUMMY 3

#define PGL_NONE      0
#define PGL_ARRAYS    1
#define PGL_IMMEDIATE 2

extern u32 __ctru_linear_heap;
extern u32 __ctru_linear_heap_size;

extern pgl_state_t pgl_state;

// state.c
void pgl_state_initialize(size_t command_buffer_length, size_t vertex_cache_size);
void pgl_state_flush();
void pgl_state_default();

// queue.c
void pgl_queue_init(void);
void pgl_queue_wait(bool clear);
void pgl_queue_commands(bool swap_list);

#endif //__PGL_INTERNAL_H__