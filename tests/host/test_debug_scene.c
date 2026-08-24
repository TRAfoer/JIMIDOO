#include <assert.h>
#include <stdint.h>

#include <nds.h>

#include "debug_scene.h"

static void test_l_start_entry_does_not_replace_plain_start(void)
{
    assert(debugSceneEntryRequested(KEY_START, KEY_START | KEY_L));
    assert(!debugSceneEntryRequested(KEY_START, KEY_START));
    assert(!debugSceneEntryRequested(KEY_L, KEY_START | KEY_L));
    assert(debugTitleLaunchForKeys(KEY_START, KEY_START | KEY_L) ==
           DEBUG_TITLE_LAUNCH_MENU);
    assert(debugTitleLaunchForKeys(KEY_START, KEY_START) ==
           DEBUG_TITLE_LAUNCH_QUICK_BATTLE);
    assert(debugTitleLaunchForKeys(KEY_L, KEY_START | KEY_L) ==
           DEBUG_TITLE_LAUNCH_NONE);
}

static void test_physical_and_touch_controls_route_to_the_same_commands(void)
{
    assert(debugSceneKeyCommand(KEY_DOWN) == DEBUG_COMMAND_MINUS_10);
    assert(debugSceneKeyCommand(KEY_LEFT) == DEBUG_COMMAND_MINUS_1);
    assert(debugSceneKeyCommand(KEY_RIGHT) == DEBUG_COMMAND_PLUS_1);
    assert(debugSceneKeyCommand(KEY_UP) == DEBUG_COMMAND_PLUS_10);
    assert(debugSceneKeyCommand(KEY_B) == DEBUG_COMMAND_BACK);
    assert(debugSceneKeyCommand(KEY_A) == DEBUG_COMMAND_START);

    assert(debugSceneTouchCommandAt(32, 90) == DEBUG_COMMAND_MINUS_10);
    assert(debugSceneTouchCommandAt(96, 90) == DEBUG_COMMAND_MINUS_1);
    assert(debugSceneTouchCommandAt(160, 90) == DEBUG_COMMAND_PLUS_1);
    assert(debugSceneTouchCommandAt(224, 90) == DEBUG_COMMAND_PLUS_10);
    assert(debugSceneTouchCommandAt(64, 150) == DEBUG_COMMAND_BACK);
    assert(debugSceneTouchCommandAt(192, 150) == DEBUG_COMMAND_START);
    assert(debugSceneTouchCommandAt(10, 40) == DEBUG_COMMAND_NONE);
}

static void test_crisis_adjustment_clamps_to_one_through_255(void)
{
    assert(debugSceneApplyCommand(1u, DEBUG_COMMAND_MINUS_1) == 1u);
    assert(debugSceneApplyCommand(5u, DEBUG_COMMAND_MINUS_10) == 1u);
    assert(debugSceneApplyCommand(100u, DEBUG_COMMAND_MINUS_10) == 90u);
    assert(debugSceneApplyCommand(100u, DEBUG_COMMAND_PLUS_1) == 101u);
    assert(debugSceneApplyCommand(250u, DEBUG_COMMAND_PLUS_10) == 255u);
    assert(debugSceneApplyCommand(255u, DEBUG_COMMAND_PLUS_1) == 255u);
}

int main(void)
{
    test_l_start_entry_does_not_replace_plain_start();
    test_physical_and_touch_controls_route_to_the_same_commands();
    test_crisis_adjustment_clamps_to_one_through_255();
    return 0;
}
