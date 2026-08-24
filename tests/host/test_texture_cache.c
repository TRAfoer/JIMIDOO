#include "graphics_service.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "gl2d.h"

#undef fopen

#define TEST_TEXTURE_ID_BASE 100
#define TEST_TEXTURE_ID_LIMIT 256
#define RESERVED_FONT_TEXTURE_ID 42

static bool active_textures[TEST_TEXTURE_ID_LIMIT];
static bool active_palettes[TEST_TEXTURE_ID_LIMIT];
static int next_texture_id = TEST_TEXTURE_ID_BASE;
static int active_texture_count;
static int active_palette_count;
static int maximum_active_textures;
static int maximum_active_palettes;
static int deleted_textures;
static int last_draw_texture = -1;
static int last_draw_scale_x;
static int last_draw_scale_y;

static FILE *openFile(const char *path, const char *mode)
{
    FILE *file = NULL;
    if (fopen_s(&file, path, mode) != 0) {
        return NULL;
    }
    return file;
}

FILE *testFopen(const char *path, const char *mode)
{
    static const char prefix[] = "nitro:/cats/";
    if (strncmp(path, prefix, sizeof(prefix) - 1) != 0) {
        return NULL;
    }

    char translated[160];
    int length = snprintf(translated, sizeof(translated),
                          "../../nitrofs/cats/%s",
                          path + sizeof(prefix) - 1);
    if (length < 0 || (size_t)length >= sizeof(translated)) {
        return NULL;
    }
    return openFile(translated, mode);
}

int glLoadTileSet(glImage *sprite, int tile_width, int tile_height,
                  int bitmap_width, int bitmap_height, int type,
                  int size_x, int size_y, int parameters,
                  int palette_width, const void *palette,
                  const void *texture)
{
    (void)bitmap_width;
    (void)bitmap_height;
    (void)type;
    (void)size_x;
    (void)size_y;
    (void)parameters;
    assert(palette_width == 256);
    assert(palette != NULL);
    assert(texture != NULL);
    assert(next_texture_id < TEST_TEXTURE_ID_LIMIT);

    int texture_id = next_texture_id++;
    sprite->width = tile_width;
    sprite->height = tile_height;
    sprite->u_off = 0;
    sprite->v_off = 0;
    sprite->textureID = texture_id;
    active_textures[texture_id] = true;
    active_palettes[texture_id] = true;
    ++active_texture_count;
    ++active_palette_count;
    if (active_texture_count > maximum_active_textures) {
        maximum_active_textures = active_texture_count;
    }
    if (active_palette_count > maximum_active_palettes) {
        maximum_active_palettes = active_palette_count;
    }
    return texture_id;
}

int glDeleteTextures(int count, int *texture_ids)
{
    assert(count == 1);
    assert(texture_ids != NULL);
    int texture_id = texture_ids[0];
    assert(texture_id != RESERVED_FONT_TEXTURE_ID);
    assert(texture_id >= TEST_TEXTURE_ID_BASE);
    assert(texture_id < TEST_TEXTURE_ID_LIMIT);
    assert(active_textures[texture_id]);
    assert(active_palettes[texture_id]);
    active_textures[texture_id] = false;
    active_palettes[texture_id] = false;
    --active_texture_count;
    --active_palette_count;
    ++deleted_textures;
    return 1;
}

void glPolyFmt(int format) { (void)format; }
void glColor(uint16_t color) { (void)color; }
void glSprite(int x, int y, int flip, const glImage *image)
{
    (void)x;
    (void)y;
    (void)flip;
    assert(image != NULL);
    assert(active_textures[image->textureID]);
    last_draw_texture = image->textureID;
}

void glSpriteScaleXY(int x, int y, int scale_x, int scale_y, int flip,
                     const glImage *image)
{
    (void)x;
    (void)y;
    (void)flip;
    assert(image != NULL);
    assert(active_textures[image->textureID]);
    last_draw_texture = image->textureID;
    last_draw_scale_x = scale_x;
    last_draw_scale_y = scale_y;
}

static void expectDrawn(CatId cat, CatAction action, bool expected)
{
    last_draw_texture = -1;
    catTextureDraw(cat, action, 0, 0, false);
    assert((last_draw_texture >= TEST_TEXTURE_ID_BASE) == expected);
}

int main(void)
{
    _Static_assert(CAT_TEXTURE_CACHE_CAT_LIMIT == 2,
                   "combat residency is bounded to two cats");
    _Static_assert(GRAPHICS_TEXTURE_MAX_RESIDENT_BYTES <=
                       GRAPHICS_TEXTURE_VRAM_BYTES,
                   "worst-case texture residency must fit banks A/B/D");

    active_textures[RESERVED_FONT_TEXTURE_ID] = true;
    active_palettes[RESERVED_FONT_TEXTURE_ID] = true;
    catTexturesReset();
    assert(active_textures[RESERVED_FONT_TEXTURE_ID]);
    assert(active_palettes[RESERVED_FONT_TEXTURE_ID]);
    assert(catTexturesLoad(CAT_ORANGE));
    assert(catTexturesLoad(CAT_TABBY));
    assert(active_texture_count == 14);
    assert(active_palette_count == 14);
    assert(maximum_active_textures == 14);
    assert(maximum_active_palettes == 14);

    assert(catTextureLoad(CAT_ORANGE, CAT_ACTION_IDLE));
    assert(catTexturesLoad(CAT_MAODIE));
    assert(active_texture_count == 14);
    assert(active_palette_count == 14);
    assert(maximum_active_textures == 14);
    assert(maximum_active_palettes == 14);
    assert(deleted_textures == 7);
    expectDrawn(CAT_ORANGE, CAT_ACTION_IDLE, true);
    expectDrawn(CAT_TABBY, CAT_ACTION_IDLE, false);
    expectDrawn(CAT_MAODIE, CAT_ACTION_IDLE, true);

    catTexturesReset();
    assert(active_texture_count == 0);
    assert(active_palette_count == 0);
    assert(deleted_textures == 21);
    assert(active_textures[RESERVED_FONT_TEXTURE_ID]);
    assert(active_palettes[RESERVED_FONT_TEXTURE_ID]);

    assert(catTextureLoad(CAT_BANANA, CAT_ACTION_IDLE));
    assert(active_texture_count == 1);
    assert(active_palette_count == 1);
    expectDrawn(CAT_BANANA, CAT_ACTION_IDLE, true);
    catTextureDrawScaled(CAT_BANANA, CAT_ACTION_IDLE, 4, 5, false, 288u);
    assert(last_draw_scale_x == (int)(288u << 4));
    assert(last_draw_scale_y == (int)(288u << 4));
    assert(active_textures[RESERVED_FONT_TEXTURE_ID]);
    assert(active_palettes[RESERVED_FONT_TEXTURE_ID]);

    catTexturesReset();
    assert(active_texture_count == 0);
    assert(active_palette_count == 0);
    assert(active_textures[RESERVED_FONT_TEXTURE_ID]);
    assert(active_palettes[RESERVED_FONT_TEXTURE_ID]);
    return 0;
}
