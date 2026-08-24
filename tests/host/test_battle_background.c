#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "gl2d.h"

#undef fopen

int battleBackgroundSelect(uint32_t entropy);
int battleBackgroundNext(uint32_t *state, uint32_t entropy);
bool battleBackgroundLoad(int background);
void battleBackgroundDraw(void);
void battleBackgroundReset(void);

static int next_texture = 70;
static int active_texture = -1;
static int draw_texture = -1;
static int draw_scale_x;
static int draw_scale_y;

FILE *testFopen(const char *path, const char *mode)
{
    static const char prefix[] = "nitro:/backgrounds/";
    char translated[192];
    FILE *file = NULL;

    if (strncmp(path, prefix, sizeof(prefix) - 1) != 0) {
        return NULL;
    }
    int length = snprintf(translated, sizeof(translated),
                          "../../nitrofs/backgrounds/%s",
                          path + sizeof(prefix) - 1);
    if (length < 0 || (size_t)length >= sizeof(translated) ||
        fopen_s(&file, translated, mode) != 0) {
        return NULL;
    }
    return file;
}

int glLoadTileSet(glImage *sprite, int tile_width, int tile_height,
                  int bitmap_width, int bitmap_height, int type,
                  int size_x, int size_y, int parameters,
                  int palette_width, const void *palette,
                  const void *texture)
{
    (void)type;
    (void)parameters;
    assert(tile_width == 128 && tile_height == 96);
    assert(bitmap_width == 128 && bitmap_height == 128);
    assert(size_x == 128 && size_y == 128);
    assert(palette_width == 256);
    assert(palette != NULL && texture != NULL);
    sprite->width = tile_width;
    sprite->height = tile_height;
    sprite->textureID = next_texture++;
    active_texture = sprite->textureID;
    return active_texture;
}

int glDeleteTextures(int count, int *textures)
{
    assert(count == 1 && textures != NULL);
    assert(*textures == active_texture);
    active_texture = -1;
    return 1;
}

void glPolyFmt(int format) { (void)format; }
void glColor(uint16_t color) { (void)color; }
void glSprite(int x, int y, int flip, const glImage *image)
{
    (void)x; (void)y; (void)flip; (void)image;
}
void glSpriteScaleXY(int x, int y, int scale_x, int scale_y, int flip,
                     const glImage *image)
{
    assert(x == 0 && y == 0 && flip == GL_FLIP_NONE);
    assert(image != NULL && image->textureID == active_texture);
    draw_texture = image->textureID;
    draw_scale_x = scale_x;
    draw_scale_y = scale_y;
}

int main(void)
{
    uint32_t state = UINT32_C(0x4A694D69);
    assert(battleBackgroundSelect(0u) == 0);
    assert(battleBackgroundSelect(1u) == 1);
    assert(battleBackgroundSelect(2u) == 2);
    assert(battleBackgroundSelect(3u) == 0);
    int first = battleBackgroundNext(&state, UINT32_C(0x12345678));
    int second = battleBackgroundNext(&state, UINT32_C(0x12345678));
    assert(first >= 0 && first < 3);
    assert(second >= 0 && second < 3);
    assert(first != second);
    assert(!battleBackgroundLoad(-1));
    assert(!battleBackgroundLoad(3));
    assert(battleBackgroundLoad(1));
    battleBackgroundDraw();
    assert(draw_texture == active_texture);
    assert(draw_scale_x == (int)(512u << 4));
    assert(draw_scale_y == (int)(512u << 4));
    battleBackgroundReset();
    assert(active_texture == -1);
    return 0;
}
