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

// Stereoscopic 3D on the top screen. While enabled the top screen shows a left and a right
// eye of 400x240 (wide mode is suspended), each drawn into its own color and depth buffer:
// pick the eye with pglStereoTarget before drawing it. pglSwapBuffers shows both.
#define PGL_STEREO_LEFT  0
#define PGL_STEREO_RIGHT 1
int  pglStereoEnable(int enable); // returns 0 if the right eye buffers can't be allocated
void pglStereoTarget(unsigned target);

#ifdef __cplusplus
}
#endif

#endif