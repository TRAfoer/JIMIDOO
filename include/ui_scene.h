#ifndef UI_SCENE_H
#define UI_SCENE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum TitleSceneInitStatus {
    TITLE_SCENE_INIT_READY,
    TITLE_SCENE_INIT_CAT_UNAVAILABLE
} TitleSceneInitStatus;

TitleSceneInitStatus titleSceneStatusForCatLoad(bool cat_loaded);
bool titleSceneCanRun(TitleSceneInitStatus status);
TitleSceneInitStatus titleSceneInit(bool audio_available);
void titleSceneUpdate(uint32_t keys_down);
void titleSceneDraw(void);

#endif
