#include "battle_scene.h"

#include <string.h>

#include "battle_scene_internal.h"

BattleCommand touchCommandAt(int x, int y)
{
    if (x < 0 || x >= 256 || y < 64 || y >= 192) {
        return CMD_NONE;
    }
    if (y < 128) {
        return x < 128 ? CMD_HISS : CMD_SCRATCH;
    }
    return x < 128 ? CMD_YOWL : CMD_HEAL;
}

static bool sceneSideIsValid(Side side)
{
    return side == SIDE_PLAYER || side == SIDE_AI;
}

static BattlePresentation noPresentation(void)
{
    BattlePresentation presentation = { SIDE_PLAYER, CMD_NONE };

    return presentation;
}

void battleSceneRouteInit(BattleSceneRouteState *route)
{
    if (route != NULL) {
        memset(route, 0, sizeof(*route));
    }
}

BattlePresentation battleSceneRouteSubmitted(BattleSceneRouteState *route,
                                              Side side,
                                              BattleCommand command,
                                              bool accepted,
                                              bool scratch_deferred)
{
    BattlePresentation presentation = noPresentation();

    if (route == NULL || !sceneSideIsValid(side) || !accepted ||
        command == CMD_NONE) {
        return presentation;
    }
    if (command == CMD_HISS) {
        route->explicit_hiss_pending[side] = true;
    }
    if (command == CMD_SCRATCH && scratch_deferred) {
        route->deferred_scratch[side] = true;
        return presentation;
    }
    presentation.side = side;
    presentation.command = command;
    return presentation;
}

static void appendPresentation(BattlePresentation *presentations,
                               size_t presentation_capacity,
                               size_t *presentation_count, Side side,
                               BattleCommand command)
{
    if (*presentation_count < presentation_capacity) {
        presentations[*presentation_count].side = side;
        presentations[*presentation_count].command = command;
        ++*presentation_count;
    }
}

size_t battleSceneRouteEvents(BattleSceneRouteState *route,
                              const BattleEvent *events, size_t event_count,
                              BattlePresentation *presentations,
                              size_t presentation_capacity)
{
    size_t event_index;
    size_t presentation_count = 0u;
    size_t explicit_result_index[SIDE_COUNT] = { event_count, event_count };

    if (route == NULL || (events == NULL && event_count != 0u) ||
        (presentations == NULL && presentation_capacity != 0u)) {
        return 0u;
    }
    for (event_index = 0u; event_index < event_count; ++event_index) {
        const BattleEvent *event = &events[event_index];

        if (sceneSideIsValid(event->source) &&
            route->explicit_hiss_pending[event->source] &&
            (event->type == EVENT_HISS_SUCCESS ||
             event->type == EVENT_HISS_FAIL)) {
            explicit_result_index[event->source] = event_index;
        }
    }
    for (event_index = 0u; event_index < event_count; ++event_index) {
        const BattleEvent *event = &events[event_index];

        if (!sceneSideIsValid(event->source) ||
            !sceneSideIsValid(event->target)) {
            continue;
        }
        if ((event->type == EVENT_HIT || event->type == EVENT_DODGE) &&
            route->deferred_scratch[event->source]) {
            route->deferred_scratch[event->source] = false;
            appendPresentation(presentations, presentation_capacity,
                               &presentation_count, event->source,
                               CMD_SCRATCH);
        } else if (event->type == EVENT_HISS_SUCCESS) {
            route->deferred_scratch[event->target] = false;
            if (explicit_result_index[event->source] == event_index) {
                route->explicit_hiss_pending[event->source] = false;
            } else {
                appendPresentation(presentations, presentation_capacity,
                                   &presentation_count, event->source,
                                   CMD_HISS);
            }
        } else if (event->type == EVENT_HISS_FAIL &&
                   explicit_result_index[event->source] == event_index) {
            route->explicit_hiss_pending[event->source] = false;
        }
    }
    return presentation_count;
}

void battleSceneLifecycleInit(BattleSceneLifecycle *lifecycle)
{
    if (lifecycle != NULL) {
        memset(lifecycle, 0, sizeof(*lifecycle));
    }
}

bool battleSceneLifecycleAfterFrame(BattleSceneLifecycle *lifecycle,
                                    bool battle_finished)
{
    if (lifecycle == NULL) {
        return false;
    }
    if (!battle_finished) {
        return true;
    }
    if (!lifecycle->terminal_started) {
        lifecycle->terminal_started = true;
        lifecycle->terminal_frames_remaining = BATTLE_TERMINAL_HOLD_FRAMES;
        return true;
    }
    if (lifecycle->terminal_frames_remaining > 0u) {
        --lifecycle->terminal_frames_remaining;
    }
    return lifecycle->terminal_frames_remaining != 0u;
}

uint32_t battleScenePresentationCooldown(uint32_t cooldown_frames,
                                         bool tick_already_advanced)
{
    if (tick_already_advanced && cooldown_frames != 0u &&
        cooldown_frames != UINT32_MAX) {
        return cooldown_frames + 1u;
    }
    return cooldown_frames;
}

#ifdef __NDS__

#include <stddef.h>

#include <nds.h>

#include "ai.h"
#include "audio_service.h"
#include "graphics_service.h"

static uint32_t background_random_state = UINT32_C(0x4261636B);

static bool setupIsValid(const BattleSetup *setup)
{
    return setup != NULL && (unsigned int)setup->player_cat < CAT_COUNT &&
           (unsigned int)setup->enemy_cat < CAT_COUNT &&
           setup->player.max_hp > 0 && setup->enemy.max_hp > 0;
}

static BattleCommand physicalCommand(uint32_t keys_down)
{
    if ((keys_down & KEY_L) != 0u) {
        return CMD_HISS;
    }
    if ((keys_down & KEY_R) != 0u) {
        return CMD_SCRATCH;
    }
    if ((keys_down & KEY_Y) != 0u) {
        return CMD_YOWL;
    }
    if ((keys_down & KEY_A) != 0u) {
        return CMD_HEAL;
    }
    return CMD_NONE;
}

static BattleCommand playerCommand(uint32_t keys_down)
{
    BattleCommand command = physicalCommand(keys_down);

    if (command == CMD_NONE && (keys_down & KEY_TOUCH) != 0u) {
        touchPosition touch;

        touchRead(&touch);
        command = touchCommandAt(touch.px, touch.py);
    }
    return command;
}

static void playActionAudio(CatId cat, BattleCommand command)
{
    switch (command) {
        case CMD_HISS:
            audioPlayHiss(cat);
            break;
        case CMD_SCRATCH:
            audioPlayScratch(cat);
            break;
        case CMD_YOWL:
            audioPlaySfx(SFX_ID_YOWL);
            break;
        case CMD_HEAL:
            audioPlayHeal(cat);
            break;
        default:
            break;
    }
}

static void presentAction(const BattleState *battle, BattleAnimation *animation,
                          const BattlePresentation *presentation,
                          CatId player_cat, CatId enemy_cat,
                          bool tick_already_advanced)
{
    CatId cat;

    if (battle == NULL || presentation == NULL ||
        presentation->command == CMD_NONE) {
        return;
    }
    cat = presentation->side == SIDE_PLAYER ? player_cat : enemy_cat;
    battleAnimationOnAction(animation, presentation->side,
                            presentation->command,
                            battleScenePresentationCooldown(
                                battle->fighter[presentation->side].cooldown,
                                tick_already_advanced));
    playActionAudio(cat, presentation->command);
}

static void submitAction(BattleState *battle, BattleAnimation *animation,
                         BattleSceneRouteState *route, Side side,
                         CatId player_cat, CatId enemy_cat,
                         BattleCommand command)
{
    bool accepted = command != CMD_NONE && battleSubmit(battle, side, command);
    bool deferred = accepted && command == CMD_SCRATCH &&
                    battle->pending_scratch_frames != 0u &&
                    battle->pending_scratch_source == side;
    BattlePresentation presentation = battleSceneRouteSubmitted(
        route, side, command, accepted, deferred);

    presentAction(battle, animation, &presentation, player_cat, enemy_cat,
                  false);
}

static void routeResultAudio(const BattleEvent *events, size_t event_count,
                             CatId player_cat, CatId enemy_cat)
{
    size_t index;

    for (index = 0u; index < event_count; ++index) {
        if (events[index].type == EVENT_BATTLE_END) {
            audioPlayDeath(events[index].target == SIDE_PLAYER ? player_cat :
                                                                enemy_cat);
        }
    }
}

BattleResult battleSceneRun(const BattleSetup *setup)
{
    BattleState battle;
    BattleHud hud;
    BattleAnimation animation;
    BattleSceneRouteState route;
    BattleSceneLifecycle lifecycle;
    BattleEvent events[BATTLE_PENDING_EVENT_CAPACITY];
    bool keep_running = true;
    BattleBackgroundId background;

    if (!setupIsValid(setup)) {
        return BATTLE_ABORTED;
    }

    background = battleBackgroundNext(&background_random_state, setup->seed);
    catTexturesReset();
    if (!battleBackgroundLoad(background) ||
        !catTexturesLoad(setup->player_cat) ||
        !catTexturesLoad(setup->enemy_cat)) {
        battleBackgroundReset();
        return BATTLE_ABORTED;
    }

    battleInit(&battle, &setup->player, &setup->enemy, setup->seed);
    battleHudInit(&hud);
    battleAnimationInit(&animation);
    battleSceneRouteInit(&route);
    battleSceneLifecycleInit(&lifecycle);
    audioSetMusic(MUSIC_BATTLE);

    while (keep_running) {
        uint32_t keys_down;
        size_t event_count = 0u;

        swiWaitForVBlank();
        battleHudDraw(&hud, &battle);

        if (!battle.finished) {
            BattleCommand command;
            BattleCommand ai_command;
            BattlePresentation presentations[4];
            size_t presentation_count;
            size_t presentation_index;

            scanKeys();
            keys_down = keysDown();
            if ((keys_down & KEY_START) != 0u) {
                battle.paused = !battle.paused;
            }

            if (!battle.paused) {
                command = playerCommand(keys_down);

                submitAction(&battle, &animation, &route, SIDE_PLAYER,
                             setup->player_cat, setup->enemy_cat, command);
                ai_command = aiChoose(&battle, SIDE_AI, setup->crisis,
                                      battle.random, battle.random_context);
                submitAction(&battle, &animation, &route, SIDE_AI,
                             setup->player_cat, setup->enemy_cat, ai_command);
                event_count = battleTick(&battle, events,
                                         BATTLE_PENDING_EVENT_CAPACITY);
                presentation_count = battleSceneRouteEvents(
                    &route, events, event_count, presentations,
                    sizeof(presentations) / sizeof(presentations[0]));
                for (presentation_index = 0u;
                     presentation_index < presentation_count;
                     ++presentation_index) {
                    presentAction(&battle, &animation,
                                  &presentations[presentation_index],
                                  setup->player_cat, setup->enemy_cat, true);
                }
                battleAnimationOnEvents(&animation, events, event_count);
                routeResultAudio(events, event_count, setup->player_cat,
                                 setup->enemy_cat);
            }
        }

        battleAnimationTick(&animation, battle.paused);
        audioUpdate();
        battleAnimationDraw(&animation, &battle, setup->player_cat,
                            setup->enemy_cat);
        keep_running = battleSceneLifecycleAfterFrame(&lifecycle,
                                                      battle.finished);
    }

    BattleResult result = battle.winner == SIDE_PLAYER ? BATTLE_PLAYER_WIN :
                                                         BATTLE_PLAYER_DEAD;
    battleBackgroundReset();
    return result;
}

#else

BattleResult battleSceneRun(const BattleSetup *setup)
{
    (void)setup;
    return BATTLE_ABORTED;
}

#endif
