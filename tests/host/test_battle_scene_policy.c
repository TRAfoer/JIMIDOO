#include <assert.h>
#include <stdbool.h>

#include "battle_scene_internal.h"

static void test_automatic_counter_presents_defender_hiss_once(void)
{
    BattleSceneRouteState route;
    BattlePresentation presentation;
    BattlePresentation output[2];
    BattleEvent event = {
        EVENT_HISS_SUCCESS, SIDE_AI, SIDE_PLAYER, 0
    };

    battleSceneRouteInit(&route);
    presentation = battleSceneRouteSubmitted(
        &route, SIDE_PLAYER, CMD_SCRATCH, true, false);
    assert(presentation.side == SIDE_PLAYER);
    assert(presentation.command == CMD_SCRATCH);
    assert(battleSceneRouteEvents(&route, &event, 1, output, 2) == 1);
    assert(output[0].side == SIDE_AI);
    assert(output[0].command == CMD_HISS);
}

static void test_explicit_hiss_event_does_not_double_present(void)
{
    BattleSceneRouteState route;
    BattlePresentation presentation;
    BattlePresentation output[2];
    BattleEvent event = {
        EVENT_HISS_SUCCESS, SIDE_PLAYER, SIDE_AI, 0
    };

    battleSceneRouteInit(&route);
    presentation = battleSceneRouteSubmitted(
        &route, SIDE_PLAYER, CMD_HISS, true, false);
    assert(presentation.side == SIDE_PLAYER);
    assert(presentation.command == CMD_HISS);
    assert(battleSceneRouteEvents(&route, &event, 1, output, 2) == 0);
    assert(!route.explicit_hiss_pending[SIDE_PLAYER]);
}

static void test_explicit_hiss_suppresses_its_own_later_result_only(void)
{
    BattleSceneRouteState route;
    BattlePresentation output[2];
    BattleEvent events[2] = {
        { EVENT_HISS_SUCCESS, SIDE_AI, SIDE_PLAYER, 0 },
        { EVENT_HISS_FAIL, SIDE_AI, SIDE_PLAYER, 0 }
    };

    battleSceneRouteInit(&route);
    (void)battleSceneRouteSubmitted(&route, SIDE_AI, CMD_HISS, true, false);
    assert(battleSceneRouteEvents(&route, events, 2, output, 2) == 1);
    assert(output[0].side == SIDE_AI);
    assert(output[0].command == CMD_HISS);
    assert(!route.explicit_hiss_pending[SIDE_AI]);
}

static void test_warning_scratch_waits_for_resolution(void)
{
    BattleSceneRouteState route;
    BattlePresentation presentation;
    BattlePresentation output[2];
    BattleEvent hit = { EVENT_HIT, SIDE_AI, SIDE_PLAYER, 16 };

    battleSceneRouteInit(&route);
    presentation = battleSceneRouteSubmitted(
        &route, SIDE_AI, CMD_SCRATCH, true, true);
    assert(presentation.command == CMD_NONE);
    assert(route.deferred_scratch[SIDE_AI]);
    assert(battleSceneRouteEvents(&route, &hit, 1, output, 2) == 1);
    assert(output[0].side == SIDE_AI);
    assert(output[0].command == CMD_SCRATCH);
    assert(!route.deferred_scratch[SIDE_AI]);
}

static void test_warning_dodge_still_presents_resolved_attack(void)
{
    BattleSceneRouteState route;
    BattlePresentation output[2];
    BattleEvent dodge = { EVENT_DODGE, SIDE_AI, SIDE_PLAYER, 0 };

    battleSceneRouteInit(&route);
    (void)battleSceneRouteSubmitted(&route, SIDE_AI, CMD_SCRATCH, true, true);
    assert(battleSceneRouteEvents(&route, &dodge, 1, output, 2) == 1);
    assert(output[0].side == SIDE_AI);
    assert(output[0].command == CMD_SCRATCH);
}

static void test_counter_cancels_warning_without_scratch_presentation(void)
{
    BattleSceneRouteState route;
    BattlePresentation output[2];
    BattleEvent counter = {
        EVENT_HISS_SUCCESS, SIDE_PLAYER, SIDE_AI, 0
    };

    battleSceneRouteInit(&route);
    (void)battleSceneRouteSubmitted(&route, SIDE_AI, CMD_SCRATCH, true, true);
    (void)battleSceneRouteSubmitted(&route, SIDE_PLAYER, CMD_HISS, true, false);
    assert(battleSceneRouteEvents(&route, &counter, 1, output, 2) == 0);
    assert(!route.deferred_scratch[SIDE_AI]);
    assert(!route.explicit_hiss_pending[SIDE_PLAYER]);
}

static void test_rejected_action_has_no_presentation_or_route_state(void)
{
    BattleSceneRouteState route;
    BattlePresentation presentation;

    battleSceneRouteInit(&route);
    presentation = battleSceneRouteSubmitted(
        &route, SIDE_AI, CMD_SCRATCH, false, true);
    assert(presentation.command == CMD_NONE);
    assert(!route.deferred_scratch[SIDE_AI]);
}

static void test_terminal_lifecycle_holds_exactly_sixty_more_frames(void)
{
    BattleSceneLifecycle lifecycle;
    unsigned int frame;

    battleSceneLifecycleInit(&lifecycle);
    assert(battleSceneLifecycleAfterFrame(&lifecycle, false));
    assert(!lifecycle.terminal_started);

    assert(battleSceneLifecycleAfterFrame(&lifecycle, true));
    assert(lifecycle.terminal_started);
    assert(lifecycle.terminal_frames_remaining == BATTLE_TERMINAL_HOLD_FRAMES);
    for (frame = 1u; frame < BATTLE_TERMINAL_HOLD_FRAMES; ++frame) {
        assert(battleSceneLifecycleAfterFrame(&lifecycle, true));
    }
    assert(!battleSceneLifecycleAfterFrame(&lifecycle, true));
    assert(lifecycle.terminal_frames_remaining == 0u);
}

int main(void)
{
    test_automatic_counter_presents_defender_hiss_once();
    test_explicit_hiss_event_does_not_double_present();
    test_explicit_hiss_suppresses_its_own_later_result_only();
    test_warning_scratch_waits_for_resolution();
    test_warning_dodge_still_presents_resolved_attack();
    test_counter_cancels_warning_without_scratch_presentation();
    test_rejected_action_has_no_presentation_or_route_state();
    test_terminal_lifecycle_holds_exactly_sixty_more_frames();
    return 0;
}
