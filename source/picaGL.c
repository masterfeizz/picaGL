#include <stdio.h>
#include "internal.h"

void pglInit()
{
	static int pgl_initialized = 0;

	if(pgl_initialized)
		return;

	pglState = malloc(sizeof(picaGLState));
	memset(pglState, 0, sizeof(picaGLState));
	
	pglState_init();
	pglState_default();
}

void pglExit()
{

}

void pglSwapBuffers()
{
	glFinish();

	uint32_t *output_framebuffer = (uint32_t*)gfxGetFramebuffer(pglState->display, pglState->display_side, NULL, NULL);
	uint8_t output_format = gfxGetScreenFormat(pglState->display);

	if(pglState->display == GFX_TOP)
	{
		GX_DisplayTransfer(
			(u32*)pglState->colorBuffer, GX_BUFFER_DIM(240, 400),
			output_framebuffer, GX_BUFFER_DIM(240, 400),
			GX_TRANSFER_OUT_FORMAT(output_format));
	}
	else
	{
		GX_DisplayTransfer(
			(u32*)pglState->colorBuffer + (240*80),GX_BUFFER_DIM(240, 320),
			output_framebuffer, GX_BUFFER_DIM(240, 320),
			GX_TRANSFER_OUT_FORMAT(output_format));
	}
	
	gspWaitForPPF();

	gfxSwapBuffersGpu();
}

void pglSelectScreen(uint8_t display, uint8_t side)
{
	pglState->display = display;
	pglState->display_side = side;
}