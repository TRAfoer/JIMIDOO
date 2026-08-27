#include <assert.h>
#include <stdbool.h>

#include "ai.h"
#include "battle_scene_internal.h"

static const FighterSpec player = {
    60, 15, 5, 15, 100, 120, 0, 100, 0
};
static const FighterSpec enemy = {
    65, 16, 5, 12, 100, 120, 0, 0, 0
};

static void init_submission_fixture(BattleState *battle,
                                    BattleSceneRouteState *route,
                                    AiBrain *brain)
{
    battleInit(battle, &player, &enemy, UINT32_C(0x13579BDF));
    battleSceneRouteInit(route);
    aiBrainInit(brain, UINT32_C(0x2468ACE0));
}

static void test_rejected_submission_does_not_enter_ai_history(void)
{
    BattleState battle;
    BattleSceneRouteState route;
    BattlePresentation presentation = { SIDE_AI, CMD_HEAL };
    AiBrain brain;

    init_submission_fixture(&battle, &route, &brain);
    battle.fighter[SIDE_PLAYER].cooldown = 1u;
    assert(!battleSceneSubmit(&battle, &route, &brain, SIDE_PLAYER,
                              CMD_HISS, &presentation));
    assert(brain.memory.player_count == 0u);
    assert(presentation.side == SIDE_PLAYER);
    assert(presentation.command == CMD_NONE);
}

static void test_accepted_player_submission_records_newest_and_ends_waiting(void)
{
    BattleState battle;
    BattleSceneRouteState route;
    BattlePresentation presentation;
    AiBrain brain;

    init_submission_fixture(&battle, &route, &brain);
    brain.opening_waiting = true;
    assert(battleSceneSubmit(&battle, &route, &brain, SIDE_PLAYER,
                             CMD_HISS, &presentation));
    assert(brain.memory.player_count == 1u);
    assert(brain.memory.player[0] == CMD_HISS);
    assert(!brain.opening_waiting);
    assert(brain.opening_frames_remaining == 0u);
    assert(presentation.side == SIDE_PLAYER);
    assert(presentation.command == CMD_HISS);
}

static void test_accepted_ai_submission_updates_repetition(void)
{
    BattleState battle;
    BattleSceneRouteState route;
    BattlePresentation presentation;
    AiBrain brain;

    init_submission_fixture(&battle, &route, &brain);
    assert(battleSceneSubmit(&battle, &route, &brain, SIDE_AI, CMD_HISS,
                             &presentation));
    assert(brain.memory.last_ai_action == CMD_HISS);
    assert(brain.memory.ai_repeat_count == 1u);
    battle.fighter[SIDE_AI].cooldown = 0u;
    assert(battleSceneSubmit(&battle, &route, &brain, SIDE_AI, CMD_HISS,
                             &presentation));
    assert(brain.memory.ai_repeat_count == 2u);
}

static void test_deferred_scratch_records_but_has_no_immediate_presentation(void)
{
    BattleState battle;
    BattleSceneRouteState route;
    BattlePresentation presentation;
    AiBrain brain;

    init_submission_fixture(&battle, &route, &brain);
    assert(battleSceneSubmit(&battle, &route, &brain, SIDE_AI, CMD_SCRATCH,
                             &presentation));
    assert(brain.memory.last_ai_action == CMD_SCRATCH);
    assert(brain.memory.ai_repeat_count == 1u);
    assert(route.deferred_scratch[SIDE_AI]);
    assert(presentation.command == CMD_NONE);
}

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

static void test_post_tick_presentation_compensates_the_animation_tick(void)
{
    assert(battleScenePresentationCooldown(120u, false) == 120u);
    assert(battleScenePresentationCooldown(78u, true) == 79u);
    assert(battleScenePresentationCooldown(0u, true) == 0u);
}

int main(void)
{
    test_rejected_submission_does_not_enter_ai_history();
    test_accepted_player_submission_records_newest_and_ends_waiting();
    test_accepted_ai_submission_updates_repetition();
    test_deferred_scratch_records_but_has_no_immediate_presentation();
    test_automatic_counter_presents_defender_hiss_once();
    test_explicit_hiss_event_does_not_double_present();
    test_explicit_hiss_suppresses_its_own_later_result_only();
    test_warning_scratch_waits_for_resolution();
    test_warning_dodge_still_presents_resolved_attack();
    test_counter_cancels_warning_without_scratch_presentation();
    test_rejected_action_has_no_presentation_or_route_state();
    test_terminal_lifecycle_holds_exactly_sixty_more_frames();
    test_post_tick_presentation_compensates_the_animation_tick();
    return 0;
}
