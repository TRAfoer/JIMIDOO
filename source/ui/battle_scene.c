#include "battle_scene.h"

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

#ifdef __NDS__

#include <stddef.h>

#include <nds.h>

#include "ai.h"
#include "audio_service.h"
#include "battle_scene_internal.h"
#include "graphics_service.h"

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

static void submitAction(BattleState *battle, BattleAnimation *animation,
                         Side side, CatId cat, BattleCommand command)
{
    if (command != CMD_NONE && battleSubmit(battle, side, command)) {
        battleAnimationOnAction(animation, side, command);
        playActionAudio(cat, command);
    }
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
    BattleEvent events[BATTLE_PENDING_EVENT_CAPACITY];

    if (!setupIsValid(setup)) {
        return BATTLE_ABORTED;
    }

    catTexturesReset();
    if (!catTexturesLoad(setup->player_cat) ||
        !catTexturesLoad(setup->enemy_cat)) {
        return BATTLE_ABORTED;
    }

    battleInit(&battle, &setup->player, &setup->enemy, setup->seed);
    battleHudInit(&hud);
    battleAnimationInit(&animation);
    audioSetMusic(MUSIC_BATTLE);

    while (!battle.finished) {
        uint32_t keys_down;
        size_t event_count = 0u;

        swiWaitForVBlank();
        battleHudDraw(&hud, &battle);

        scanKeys();
        keys_down = keysDown();
        if ((keys_down & KEY_START) != 0u) {
            battle.paused = !battle.paused;
        }

        if (!battle.paused) {
            BattleCommand command = playerCommand(keys_down);
            BattleCommand ai_command;

            submitAction(&battle, &animation, SIDE_PLAYER,
                         setup->player_cat, command);
            ai_command = aiChoose(&battle, SIDE_AI, setup->crisis,
                                  battle.random, battle.random_context);
            submitAction(&battle, &animation, SIDE_AI,
                         setup->enemy_cat, ai_command);
            event_count = battleTick(&battle, events,
                                     BATTLE_PENDING_EVENT_CAPACITY);
            battleAnimationOnEvents(&animation, events, event_count);
            routeResultAudio(events, event_count, setup->player_cat,
                             setup->enemy_cat);
        }

        battleAnimationTick(&animation, battle.paused);
        audioUpdate();
        battleAnimationDraw(&animation, &battle, setup->player_cat,
                            setup->enemy_cat);
    }

    return battle.winner == SIDE_PLAYER ? BATTLE_PLAYER_WIN :
                                          BATTLE_PLAYER_DEAD;
}

#else

BattleResult battleSceneRun(const BattleSetup *setup)
{
    (void)setup;
    return BATTLE_ABORTED;
}

#endif
