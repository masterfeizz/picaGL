#ifndef __PICAGL_H__
#define __PICAGL_H__

#include <GL/gl.h>

#ifdef __cplusplus
extern "C" {
#endif

void pglInit();
void pglExit();
void pglSwapBuffers();
void pglSelectScreen(uint8_t display, uint8_t side);

#ifdef __cplusplus
}
#endif

#endif