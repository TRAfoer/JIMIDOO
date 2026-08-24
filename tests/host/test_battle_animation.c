#include <assert.h>

#include "battle_scene_internal.h"

static void test_successful_hiss_clears_active_warning(void)
{
    BattleAnimation animation;
    BattleEvent warning = {
        EVENT_WARNING, SIDE_AI, SIDE_PLAYER, BATTLE_WARNING_FRAMES
    };
    BattleEvent success = {
        EVENT_HISS_SUCCESS, SIDE_PLAYER, SIDE_AI, 0
    };

    battleAnimationInit(&animation);
    battleAnimationOnEvents(&animation, &warning, 1);
    assert(animation.warning_frames == BATTLE_WARNING_FRAMES);
    battleAnimationOnEvents(&animation, &success, 1);
    assert(animation.warning_frames == 0u);
}

static void test_failed_hiss_keeps_active_warning(void)
{
    BattleAnimation animation;
    BattleEvent warning = {
        EVENT_WARNING, SIDE_AI, SIDE_PLAYER, BATTLE_WARNING_FRAMES
    };
    BattleEvent fail = {
        EVENT_HISS_FAIL, SIDE_PLAYER, SIDE_AI, 0
    };

    battleAnimationInit(&animation);
    battleAnimationOnEvents(&animation, &warning, 1);
    battleAnimationOnEvents(&animation, &fail, 1);
    assert(animation.warning_frames == BATTLE_WARNING_FRAMES);
}

static void test_hit_flash_blinks_the_sprite_without_an_opaque_backdrop(void)
{
    assert(battleAnimationFighterVisible(0u, 0u));
    assert(!battleAnimationFighterVisible(6u, 1u));
    assert(battleAnimationFighterVisible(6u, 0u));
}

static void test_only_the_enemy_sprite_is_horizontally_flipped(void)
{
    assert(!battleAnimationHorizontalFlip(SIDE_PLAYER));
    assert(battleAnimationHorizontalFlip(SIDE_AI));
}

static void test_action_pose_covers_cooldown_without_stretching_motion(void)
{
    BattleAnimation animation;
    unsigned int frame;

    battleAnimationInit(&animation);
    battleAnimationOnAction(&animation, SIDE_PLAYER, CMD_SCRATCH, 120u);
    assert(animation.fighter[SIDE_PLAYER].action == CAT_ACTION_SCRATCH);
    assert(animation.fighter[SIDE_PLAYER].pose_frames == 120u);
    assert(animation.fighter[SIDE_PLAYER].effect_frames == 10u);

    for (frame = 0u; frame < 10u; ++frame) {
        battleAnimationTick(&animation, false);
    }
    assert(animation.fighter[SIDE_PLAYER].pose_frames == 110u);
    assert(animation.fighter[SIDE_PLAYER].effect_frames == 0u);
    assert(animation.fighter[SIDE_PLAYER].action == CAT_ACTION_SCRATCH);

    for (frame = 0u; frame < 110u; ++frame) {
        battleAnimationTick(&animation, false);
    }
    assert(animation.fighter[SIDE_PLAYER].pose_frames == 0u);
    assert(animation.fighter[SIDE_PLAYER].action == CAT_ACTION_IDLE);
}

static void test_zero_cooldown_hiss_keeps_the_minimum_action_pose(void)
{
    BattleAnimation animation;

    battleAnimationInit(&animation);
    battleAnimationOnAction(&animation, SIDE_AI, CMD_HISS, 0u);
    assert(animation.fighter[SIDE_AI].pose_frames == 10u);
    assert(animation.fighter[SIDE_AI].effect_frames == 10u);
}

static void test_hit_feedback_overrides_a_long_action_pose(void)
{
    BattleAnimation animation;
    BattleEvent hit = { EVENT_HIT, SIDE_AI, SIDE_PLAYER, 15 };

    battleAnimationInit(&animation);
    battleAnimationOnAction(&animation, SIDE_PLAYER, CMD_HEAL, 120u);
    battleAnimationOnEvents(&animation, &hit, 1u);
    assert(animation.fighter[SIDE_PLAYER].action == CAT_ACTION_HIT);
    assert(animation.fighter[SIDE_PLAYER].pose_frames == 8u);
    assert(animation.fighter[SIDE_PLAYER].effect_frames == 8u);
}

int main(void)
{
    test_successful_hiss_clears_active_warning();
    test_failed_hiss_keeps_active_warning();
    test_hit_flash_blinks_the_sprite_without_an_opaque_backdrop();
    test_only_the_enemy_sprite_is_horizontally_flipped();
    test_action_pose_covers_cooldown_without_stretching_motion();
    test_zero_cooldown_hiss_keeps_the_minimum_action_pose();
    test_hit_feedback_overrides_a_long_action_pose();
    return 0;
}
