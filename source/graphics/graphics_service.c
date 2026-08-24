#include "graphics_service.h"

#include <stddef.h>
#include <stdio.h>

#include <filesystem.h>
#include <gl2d.h>
#include <nds.h>

#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 192
#define FONT_TEXTURE_SIZE \
    (JIMIDOO_FONT_ATLAS_WIDTH * JIMIDOO_FONT_TEXTURE_HEIGHT)
#define FONT_PALETTE_ENTRIES 8

_Static_assert(FONT_TEXTURE_SIZE <= GRAPHICS_FONT_TEXTURE_MAX_BYTES,
               "runtime font texture exceeds its VRAM budget");
_Static_assert(GRAPHICS_TEXTURE_MAX_RESIDENT_BYTES <=
                   GRAPHICS_TEXTURE_VRAM_BYTES,
               "font and two-cat cache exceed texture VRAM");

static uint8_t font_texture_pixels[FONT_TEXTURE_SIZE]
    __attribute__((aligned(4)));
static uint16_t font_palette[FONT_PALETTE_ENTRIES]
    __attribute__((aligned(4)));
static int font_texture_id = -1;
static uint16_t *sub_pixels;

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

static bool loadRuntimeFont(void)
{
    if (JIMIDOO_FONT_ATLAS_HEIGHT > JIMIDOO_FONT_TEXTURE_HEIGHT ||
        !readExactFile("nitro:/fonts/jimidou_font.a5i3.bin",
                       font_texture_pixels, sizeof(font_texture_pixels)) ||
        !readExactFile("nitro:/fonts/jimidou_font.pal.bin",
                       font_palette, sizeof(font_palette))) {
        return false;
    }

    glGenTextures(1, &font_texture_id);
    glBindTexture(0, font_texture_id);
    if (!glTexImageNtr2D(GL_RGB8_A5, JIMIDOO_FONT_ATLAS_WIDTH,
                         JIMIDOO_FONT_TEXTURE_HEIGHT, TEXGEN_TEXCOORD,
                         font_texture_pixels, NULL) ||
        !glColorTableNtr(FONT_PALETTE_ENTRIES, font_palette)) {
        glDeleteTextures(1, &font_texture_id);
        font_texture_id = -1;
        return false;
    }
    return true;
}

bool graphicsInit(void)
{
    powerOn(POWER_ALL_2D | POWER_3D_CORE | POWER_MATRIX);
    lcdMainOnTop();

    glScreen2D();
    videoSetMode(MODE_0_3D);
    videoSetModeSub(MODE_5_2D | DISPLAY_BG2_ACTIVE);

    vramSetBankA(VRAM_A_TEXTURE_SLOT0);
    vramSetBankB(VRAM_B_TEXTURE_SLOT1);
    vramSetBankC(VRAM_C_SUB_BG_0x06200000);
    vramSetBankD(VRAM_D_TEXTURE_SLOT3);
    vramSetBankE(VRAM_E_TEX_PALETTE);

    int sub_background = bgInitSub(2, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
    sub_pixels = (uint16_t *)bgGetGfxPtr(sub_background);
    graphicsSubClear(RGB15(0, 0, 0));

    if (!nitroFSInit(NULL)) {
        return false;
    }
    return loadRuntimeFont();
}

void graphicsFrameBegin(void)
{
    glBegin2D();
}

void graphicsFrameEnd(void)
{
    glEnd2D();
    glFlush(0);
}

void graphicsTopFillGradient(uint16_t top_left, uint16_t bottom_left,
                             uint16_t bottom_right, uint16_t top_right)
{
    glBoxFilledGradient(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1,
                        top_left, bottom_left, bottom_right, top_right);
}

void graphicsTopFillRect(int x, int y, int width, int height, uint16_t color)
{
    if (width > 0 && height > 0) {
        glBoxFilled(x, y, x + width - 1, y + height - 1, color);
    }
}

void graphicsSubClear(uint16_t color)
{
    if (sub_pixels == NULL) {
        return;
    }
    uint16_t opaque = color | BIT(15);
    for (int index = 0; index < SCREEN_WIDTH * SCREEN_HEIGHT; ++index) {
        sub_pixels[index] = opaque;
    }
}

void graphicsSubFillRect(int x, int y, int width, int height, uint16_t color)
{
    if (sub_pixels == NULL || width <= 0 || height <= 0) {
        return;
    }

    int left = x < 0 ? 0 : x;
    int top = y < 0 ? 0 : y;
    int right = x + width > SCREEN_WIDTH ? SCREEN_WIDTH : x + width;
    int bottom = y + height > SCREEN_HEIGHT ? SCREEN_HEIGHT : y + height;
    uint16_t opaque = color | BIT(15);
    for (int row = top; row < bottom; ++row) {
        for (int column = left; column < right; ++column) {
            sub_pixels[row * SCREEN_WIDTH + column] = opaque;
        }
    }
}

void graphicsTextDrawTop(int x, int y, unsigned int scale, uint16_t color,
                         const char *text)
{
    if (font_texture_id < 0 || text == NULL || scale == 0) {
        return;
    }

    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(1));
    glColor(color);

    int cursor = x;
    uint32_t codepoint;
    while (fontUtf8Next(&text, &codepoint)) {
        const JimiDooGlyphMetric *glyph = fontGlyphFind(codepoint);
        if (glyph == NULL) {
            continue;
        }
        if (glyph->width > 0 && glyph->height > 0) {
            glImage image = {
                .width = glyph->width,
                .height = glyph->height,
                .u_off = glyph->x,
                .v_off = glyph->y,
                .textureID = font_texture_id
            };
            glSpriteScaleXY(cursor + fontScaleMetric(glyph->bearing_x, scale),
                            y + fontScaleMetric(glyph->bearing_y, scale),
                            (s32)(scale << 4), (s32)(scale << 4),
                            GL_FLIP_NONE, &image);
        }
        cursor += fontScaleMetric(glyph->advance_x, scale);
    }
}

static uint16_t alphaBlend(uint16_t destination, uint16_t source,
                           unsigned int alpha)
{
    unsigned int inverse = 31U - alpha;
    unsigned int red = (((source & 31U) * alpha) +
                        ((destination & 31U) * inverse) + 15U) / 31U;
    unsigned int green = ((((source >> 5) & 31U) * alpha) +
                          (((destination >> 5) & 31U) * inverse) + 15U) / 31U;
    unsigned int blue = ((((source >> 10) & 31U) * alpha) +
                         (((destination >> 10) & 31U) * inverse) + 15U) / 31U;
    return (uint16_t)(red | (green << 5) | (blue << 10) | BIT(15));
}

static void drawGlyphSub(int x, int y, unsigned int scale, uint16_t color,
                         const JimiDooGlyphMetric *glyph)
{
    int width = fontScaleMetric(glyph->width, scale);
    int height = fontScaleMetric(glyph->height, scale);
    if (width <= 0 || height <= 0) {
        return;
    }

    for (int destination_y = 0; destination_y < height; ++destination_y) {
        int screen_y = y + destination_y;
        if ((unsigned int)screen_y >= SCREEN_HEIGHT) {
            continue;
        }
        int source_y = glyph->y + destination_y * glyph->height / height;
        for (int destination_x = 0; destination_x < width; ++destination_x) {
            int screen_x = x + destination_x;
            if ((unsigned int)screen_x >= SCREEN_WIDTH) {
                continue;
            }
            int source_x = glyph->x + destination_x * glyph->width / width;
            unsigned int alpha =
                font_texture_pixels[source_y * JIMIDOO_FONT_ATLAS_WIDTH + source_x] >> 3;
            if (alpha != 0) {
                uint16_t *pixel = &sub_pixels[screen_y * SCREEN_WIDTH + screen_x];
                *pixel = alphaBlend(*pixel, color, alpha);
            }
        }
    }
}

void graphicsTextDrawSub(int x, int y, unsigned int scale, uint16_t color,
                         const char *text)
{
    if (sub_pixels == NULL || font_texture_id < 0 || text == NULL || scale == 0) {
        return;
    }

    int cursor = x;
    uint32_t codepoint;
    while (fontUtf8Next(&text, &codepoint)) {
        const JimiDooGlyphMetric *glyph = fontGlyphFind(codepoint);
        if (glyph == NULL) {
            continue;
        }
        drawGlyphSub(cursor + fontScaleMetric(glyph->bearing_x, scale),
                     y + fontScaleMetric(glyph->bearing_y, scale),
                     scale, color, glyph);
        cursor += fontScaleMetric(glyph->advance_x, scale);
    }
}
