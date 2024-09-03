// https://github.com/Lameguy64/PSn00bSDK/blob/master/examples/graphics/hdtv/clip.h
#ifndef _CLIP_H
#define _CLIP_H

#include <sys/types.h>
#include <libgte.h>
#include <libgpu.h>

/* tri_clip
 *
 * Returns non-zero if a triangle (v0, v1, v2) is outside 'clip'.
 *
 * clip			- Clipping area
 * v0,v1,v2		- Triangle coordinates
 *
 */
int tri_clip(RECT* clip, DVECTOR* v0, DVECTOR* v1, DVECTOR* v2);

/* quad_clip
 *
 * Returns non-zero if a quad (v0, v1, v2, v3) is outside 'clip'.
 *
 * clip			- Clipping area
 * v0,v1,v2,v3	- Quad coordinates
 *
 */
int quad_clip(RECT* clip, DVECTOR* v0, DVECTOR* v1, DVECTOR* v2, DVECTOR* v3);

#endif // _CLIP_H
