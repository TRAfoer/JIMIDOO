#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <nds.h>

#include "debug_scene.h"
#include "font_layout.h"
#include "graphics_service.h"
#include "localization.h"

static bool load_results[2];
static unsigned int load_count;
static unsigned int reset_count;
static unsigned int cat_draw_count;
static bool enemy_flip;
static char top_text[12][64];
static char sub_text[12][64];
static unsigned int top_text_count;
static unsigned int sub_text_count;

int fontTextWidth(const char *text, unsigned int scale)
{
    (void)scale;
    return (int)strlen(text) * 4;
}

const char *textGet(GameTextId id)
{
    switch (id) {
        case TEXT_DEBUG_TITLE: return "Battle Debug";
        case TEXT_PREBATTLE_CRISIS: return "Crisis Level";
        case TEXT_DEBUG_BEST_RATE: return "Best-choice rate";
        case TEXT_MENU_BACK: return "Back";
        case TEXT_PREBATTLE_START: return "Start Challenge";
        case TEXT_DEBUG_CONTROLS: return "D-Pad adjust  A start  B back";
        default: return "?";
    }
}

bool catTextureLoad(CatId cat, CatAction action)
{
    assert(action == CAT_ACTION_IDLE);
    assert(cat == (load_count == 0u ? CAT_ORANGE : CAT_TABBY));
    return load_results[load_count++];
}

void catTexturesReset(void) { ++reset_count; }
void catTextureDraw(CatId cat, CatAction action, int x, int y,
                    bool horizontal_flip)
{
    (void)action; (void)x; (void)y;
    ++cat_draw_count;
    if (cat == CAT_TABBY) {
        enemy_flip = horizontal_flip;
    }
}

void graphicsFrameBegin(void) {}
void graphicsFrameEnd(void) {}
void graphicsTopFillGradient(uint16_t a, uint16_t b, uint16_t c, uint16_t d)
{ (void)a; (void)b; (void)c; (void)d; }
void graphicsTopFillRect(int x, int y, int w, int h, uint16_t color)
{ (void)x; (void)y; (void)w; (void)h; (void)color; }
void graphicsSubClear(uint16_t color) { (void)color; }
void graphicsSubFillRect(int x, int y, int w, int h, uint16_t color)
{ (void)x; (void)y; (void)w; (void)h; (void)color; }
void graphicsTextDrawTop(int x, int y, unsigned int scale, uint16_t color,
                         const char *text)
{
    (void)x; (void)y; (void)scale; (void)color;
    assert(top_text_count < 12u);
    strcpy_s(top_text[top_text_count++], 64u, text);
}
void graphicsTextDrawSub(int x, int y, unsigned int scale, uint16_t color,
                         const char *text)
{
    (void)x; (void)y; (void)scale; (void)color;
    assert(sub_text_count < 12u);
    strcpy_s(sub_text[sub_text_count++], 64u, text);
}

static bool contains(char values[][64], unsigned int count, const char *wanted)
{
    for (unsigned int index = 0u; index < count; ++index) {
        if (strcmp(values[index], wanted) == 0) return true;
    }
    return false;
}

int main(void)
{
    load_results[0] = true;
    load_results[1] = true;
    assert(debugScenePrepare());
    assert(load_count == 2u);
    assert(reset_count == 1u);

    debugSceneDraw(25u, true);
    assert(cat_draw_count == 2u);
    assert(enemy_flip);
    assert(contains(top_text, top_text_count, "25"));
    assert(contains(top_text, top_text_count, "55%"));
    assert(contains(sub_text, sub_text_count, "-10"));
    assert(contains(sub_text, sub_text_count, "-1"));
    assert(contains(sub_text, sub_text_count, "+1"));
    assert(contains(sub_text, sub_text_count, "+10"));
    assert(contains(sub_text, sub_text_count, "B Back"));
    assert(contains(sub_text, sub_text_count, "A Start Challenge"));

    load_count = 0u;
    load_results[0] = true;
    load_results[1] = false;
    assert(!debugScenePrepare());
    assert(reset_count == 3u);
    return 0;
}
