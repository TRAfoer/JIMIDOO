#include <assert.h>
#include <string.h>

#include "battle_scene.h"
#include "battle_scene_internal.h"

static void test_status_and_out_of_bounds_are_not_commands(void)
{
    assert(touchCommandAt(0, 0) == CMD_NONE);
    assert(touchCommandAt(127, 63) == CMD_NONE);
    assert(touchCommandAt(255, 63) == CMD_NONE);
    assert(touchCommandAt(-1, 64) == CMD_NONE);
    assert(touchCommandAt(256, 64) == CMD_NONE);
    assert(touchCommandAt(0, -1) == CMD_NONE);
    assert(touchCommandAt(0, 192) == CMD_NONE);
}

static void test_top_button_row_maps_hiss_and_scratch(void)
{
    assert(touchCommandAt(64, 96) == CMD_HISS);
    assert(touchCommandAt(192, 96) == CMD_SCRATCH);
    assert(touchCommandAt(0, 64) == CMD_HISS);
    assert(touchCommandAt(127, 127) == CMD_HISS);
    assert(touchCommandAt(128, 64) == CMD_SCRATCH);
    assert(touchCommandAt(255, 127) == CMD_SCRATCH);
}

static void test_bottom_button_row_maps_yowl_and_heal(void)
{
    assert(touchCommandAt(64, 160) == CMD_YOWL);
    assert(touchCommandAt(192, 160) == CMD_HEAL);
    assert(touchCommandAt(0, 128) == CMD_YOWL);
    assert(touchCommandAt(127, 191) == CMD_YOWL);
    assert(touchCommandAt(128, 128) == CMD_HEAL);
    assert(touchCommandAt(255, 191) == CMD_HEAL);
}

static void test_cooldown_frames_round_up_to_visible_tenths(void)
{
    assert(battleHudCooldownTenths(0) == 0);
    assert(battleHudCooldownTenths(1) == 1);
    assert(battleHudCooldownTenths(6) == 1);
    assert(battleHudCooldownTenths(7) == 2);
    assert(battleHudCooldownTenths(60) == 10);
    assert(battleHudCooldownTenths(120) == 20);
}

static void test_hud_dirty_regions_follow_only_visible_changes(void)
{
    BattleHudSnapshot shown;
    BattleHudSnapshot next;

    memset(&shown, 0, sizeof(shown));
    shown.hp[SIDE_PLAYER] = 60;
    shown.hp[SIDE_AI] = 65;
    shown.rage[SIDE_PLAYER] = 10;
    shown.rage[SIDE_AI] = 20;
    shown.cooldown_tenths = 10;
    shown.stun = 60;
    shown.channel = CHANNEL_NONE;
    shown.available = false;
    next = shown;

    assert(battleHudDirtyRegions(&shown, &next, true, true) ==
           BATTLE_HUD_DIRTY_NONE);
    next.stun = 59;
    assert(battleHudDirtyRegions(&shown, &next, true, true) ==
           BATTLE_HUD_DIRTY_NONE);
    next = shown;
    next.stun = 0;
    assert(battleHudDirtyRegions(&shown, &next, true, true) ==
           (BATTLE_HUD_DIRTY_STATUS | BATTLE_HUD_DIRTY_BUTTONS));
    next = shown;
    next.hp[SIDE_PLAYER] = 59;
    assert(battleHudDirtyRegions(&shown, &next, true, true) ==
           BATTLE_HUD_DIRTY_STATUS);
    next = shown;
    next.cooldown_tenths = 9;
    assert(battleHudDirtyRegions(&shown, &next, true, true) ==
           BATTLE_HUD_DIRTY_BUTTONS);
    assert(battleHudDirtyRegions(&shown, &shown, false, false) ==
           (BATTLE_HUD_DIRTY_STATUS | BATTLE_HUD_DIRTY_BUTTONS));
}

int main(void)
{
    test_status_and_out_of_bounds_are_not_commands();
    test_top_button_row_maps_hiss_and_scratch();
    test_bottom_button_row_maps_yowl_and_heal();
    test_cooldown_frames_round_up_to_visible_tenths();
    test_hud_dirty_regions_follow_only_visible_changes();
    return 0;
}
