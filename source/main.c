#include <nds.h>

#include "audio_service.h"
#include "graphics_service.h"
#include "ui_scene.h"

static void runSafeLoop(bool audio_available) __attribute__((noreturn));

static void runSafeLoop(bool audio_available)
{
    while (1) {
        swiWaitForVBlank();
        if (audio_available) {
            audioUpdate();
        }
    }
}

int main(void)
{
    if (!graphicsInit()) {
        runSafeLoop(false);
    }

    bool audio_available = audioInit();
    TitleSceneInitStatus title_status = titleSceneInit(audio_available);
    if (!titleSceneCanRun(title_status)) {
        runSafeLoop(audio_available);
    }

    if (audio_available) {
        audioSetMusic(MUSIC_MENU);
    }

    while (1) {
        swiWaitForVBlank();
        scanKeys();
        titleSceneUpdate(keysDown());
        audioUpdate();
        titleSceneDraw();
    }
}
