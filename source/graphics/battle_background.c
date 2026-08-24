#include "graphics_service.h"

#include <stddef.h>
#include <stdio.h>

#include <gl2d.h>

#define BACKGROUND_WIDTH 128
#define BACKGROUND_VISIBLE_HEIGHT 96
#define BACKGROUND_TEXTURE_HEIGHT 128
#define BACKGROUND_PALETTE_ENTRIES 256
#define BACKGROUND_DRAW_SCALE 512U

static const char *const resource_names[BATTLE_BACKGROUND_COUNT] = {
    [BATTLE_BACKGROUND_ALLEY_DAY] = "alley_day",
    [BATTLE_BACKGROUND_ALLEY_DUSK] = "alley_dusk",
    [BATTLE_BACKGROUND_ALLEY_RAIN] = "alley_rain"
};

static uint8_t image_buffer[BATTLE_BACKGROUND_TEXTURE_BYTES]
    __attribute__((aligned(4)));
static uint16_t palette_buffer[BACKGROUND_PALETTE_ENTRIES]
    __attribute__((aligned(4)));
static glImage background_image;
static bool background_loaded;

static bool readExactFile(const char *path, void *destination, size_t size)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }
    bool valid = fread(destination, 1, size, file) == size && fgetc(file) == EOF;
    fclose(file);
    return valid;
}

BattleBackgroundId battleBackgroundSelect(uint32_t entropy)
{
    return (BattleBackgroundId)(entropy % BATTLE_BACKGROUND_COUNT);
}

BattleBackgroundId battleBackgroundNext(uint32_t *state, uint32_t entropy)
{
    if (state == NULL) {
        return battleBackgroundSelect(entropy);
    }
    *state = ((*state ^ entropy) * UINT32_C(1664525)) +
             UINT32_C(1013904223);
    return battleBackgroundSelect(*state);
}

void battleBackgroundReset(void)
{
    if (background_loaded) {
        int texture_id = background_image.textureID;
        glDeleteTextures(1, &texture_id);
    }
    background_image = (glImage){ 0 };
    background_loaded = false;
}

bool battleBackgroundLoad(BattleBackgroundId background)
{
    char image_path[72];
    char palette_path[72];

    if ((unsigned int)background >= BATTLE_BACKGROUND_COUNT) {
        return false;
    }
    battleBackgroundReset();
    int image_length = snprintf(image_path, sizeof(image_path),
                                "nitro:/backgrounds/%s.img.bin",
                                resource_names[background]);
    int palette_length = snprintf(palette_path, sizeof(palette_path),
                                  "nitro:/backgrounds/%s.pal.bin",
                                  resource_names[background]);
    if (image_length < 0 || (size_t)image_length >= sizeof(image_path) ||
        palette_length < 0 || (size_t)palette_length >= sizeof(palette_path) ||
        !readExactFile(image_path, image_buffer, sizeof(image_buffer)) ||
        !readExactFile(palette_path, palette_buffer, sizeof(palette_buffer))) {
        return false;
    }
    int texture_id = glLoadTileSet(
        &background_image, BACKGROUND_WIDTH, BACKGROUND_VISIBLE_HEIGHT,
        BACKGROUND_WIDTH, BACKGROUND_TEXTURE_HEIGHT, GL_RGB256,
        BACKGROUND_WIDTH, BACKGROUND_TEXTURE_HEIGHT, TEXGEN_TEXCOORD,
        BACKGROUND_PALETTE_ENTRIES, palette_buffer, image_buffer);
    if (texture_id < 0) {
        background_image = (glImage){ 0 };
        return false;
    }
    background_loaded = true;
    return true;
}

void battleBackgroundDraw(void)
{
    if (!background_loaded) {
        return;
    }
    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(0));
    glColor(RGB15(31, 31, 31));
    glSpriteScaleXY(0, 0, (int)(BACKGROUND_DRAW_SCALE << 4),
                    (int)(BACKGROUND_DRAW_SCALE << 4), GL_FLIP_NONE,
                    &background_image);
}
