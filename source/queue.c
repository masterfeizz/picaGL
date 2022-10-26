#include "internal.h"

static bool swap_top, swap_bottom;

static void queue_finish_callback(gxCmdQueue_s* queue)
{
	if (swap_top)
		gfxScreenSwapBuffers(GFX_TOP, false);
	
	if (swap_bottom)
		gfxScreenSwapBuffers(GFX_BOTTOM, false);

	swap_top = swap_bottom = false;

	gxCmdQueueSetCallback(queue, NULL, NULL);
}

void pgl_queue_init(void)
{
	swap_top = swap_bottom = false;

	GX_BindQueue( &pgl_state.gx_queue);
	gxCmdQueueRun(&pgl_state.gx_queue);
}

void pgl_queue_wait(bool clear)
{
	gxCmdQueueWait (NULL, -1);

	if(clear)
		gxCmdQueueClear(&pgl_state.gx_queue);
}

void glFlush(void)
{
	static uint32_t next_buffer = 1;

	if(pgl_state.batched_draws == 0) return;

	pgl_queue_wait(true);

	GSPGPU_FlushDataCache(pgl_state.vertex_cache, pgl_state.vertex_cache_size);
	
	u32* commandBuffer;
	u32  commandBuffer_size;

	GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_FLUSH, 1);
	GPUCMD_AddWrite(GPUREG_EARLYDEPTH_CLEAR, 1);

	GPUCMD_Split(&commandBuffer, &commandBuffer_size);

	GX_FlushCacheRegions (commandBuffer, commandBuffer_size * 4, (u32 *) __ctru_linear_heap, __ctru_linear_heap_size, NULL, 0);
	GX_ProcessCommandList(commandBuffer, commandBuffer_size * 4, GX_CMDLIST_FLUSH);
	
	pgl_state.batched_draws = 0;

	GPUCMD_SetBuffer(pgl_state.command_buffer[next_buffer], pgl_state.command_buffer_length, 0);
	
	next_buffer = !next_buffer;
}

void glFinish(void)
{
	glFlush();
	pgl_queue_wait(true);
}

//Input transfer format doesn't match render target format...
static const int transfer_in_format[5] = { 0, 1, 3, 2, 4 };

void pglSwapBuffersEx(unsigned top, unsigned bot)
{
	uint32_t *output_framebuffer;
	uint32_t  transfer_flags, dimension;

	glFlush();

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

	gxCmdQueueSetCallback(&pgl_state.gx_queue, queue_finish_callback, NULL);

	pgl_state.batched_draws = 0;
	pgl_state.current_mode = 0;
}

void pglSwapBuffers()
{
	pglSwapBuffersEx(true, true);
}