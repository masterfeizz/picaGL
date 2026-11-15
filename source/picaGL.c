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
	else if(pgl_state.stereo_enabled && pgl_state.stereo_target == PGL_STEREO_RIGHT)
		pgl_state.render_target_active = &pgl_state.render_target[2];
	else
		pgl_state.render_target_active = &pgl_state.render_target[1];

	pica_rendertarget_set(pgl_state.render_target_active);
}

int pglStereoEnable(int enable)
{
	pgl_rendertarget_t *top = &pgl_state.render_target[1], *right = &pgl_state.render_target[2];

	enable = enable ? 1 : 0;

	if(enable == pgl_state.stereo_enabled)
		return 1;

	if(enable && !right->color_buffer)
	{
		size_t bpp = top->color_format == GPU_RB_RGBA8 ? 4 : (top->color_format == GPU_RB_RGB8 ? 3 : 2);

		right->color_buffer = vramAlloc(400 * 240 * bpp);
		right->depth_buffer = vramAlloc(400 * 240 * 2);

		// render targets work in FCRAM too, a bit slower
		if(!right->color_buffer)
			right->color_buffer = linearAlloc(400 * 240 * bpp);
		if(!right->depth_buffer)
			right->depth_buffer = linearAlloc(400 * 240 * 2);

		if(!right->color_buffer || !right->depth_buffer)
			return 0;
	}

	GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_FLUSH, 1);

	pgl_state.stereo_enabled = enable;
	pgl_state.stereo_target  = PGL_STEREO_LEFT;

	// the left eye is the first 400 columns of the top target
	top->width = (!enable && pgl_state.top_wide) ? 800 : 400;

	if(pgl_state.render_target_active != &pgl_state.render_target[0])
	{
		pgl_state.render_target_active = top;
		pica_rendertarget_set(top);
	}

	pgl_state.changes |= pgl_change_viewport | pgl_change_scissor;

	return 1;
}

void pglStereoTarget(unsigned target)
{
	pgl_rendertarget_t *rt;

	if(!pgl_state.stereo_enabled)
		target = PGL_STEREO_LEFT;

	pgl_state.stereo_target = target;

	if(pgl_state.render_target_active == &pgl_state.render_target[0])
		return;

	rt = &pgl_state.render_target[target == PGL_STEREO_RIGHT ? 2 : 1];

	if(rt == pgl_state.render_target_active)
		return;

	GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_FLUSH, 1);

	pgl_state.render_target_active = rt;
	pica_rendertarget_set(rt);
}