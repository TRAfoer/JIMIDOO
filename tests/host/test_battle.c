#include "battle.h"
#include "battle_rng.h"

#include <assert.h>
#include <stddef.h>

static const FighterSpec orange = { 60, 15, 5, 15, 100, 120, 30, 30, 0 };
static const FighterSpec tabby = { 65, 16, 5, 12, 100, 120, 30, 30, 0 };

static void tick(BattleState *battle, unsigned int frames)
{
    BattleEvent events[4];

    while (frames-- > 0) {
        (void)battleTick(battle, events, sizeof(events) / sizeof(events[0]));
    }
}

static void test_rng_is_seeded_and_deterministic(void)
{
    BattleRng first;
    BattleRng second;

    battleRngSeed(&first, 1u);
    battleRngSeed(&second, 1u);
    assert(battleRngNext(&first) == 270369u);
    assert(battleRngNext(&first) == 67634689u);
    assert(battleRngNext(&second) == 270369u);
    assert(battleRngNext(&second) == 67634689u);
}

static void test_yowl_gains_rage_every_thirty_frames(void)
{
    BattleState battle;

    battleInit(&battle, &orange, &tabby, 1u);
    assert(battleSubmit(&battle, SIDE_PLAYER, CMD_YOWL));
    assert(battle.fighter[SIDE_PLAYER].cooldown == 120u);
    tick(&battle, 30u);
    assert(battle.fighter[SIDE_PLAYER].rage == 5);
    tick(&battle, 30u);
    assert(battle.fighter[SIDE_PLAYER].rage == 10);
}

static void test_heal_ticks_at_sixty_frames_and_caps_at_maximum_hp(void)
{
    BattleState battle;

    battleInit(&battle, &orange, &tabby, 1u);
    battle.fighter[SIDE_PLAYER].hp = 25;
    assert(battleSubmit(&battle, SIDE_PLAYER, CMD_HEAL));
    tick(&battle, 60u);
    assert(battle.fighter[SIDE_PLAYER].hp == 40);
    tick(&battle, 60u);
    assert(battle.fighter[SIDE_PLAYER].hp == 55);
    tick(&battle, 60u);
    assert(battle.fighter[SIDE_PLAYER].hp == 60);
}

static void test_scratch_is_the_only_direct_damage_action(void)
{
    BattleState battle;

    battleInit(&battle, &orange, &tabby, 1u);
    assert(battleSubmit(&battle, SIDE_PLAYER, CMD_YOWL));
    tick(&battle, 60u);
    assert(battle.fighter[SIDE_PLAYER].rage == 10);
    battle.fighter[SIDE_PLAYER].cooldown = 0u;
    assert(battleSubmit(&battle, SIDE_PLAYER, CMD_SCRATCH));
    assert(battle.fighter[SIDE_AI].hp == 65 - (15 + 2));
    assert(battle.fighter[SIDE_PLAYER].channel == CHANNEL_NONE);
    assert(battle.fighter[SIDE_PLAYER].cooldown == 120u);

    battleInit(&battle, &orange, &tabby, 1u);
    assert(battleSubmit(&battle, SIDE_PLAYER, CMD_HISS));
    assert(battle.fighter[SIDE_PLAYER].cooldown == 0u);
    assert(battle.fighter[SIDE_AI].hp == 65);
    assert(battleSubmit(&battle, SIDE_PLAYER, CMD_YOWL));
    assert(battle.fighter[SIDE_AI].hp == 65);
}

static void test_scratch_interrupts_the_target_channel_without_clearing_rage(void)
{
    BattleState battle;

    battleInit(&battle, &orange, &tabby, 1u);
    battle.fighter[SIDE_AI].rage = 20;
    battle.fighter[SIDE_AI].channel = CHANNEL_HEAL;
    battle.fighter[SIDE_AI].channel_frames = 59u;
    assert(battleSubmit(&battle, SIDE_PLAYER, CMD_SCRATCH));
    assert(battle.fighter[SIDE_AI].channel == CHANNEL_NONE);
    assert(battle.fighter[SIDE_AI].channel_frames == 0u);
    assert(battle.fighter[SIDE_AI].rage == 20);
}

static void test_commands_are_rejected_during_cooldown_or_stun(void)
{
    BattleState battle;

    battleInit(&battle, &orange, &tabby, 1u);
    assert(battleSubmit(&battle, SIDE_PLAYER, CMD_YOWL));
    assert(!battleSubmit(&battle, SIDE_PLAYER, CMD_SCRATCH));
    battle.fighter[SIDE_PLAYER].stun = 3u;
    tick(&battle, 3u);
    assert(battle.fighter[SIDE_PLAYER].cooldown == 117u);
    assert(battle.fighter[SIDE_PLAYER].stun == 0u);
    assert(!battleSubmit(&battle, SIDE_PLAYER, CMD_HEAL));
    tick(&battle, 117u);
    assert(battleSubmit(&battle, SIDE_PLAYER, CMD_HEAL));
}

static void test_invalid_commands_do_not_change_the_fighter(void)
{
    BattleState battle;

    battleInit(&battle, &orange, &tabby, 1u);
    battle.fighter[SIDE_PLAYER].channel = CHANNEL_YOWL;
    battle.fighter[SIDE_PLAYER].channel_frames = 11u;
    assert(!battleSubmit(&battle, SIDE_PLAYER, (BattleCommand)99));
    assert(battle.fighter[SIDE_PLAYER].channel == CHANNEL_YOWL);
    assert(battle.fighter[SIDE_PLAYER].channel_frames == 11u);
    assert(battle.fighter[SIDE_PLAYER].cooldown == 0u);
}

static void test_pause_freezes_all_timers_and_channels(void)
{
    BattleState battle;

    battleInit(&battle, &orange, &tabby, 1u);
    assert(battleSubmit(&battle, SIDE_PLAYER, CMD_YOWL));
    battle.fighter[SIDE_PLAYER].stun = 4u;
    battle.paused = true;
    tick(&battle, 60u);
    assert(battle.fighter[SIDE_PLAYER].cooldown == 120u);
    assert(battle.fighter[SIDE_PLAYER].stun == 4u);
    assert(battle.fighter[SIDE_PLAYER].channel_frames == 0u);
    assert(battle.fighter[SIDE_PLAYER].rage == 0);
}

static void test_death_emits_ordered_events_and_stops_frame_resolution(void)
{
    BattleState battle;
    BattleEvent events[4];
    size_t count;

    battleInit(&battle, &orange, &tabby, 1u);
    battle.fighter[SIDE_AI].hp = 16;
    battle.fighter[SIDE_PLAYER].rage = 10;
    battle.fighter[SIDE_PLAYER].channel = CHANNEL_YOWL;
    battle.fighter[SIDE_PLAYER].channel_frames = 29u;
    battle.fighter[SIDE_AI].channel = CHANNEL_HEAL;
    battle.fighter[SIDE_AI].channel_frames = 59u;
    assert(battleSubmit(&battle, SIDE_PLAYER, CMD_SCRATCH));
    count = battleTick(&battle, events, sizeof(events) / sizeof(events[0]));
    assert(count == 2u);
    assert(events[0].type == EVENT_DAMAGE && events[0].amount == 17);
    assert(events[1].type == EVENT_BATTLE_END);
    assert(battle.finished);
    assert(battle.fighter[SIDE_PLAYER].rage == 10);
    assert(battle.fighter[SIDE_AI].hp == 0);
}

int main(void)
{
    test_rng_is_seeded_and_deterministic();
    test_yowl_gains_rage_every_thirty_frames();
    test_heal_ticks_at_sixty_frames_and_caps_at_maximum_hp();
    test_scratch_is_the_only_direct_damage_action();
    test_scratch_interrupts_the_target_channel_without_clearing_rage();
    test_commands_are_rejected_during_cooldown_or_stun();
    test_invalid_commands_do_not_change_the_fighter();
    test_pause_freezes_all_timers_and_channels();
    test_death_emits_ordered_events_and_stops_frame_resolution();
    return 0;
}
