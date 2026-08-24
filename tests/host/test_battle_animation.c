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

int main(void)
{
    test_successful_hiss_clears_active_warning();
    test_failed_hiss_keeps_active_warning();
    test_hit_flash_blinks_the_sprite_without_an_opaque_backdrop();
    test_only_the_enemy_sprite_is_horizontally_flipped();
    return 0;
}
