#ifndef FONT_LAYOUT_H
#define FONT_LAYOUT_H

#include <stdbool.h>
#include <stdint.h>

#include "generated/jimidou_font_metrics.h"

#define FONT_SCALE_ONE 256U
#define FONT_SCALE_HALF 128U

bool fontUtf8Next(const char **text, uint32_t *codepoint);
const JimiDooGlyphMetric *fontGlyphFind(uint32_t codepoint);
int fontScaleMetric(int value, unsigned int scale);
int fontTextWidth(const char *text, unsigned int scale);

#endif
