#include <nds.h>

#include "audio_service.h"
#include "graphics_service.h"
#include "ui_scene.h"

int main(void)
{
    graphicsInit();
    bool audio_available = audioInit();
    if (audio_available) {
        audioSetMusic(MUSIC_MENU);
    }
    titleSceneInit(audio_available);

    while (1) {
        swiWaitForVBlank();
        scanKeys();
        titleSceneUpdate(keysDown());
        audioUpdate();
        titleSceneDraw();
    }
}
