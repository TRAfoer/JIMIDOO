#include "ui_scene.h"

#include <nds.h>

#include "font_layout.h"
#include "graphics_service.h"
#include "localization.h"

#define SCREEN_WIDTH 256

static Language title_language;
static bool title_audio_muted;

static int centeredTextX(const char *text, unsigned int scale)
{
    return (SCREEN_WIDTH - fontTextWidth(text, scale)) / 2;
}

void titleSceneInit(bool audio_available)
{
    title_language = LANG_ZH_CN;
    title_audio_muted = !audio_available;
    textSetLanguage(title_language);
    catTextureLoad(CAT_ORANGE, CAT_ACTION_IDLE);
}

void titleSceneUpdate(uint32_t keys_down)
{
    if ((keys_down & KEY_SELECT) != 0) {
        title_language = title_language == LANG_ZH_CN ? LANG_EN : LANG_ZH_CN;
        textSetLanguage(title_language);
    }
}

void titleSceneDraw(void)
{
    const char *title = textGet(TEXT_GAME_TITLE);
    const char *toggle = textGet(TEXT_TITLE_LANGUAGE_TOGGLE);
    const char *muted = textGet(TEXT_ERROR_AUDIO_MUTED);
    uint16_t cream = RGB15(31, 29, 23);
    uint16_t gold = RGB15(31, 20, 5);
    uint16_t ink = RGB15(4, 3, 8);
    uint16_t panel = RGB15(9, 5, 13);

    graphicsSubClear(RGB15(5, 3, 9));
    graphicsSubFillRect(12, 18, 232, 156, panel);
    graphicsSubFillRect(12, 18, 232, 3, gold);
    graphicsTextDrawSub(centeredTextX(title, FONT_SCALE_ONE), 38,
                        FONT_SCALE_ONE, cream, title);
    graphicsTextDrawSub(centeredTextX(toggle, FONT_SCALE_HALF), 104,
                        FONT_SCALE_HALF, gold, toggle);
    if (title_audio_muted) {
        graphicsTextDrawSub(centeredTextX(muted, FONT_SCALE_HALF), 142,
                            FONT_SCALE_HALF, gold, muted);
    }

    graphicsFrameBegin();
    graphicsTopFillGradient(RGB15(8, 3, 12), RGB15(3, 2, 7),
                            RGB15(12, 5, 2), RGB15(18, 7, 3));
    graphicsTopFillRect(20, 4, 216, 36, ink);
    catTextureDraw(CAT_ORANGE, CAT_ACTION_IDLE, 64, 43, false);
    graphicsTextDrawTop(centeredTextX(title, FONT_SCALE_ONE), 7,
                        FONT_SCALE_ONE, cream, title);
    graphicsTextDrawTop(centeredTextX(toggle, FONT_SCALE_HALF), 168,
                        FONT_SCALE_HALF, gold, toggle);
    if (title_audio_muted) {
        graphicsTextDrawTop(centeredTextX(muted, FONT_SCALE_HALF), 145,
                            FONT_SCALE_HALF, gold, muted);
    }
    graphicsFrameEnd();
}
