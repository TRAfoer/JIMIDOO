#ifndef UI_SCENE_H
#define UI_SCENE_H

#include <stdbool.h>
#include <stdint.h>

void titleSceneInit(bool audio_available);
void titleSceneUpdate(uint32_t keys_down);
void titleSceneDraw(void);

#endif
