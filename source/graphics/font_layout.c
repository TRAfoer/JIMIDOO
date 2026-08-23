#include "font_layout.h"

#include <stddef.h>

static bool isContinuation(unsigned char value)
{
    return (value & 0xC0U) == 0x80U;
}

bool fontUtf8Next(const char **text, uint32_t *codepoint)
{
    if (text == NULL || *text == NULL || codepoint == NULL || **text == '\0') {
        return false;
    }

    const unsigned char *input = (const unsigned char *)*text;
    uint32_t value;
    unsigned int length;

    if (input[0] < 0x80U) {
        value = input[0];
        length = 1;
    }
    else if (input[0] >= 0xC2U && input[0] <= 0xDFU &&
             input[1] != '\0' && isContinuation(input[1])) {
        value = ((uint32_t)(input[0] & 0x1FU) << 6) |
            (uint32_t)(input[1] & 0x3FU);
        length = 2;
    }
    else if (input[0] >= 0xE0U && input[0] <= 0xEFU &&
             input[1] != '\0' && input[2] != '\0' &&
             isContinuation(input[1]) && isContinuation(input[2]) &&
             !(input[0] == 0xE0U && input[1] < 0xA0U) &&
             !(input[0] == 0xEDU && input[1] >= 0xA0U)) {
        value = ((uint32_t)(input[0] & 0x0FU) << 12) |
            ((uint32_t)(input[1] & 0x3FU) << 6) |
            (uint32_t)(input[2] & 0x3FU);
        length = 3;
    }
    else if (input[0] >= 0xF0U && input[0] <= 0xF4U &&
             input[1] != '\0' && input[2] != '\0' && input[3] != '\0' &&
             isContinuation(input[1]) && isContinuation(input[2]) &&
             isContinuation(input[3]) &&
             !(input[0] == 0xF0U && input[1] < 0x90U) &&
             !(input[0] == 0xF4U && input[1] >= 0x90U)) {
        value = ((uint32_t)(input[0] & 0x07U) << 18) |
            ((uint32_t)(input[1] & 0x3FU) << 12) |
            ((uint32_t)(input[2] & 0x3FU) << 6) |
            (uint32_t)(input[3] & 0x3FU);
        length = 4;
    }
    else {
        value = UINT32_C(0xFFFD);
        length = 1;
    }

    *text += length;
    *codepoint = value;
    return true;
}

const JimiDooGlyphMetric *fontGlyphFind(uint32_t codepoint)
{
    unsigned int low = 0;
    unsigned int high = JIMIDOO_FONT_GLYPH_COUNT;

    while (low < high) {
        unsigned int middle = low + (high - low) / 2U;
        uint32_t candidate = jimidou_font_glyphs[middle].codepoint;
        if (candidate < codepoint) {
            low = middle + 1U;
        }
        else {
            high = middle;
        }
    }

    if (low < JIMIDOO_FONT_GLYPH_COUNT &&
        jimidou_font_glyphs[low].codepoint == codepoint) {
        return &jimidou_font_glyphs[low];
    }
    return NULL;
}

int fontScaleMetric(int value, unsigned int scale)
{
    if (value < 0) {
        return -fontScaleMetric(-value, scale);
    }
    return (int)(((unsigned int)value * scale + FONT_SCALE_ONE / 2U) /
                 FONT_SCALE_ONE);
}

int fontTextWidth(const char *text, unsigned int scale)
{
    int width = 0;
    uint32_t codepoint;

    while (fontUtf8Next(&text, &codepoint)) {
        const JimiDooGlyphMetric *glyph = fontGlyphFind(codepoint);
        if (glyph != NULL) {
            width += fontScaleMetric(glyph->advance_x, scale);
        }
    }
    return width;
}
