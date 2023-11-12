#include <stdio.h>
#include "internal.h"

#define COMMAND_BUFFER_LENGTH 0x060000
#define VERTEX_BUFFER_SIZE    0x080000

static bool pgl_initialized = false;
static aptHookCookie hook_cookie;

static void apt_hook_callback(APT_HookType type, void* param)
{

	switch (type)
	{
		case APTHOOK_ONSUSPEND:
		{
			glFinish();

			break;
		}
		case APTHOOK_ONRESTORE:
		{
			pglRestoreContext();

			break;
		}
		default: break;
	}
}

void pglInitEx(size_t command_buffer_length, size_t vertex_cache_size)
{
	if(pgl_initialized)
		return;

	memset(&pgl_state, 0, sizeof(pgl_state_t));
	
	pgl_state_initialize(command_buffer_length, vertex_cache_size);
	pgl_state_default();

	aptHook(&hook_cookie, apt_hook_callback, NULL);

	pgl_initialized = true;
}

void pglInit()
{
	pglInitEx(COMMAND_BUFFER_LENGTH, VERTEX_BUFFER_SIZE);
}

void pglExit()
{
	aptUnhook(&hook_cookie);
	
	pgl_queue_wait(true);
	GX_BindQueue(NULL);
	//TODO: Clear memory
}

void pglRestoreContext()
{
	if(!pgl_initialized)
		return;

	pica_rendertarget_set(pgl_state.render_target_active);
	pica_attribbuffers_location((void*)__ctru_linear_heap);

	GPUCMD_AddIncrementalWrites(GPUREG_TEXENV2_SOURCE, (uint32_t*)&pgl_state.texenv[PGL_TEXENV_DUMMY], 5);
	GPUCMD_AddIncrementalWrites(GPUREG_TEXENV3_SOURCE, (uint32_t*)&pgl_state.texenv[PGL_TEXENV_DUMMY], 5);
	GPUCMD_AddIncrementalWrites(GPUREG_TEXENV4_SOURCE, (uint32_t*)&pgl_state.texenv[PGL_TEXENV_DUMMY], 5);
	GPUCMD_AddIncrementalWrites(GPUREG_TEXENV5_SOURCE, (uint32_t*)&pgl_state.texenv[PGL_TEXENV_DUMMY], 5);

	shaderProgramUse(&pgl_state.default_shader_program);

	pgl_state.current_mode = -1;
	pgl_state.changes = pgl_change_any;
}

void pglSelectScreen(unsigned display, unsigned side)
{
	pgl_queue_commands(false);
	
	if(display == GFX_BOTTOM) 
		pgl_state.render_target_active = &pgl_state.render_target[0];
	else 
		pgl_state.render_target_active = &pgl_state.render_target[1];

	pica_rendertarget_set(pgl_state.render_target_active);
}