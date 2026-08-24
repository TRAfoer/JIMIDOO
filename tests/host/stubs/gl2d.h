#ifndef TEST_GL2D_H
#define TEST_GL2D_H

#include <stdint.h>

#ifndef __GNUC__
#define __attribute__(value)
#endif

typedef struct glImage {
    int width;
    int height;
    int u_off;
    int v_off;
    int textureID;
} glImage;

#define GL_RGB256 1
#define TEXGEN_TEXCOORD 2
#define GL_TEXTURE_COLOR0_TRANSPARENT 4
#define GL_FLIP_NONE (1 << 0)
#define GL_FLIP_H (1 << 2)
#define POLY_CULL_NONE 0
#define POLY_ALPHA(value) (value)
#define POLY_ID(value) ((value) << 8)
#define RGB15(red, green, blue) \
    ((uint16_t)((red) | ((green) << 5) | ((blue) << 10)))

int glLoadTileSet(glImage *sprite, int tile_width, int tile_height,
                  int bitmap_width, int bitmap_height, int type,
                  int size_x, int size_y, int parameters,
                  int palette_width, const void *palette,
                  const void *texture);
int glDeleteTextures(int count, int *texture_ids);
void glPolyFmt(int format);
void glColor(uint16_t color);
void glSprite(int x, int y, int flip, const glImage *image);
void glSpriteScaleXY(int x, int y, int scale_x, int scale_y, int flip,
                     const glImage *image);

#endif
