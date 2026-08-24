#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include <nds.h>

#include "font_layout.h"
#include "graphics_service.h"
#include "localization.h"
#include "ui_scene.h"

static unsigned int sub_clear_count;
static unsigned int sub_fill_count;
static unsigned int sub_text_count;
static unsigned int top_frame_count;
static unsigned int language_set_count;
static bool cat_load_succeeds;

int fontTextWidth(const char *text, unsigned int scale)
{
    (void)text;
    (void)scale;
    return 0;
}

void textSetLanguage(Language language)
{
    (void)language;
    ++language_set_count;
}

const char *textGet(GameTextId id)
{
    (void)id;
    return "title";
}

void graphicsFrameBegin(void)
{
    ++top_frame_count;
}

void graphicsFrameEnd(void)
{
}

void graphicsTopFillGradient(uint16_t top_left, uint16_t bottom_left,
                             uint16_t bottom_right, uint16_t top_right)
{
    (void)top_left;
    (void)bottom_left;
    (void)bottom_right;
    (void)top_right;
}

void graphicsTopFillRect(int x, int y, int width, int height, uint16_t color)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)color;
}

void graphicsSubClear(uint16_t color)
{
    (void)color;
    ++sub_clear_count;
}

void graphicsSubFillRect(int x, int y, int width, int height, uint16_t color)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)color;
    ++sub_fill_count;
}

void graphicsTextDrawTop(int x, int y, unsigned int scale, uint16_t color,
                         const char *text)
{
    (void)x;
    (void)y;
    (void)scale;
    (void)color;
    (void)text;
}

void graphicsTextDrawSub(int x, int y, unsigned int scale, uint16_t color,
                         const char *text)
{
    (void)x;
    (void)y;
    (void)scale;
    (void)color;
    (void)text;
    ++sub_text_count;
}

bool catTextureLoad(CatId cat, CatAction action)
{
    (void)cat;
    (void)action;
    return cat_load_succeeds;
}

void catTexturesReset(void)
{
}

void catTextureDraw(CatId cat, CatAction action, int x, int y,
                    bool horizontal_flip)
{
    (void)cat;
    (void)action;
    (void)x;
    (void)y;
    (void)horizontal_flip;
}

static void assertLowerDrawCount(unsigned int clears, unsigned int fills,
                                 unsigned int text)
{
    assert(sub_clear_count == clears);
    assert(sub_fill_count == fills);
    assert(sub_text_count == text);
}

int main(void)
{
    titleSceneDraw();
    assertLowerDrawCount(0, 0, 0);

    cat_load_succeeds = true;
    assert(titleSceneInit(true) == TITLE_SCENE_INIT_READY);
    titleSceneDraw();
    assertLowerDrawCount(1, 2, 2);
    assert(top_frame_count == 1);

    titleSceneDraw();
    assertLowerDrawCount(1, 2, 2);
    assert(top_frame_count == 2);

    titleSceneUpdate(0);
    titleSceneDraw();
    assertLowerDrawCount(1, 2, 2);
    assert(top_frame_count == 3);

    titleSceneUpdate(KEY_SELECT);
    assert(language_set_count == 2);
    titleSceneDraw();
    assertLowerDrawCount(2, 4, 4);
    assert(top_frame_count == 4);

    cat_load_succeeds = false;
    assert(titleSceneInit(false) == TITLE_SCENE_INIT_CAT_UNAVAILABLE);
    titleSceneDraw();
    assertLowerDrawCount(2, 4, 4);
    assert(top_frame_count == 4);
    return 0;
}
