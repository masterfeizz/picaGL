#include "internal.h"

static LightEvent swap_done;
static bool       swap_top, swap_bottom;

static void display_transfer_callback(void *arg)
{
	if (swap_top) 
	{
		swap_top = false;
		gfxScreenSwapBuffers(GFX_TOP, false);
	}
	else if (swap_bottom)
	{
		swap_bottom = false;
		gfxScreenSwapBuffers(GFX_BOTTOM, false);
	}

	if (swap_bottom == false && swap_top == false)
	{
		LightEvent_Signal(&swap_done);
	}
}

void pgl_queue_init(void)
{
	swap_top = swap_bottom = false;

	GX_BindQueue( &pgl_state.gx_queue);
	gxCmdQueueRun(&pgl_state.gx_queue);

	gspSetEventCallback(GSPGPU_EVENT_PPF, display_transfer_callback, NULL, false); 

	LightEvent_Init(&swap_done, RESET_ONESHOT);
	LightEvent_Signal(&swap_done);
}

void pgl_queue_wait(bool clear)
{
	gxCmdQueueWait (NULL, -1);

	if(clear)
		gxCmdQueueClear(&pgl_state.gx_queue);
}

void pgl_queue_commands(bool swap_list)
{
	static uint32_t next_buffer = 1;

	u32* command_buffer;
	u32  command_buffer_size;

	GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_FLUSH, 1);

	GPUCMD_Split(&command_buffer, &command_buffer_size);

	GX_FlushCacheRegions ((u32 *)__ctru_linear_heap, __ctru_linear_heap_size, (u32 *)NULL, 0, NULL, 0);

	if(swap_list)
	{
		pgl_queue_wait(true);
		
		GPUCMD_SetBuffer(pgl_state.command_buffer[next_buffer], pgl_state.command_buffer_length, 0);

		next_buffer = !next_buffer;

		pgl_state.batched_draws = 0;
	}

	GX_ProcessCommandList(command_buffer, command_buffer_size * 4, 0);
}

void glFlush(void)
{
	pgl_queue_commands(false);
}

void glFinish(void)
{
	pgl_queue_commands(true);
	pgl_queue_wait(true);
}

//Input transfer format doesn't match render target format...
static const int transfer_in_format[5] = { 0, 1, 3, 2, 4 };

void pglSwapBuffersEx(unsigned top, unsigned bot)
{
	uint32_t *output_framebuffer;
	uint32_t  transfer_flags, dimension;

	pgl_queue_commands(true);

	LightEvent_Wait(&swap_done);

	if(top)
	{
		output_framebuffer = (uint32_t*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
		dimension = GX_BUFFER_DIM(pgl_state.render_target[1].height, pgl_state.render_target[1].width);

		transfer_flags  = GX_TRANSFER_IN_FORMAT(transfer_in_format[pgl_state.render_target[1].color_format]);
		transfer_flags |= GX_TRANSFER_OUT_FORMAT(gfxGetScreenFormat(GFX_TOP));
		
		GX_DisplayTransfer(
			(u32*)pgl_state.render_target[1].color_buffer, dimension,
			output_framebuffer, dimension,
			transfer_flags);

		swap_top = true;
	}

	if(bot)
	{
		output_framebuffer = (uint32_t*)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
		dimension = GX_BUFFER_DIM(pgl_state.render_target[0].height, pgl_state.render_target[0].width);

		transfer_flags  = GX_TRANSFER_IN_FORMAT(transfer_in_format[pgl_state.render_target[0].color_format]);
		transfer_flags |= GX_TRANSFER_OUT_FORMAT(gfxGetScreenFormat(GFX_BOTTOM));

		GX_DisplayTransfer(
			(u32*)pgl_state.render_target[0].color_buffer, dimension,
			output_framebuffer, dimension,
			transfer_flags);

		swap_bottom = true;
	}
}

void pglSwapBuffers()
{
	pglSwapBuffersEx(true, true);
}