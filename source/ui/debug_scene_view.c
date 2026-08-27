#include "debug_scene.h"

#include <stdio.h>

#include <nds.h>

#include "ai.h"
#include "audio_service.h"
#include "font_layout.h"
#include "graphics_service.h"
#include "localization.h"

#define SCREEN_WIDTH 256

static int centeredTextX(const char *text, unsigned int scale)
{
    return (SCREEN_WIDTH - fontTextWidth(text, scale)) / 2;
}

bool debugScenePrepare(void)
{
    catTexturesReset();
    if (!catTextureLoad(CAT_ORANGE, CAT_ACTION_IDLE) ||
        !catTextureLoad(CAT_TABBY, CAT_ACTION_IDLE)) {
        catTexturesReset();
        return false;
    }
    return true;
}

static void drawButton(int x, int y, int width, int height, uint16_t color,
                       const char *label)
{
    graphicsSubFillRect(x + 2, y + 2, width - 4, height - 4, color);
    graphicsTextDrawSub(x + (width - fontTextWidth(label, FONT_SCALE_HALF)) / 2,
                        y + (height - 8) / 2, FONT_SCALE_HALF,
                        RGB15(31, 29, 23), label);
}

static void drawSubscreen(uint8_t crisis)
{
    char value[8];
    char back[48];
    char start[48];
    uint16_t panel = RGB15(7, 9, 13);
    uint16_t button = RGB15(9, 14, 19);
    uint16_t accent = RGB15(6, 22, 22);

    snprintf(value, sizeof(value), "%u", (unsigned int)crisis);
    snprintf(back, sizeof(back), "B %s", textGet(TEXT_MENU_BACK));
    snprintf(start, sizeof(start), "A %s", textGet(TEXT_PREBATTLE_START));
    graphicsSubClear(RGB15(3, 4, 6));
    graphicsSubFillRect(8, 6, 240, 54, panel);
    graphicsTextDrawSub(centeredTextX(textGet(TEXT_PREBATTLE_CRISIS),
                                     FONT_SCALE_HALF),
                        13, FONT_SCALE_HALF, RGB15(23, 25, 27),
                        textGet(TEXT_PREBATTLE_CRISIS));
    graphicsTextDrawSub(centeredTextX(value, FONT_SCALE_ONE), 34,
                        FONT_SCALE_ONE, RGB15(31, 24, 5), value);
    drawButton(0, 70, 64, 50, button, "-10");
    drawButton(64, 70, 64, 50, button, "-1");
    drawButton(128, 70, 64, 50, button, "+1");
    drawButton(192, 70, 64, 50, button, "+10");
    drawButton(0, 128, 128, 64, button, back);
    drawButton(128, 128, 128, 64, accent, start);
}

void debugSceneDraw(uint8_t crisis, bool redraw_subscreen)
{
    char value[8];
    char percent[8];
    const char *title = textGet(TEXT_DEBUG_TITLE);
    const char *crisis_label = textGet(TEXT_PREBATTLE_CRISIS);
    const char *rate_label = textGet(TEXT_DEBUG_ACTION_CAP);
    const char *controls = textGet(TEXT_DEBUG_CONTROLS);
    uint16_t cream = RGB15(31, 29, 23);
    uint16_t gold = RGB15(31, 22, 5);

    snprintf(value, sizeof(value), "%u", (unsigned int)crisis);
    snprintf(percent, sizeof(percent), "%u%%",
             (unsigned int)aiActionProbabilityCap(crisis));
    if (redraw_subscreen) {
        drawSubscreen(crisis);
    }

    graphicsFrameBegin();
    graphicsTopFillGradient(RGB15(6, 8, 11), RGB15(2, 3, 5),
                            RGB15(7, 8, 9), RGB15(11, 12, 13));
    graphicsTopFillRect(0, 0, 256, 61, RGB15(3, 4, 6));
    graphicsTopFillRect(0, 166, 256, 26, RGB15(3, 4, 6));
    graphicsTextDrawTop(centeredTextX(title, FONT_SCALE_HALF), 5,
                        FONT_SCALE_HALF, gold, title);
    graphicsTextDrawTop(8, 25, FONT_SCALE_HALF, cream, crisis_label);
    graphicsTextDrawTop(8, 43, FONT_SCALE_HALF, gold, value);
    graphicsTextDrawTop(145, 25, FONT_SCALE_HALF, cream, rate_label);
    graphicsTextDrawTop(190, 43, FONT_SCALE_HALF, gold, percent);
    catTextureDraw(CAT_ORANGE, CAT_ACTION_IDLE, 0, 55, false);
    catTextureDraw(CAT_TABBY, CAT_ACTION_IDLE, 128, 55, true);
    graphicsTextDrawTop(centeredTextX(controls, FONT_SCALE_HALF), 174,
                        FONT_SCALE_HALF, cream, controls);
    graphicsFrameEnd();
}

#ifdef __NDS__

DebugSceneResult debugSceneRun(uint8_t *crisis)
{
    bool dirty = true;

    if (crisis == NULL || !debugScenePrepare()) {
        return DEBUG_SCENE_UNAVAILABLE;
    }
    *crisis = debugSceneApplyCommand(*crisis, DEBUG_COMMAND_NONE);
    while (1) {
        DebugSceneCommand command;
        uint8_t adjusted;

        swiWaitForVBlank();
        scanKeys();
        uint32_t keys_down = keysDown();
        command = debugSceneKeyCommand(keys_down);
        if (command == DEBUG_COMMAND_NONE &&
            (keys_down & KEY_TOUCH) != 0u) {
            touchPosition touch;
            touchRead(&touch);
            command = debugSceneTouchCommandAt(touch.px, touch.py);
        }
        if (command == DEBUG_COMMAND_START) {
            return DEBUG_SCENE_START;
        }
        if (command == DEBUG_COMMAND_BACK) {
            return DEBUG_SCENE_BACK;
        }
        adjusted = debugSceneApplyCommand(*crisis, command);
        dirty = dirty || adjusted != *crisis;
        *crisis = adjusted;
        audioUpdate();
        debugSceneDraw(*crisis, dirty);
        dirty = false;
    }
}

#else

DebugSceneResult debugSceneRun(uint8_t *crisis)
{
    (void)crisis;
    return DEBUG_SCENE_UNAVAILABLE;
}

#endif
