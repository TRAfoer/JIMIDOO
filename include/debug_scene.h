#ifndef DEBUG_SCENE_H
#define DEBUG_SCENE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum DebugSceneCommand {
    DEBUG_COMMAND_NONE,
    DEBUG_COMMAND_MINUS_10,
    DEBUG_COMMAND_MINUS_1,
    DEBUG_COMMAND_PLUS_1,
    DEBUG_COMMAND_PLUS_10,
    DEBUG_COMMAND_BACK,
    DEBUG_COMMAND_START
} DebugSceneCommand;

typedef enum DebugSceneResult {
    DEBUG_SCENE_UNAVAILABLE,
    DEBUG_SCENE_BACK,
    DEBUG_SCENE_START
} DebugSceneResult;

typedef enum DebugTitleLaunch {
    DEBUG_TITLE_LAUNCH_NONE,
    DEBUG_TITLE_LAUNCH_QUICK_BATTLE,
    DEBUG_TITLE_LAUNCH_MENU
} DebugTitleLaunch;

bool debugSceneEntryRequested(uint32_t keys_down, uint32_t keys_held);
DebugTitleLaunch debugTitleLaunchForKeys(uint32_t keys_down,
                                        uint32_t keys_held);
DebugSceneCommand debugSceneKeyCommand(uint32_t keys_down);
DebugSceneCommand debugSceneTouchCommandAt(int x, int y);
uint8_t debugSceneApplyCommand(uint8_t crisis, DebugSceneCommand command);
bool debugScenePrepare(void);
void debugSceneDraw(uint8_t crisis, bool redraw_subscreen);
DebugSceneResult debugSceneRun(uint8_t *crisis);

#endif
