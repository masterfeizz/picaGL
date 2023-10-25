#ifndef __PICAGL_H__
#define __PICAGL_H__

#include <GL/gl.h>

#ifdef __cplusplus
extern "C" {
#endif

void pglInit();
void pglInitEx(size_t command_buffer_length, size_t vertex_cache_size);
void pglExit();
void pglRestoreContext();

void pglSelectScreen(unsigned display, unsigned side);
void pglSwapBuffersEx(unsigned top, unsigned bot);
void pglSwapBuffers();

#ifdef __cplusplus
}
#endif

#endif