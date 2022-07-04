#ifndef __PICAGL_H__
#define __PICAGL_H__

#include <GL/gl.h>

#ifdef __cplusplus
extern "C" {
#endif

void pglInit();
void pglExit();
void pglSelectScreen(unsigned display, unsigned side);
void pglSwapBuffersEx(unsigned top, unsigned bot);
void pglSwapBuffers();

#ifdef __cplusplus
}
#endif

#endif