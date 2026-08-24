#include "debug_scene.h"

#include <nds.h>

bool debugSceneEntryRequested(uint32_t keys_down, uint32_t keys_held)
{
    return debugTitleLaunchForKeys(keys_down, keys_held) ==
           DEBUG_TITLE_LAUNCH_MENU;
}

DebugTitleLaunch debugTitleLaunchForKeys(uint32_t keys_down,
                                        uint32_t keys_held)
{
    if ((keys_down & KEY_START) == 0u) {
        return DEBUG_TITLE_LAUNCH_NONE;
    }
    return (keys_held & KEY_L) != 0u ? DEBUG_TITLE_LAUNCH_MENU :
                                      DEBUG_TITLE_LAUNCH_QUICK_BATTLE;
}

DebugSceneCommand debugSceneKeyCommand(uint32_t keys_down)
{
    if ((keys_down & KEY_A) != 0u) {
        return DEBUG_COMMAND_START;
    }
    if ((keys_down & KEY_B) != 0u) {
        return DEBUG_COMMAND_BACK;
    }
    if ((keys_down & KEY_UP) != 0u) {
        return DEBUG_COMMAND_PLUS_10;
    }
    if ((keys_down & KEY_DOWN) != 0u) {
        return DEBUG_COMMAND_MINUS_10;
    }
    if ((keys_down & KEY_RIGHT) != 0u) {
        return DEBUG_COMMAND_PLUS_1;
    }
    if ((keys_down & KEY_LEFT) != 0u) {
        return DEBUG_COMMAND_MINUS_1;
    }
    return DEBUG_COMMAND_NONE;
}

DebugSceneCommand debugSceneTouchCommandAt(int x, int y)
{
    if (x < 0 || x >= 256 || y < 0 || y >= 192) {
        return DEBUG_COMMAND_NONE;
    }
    if (y >= 70 && y < 120) {
        if (x < 64) {
            return DEBUG_COMMAND_MINUS_10;
        }
        if (x < 128) {
            return DEBUG_COMMAND_MINUS_1;
        }
        if (x < 192) {
            return DEBUG_COMMAND_PLUS_1;
        }
        return DEBUG_COMMAND_PLUS_10;
    }
    if (y >= 128) {
        return x < 128 ? DEBUG_COMMAND_BACK : DEBUG_COMMAND_START;
    }
    return DEBUG_COMMAND_NONE;
}

uint8_t debugSceneApplyCommand(uint8_t crisis, DebugSceneCommand command)
{
    int value = crisis < 1u ? 1 : crisis;

    switch (command) {
        case DEBUG_COMMAND_MINUS_10:
            value -= 10;
            break;
        case DEBUG_COMMAND_MINUS_1:
            --value;
            break;
        case DEBUG_COMMAND_PLUS_1:
            ++value;
            break;
        case DEBUG_COMMAND_PLUS_10:
            value += 10;
            break;
        default:
            break;
    }
    if (value < 1) {
        value = 1;
    }
    if (value > 255) {
        value = 255;
    }
    return (uint8_t)value;
}
