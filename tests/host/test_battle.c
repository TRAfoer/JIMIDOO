#include "battle.h"
#include "battle_rng.h"

#include <assert.h>
#include <stddef.h>

static const FighterSpec orange = { 60, 15, 5, 15, 100, 120, 30, 30, 0 };
static const FighterSpec tabby = { 65, 16, 5, 12, 100, 120, 30, 30, 0 };

typedef struct ForcedRoll {
    uint16_t value;
} ForcedRoll;

static uint16_t forcedRandom(void *context, uint16_t upper_exclusive)
{
    ForcedRoll *roll = context;

    assert(upper_exclusive == 100u);
    assert(roll->value < upper_exclusive);
    return roll->value;
}

static void force_roll(BattleState *battle, ForcedRoll *roll, uint16_t value)
{
    roll->value = value;
    battle->random = forcedRandom;
    battle->random_context = roll;
}

static void tick(BattleState *battle, unsigned int frames)
{
    BattleEvent events[4];

    while (frames-- > 0) {
        (void)battleTick(battle, events, sizeof(events) / sizeof(events[0]));
    }
}

static bool hiss_channel_succeeds(BattleState *battle)
{
    return battleSubmit(battle, SIDE_PLAYER, CMD_HISS) &&
           battle->fighter[SIDE_AI].stun == 120u;
}

static void test_counter_percent_is_clamped_by_rage(void)
{
    static const struct {
        int rage;
        int want;
    } cases[] = {
        { 0, 40 },
        { 10, 10 },
        { 14, 0 }
    };
    size_t index;

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        assert(counterPercent(cases[index].rage) == cases[index].want);
    }
}

static void test_hiss_channel_probability_boundaries(void)
{
    BattleState battle;
    ForcedRoll roll;

    battleInit(&battle, &orange, &tabby, 1u);
    battle.fighter[SIDE_AI].channel = CHANNEL_YOWL;
    force_roll(&battle, &roll, 79u);
    assert(hiss_channel_succeeds(&battle));

    battleInit(&battle, &orange, &tabby, 1u);
    battle.fighter[SIDE_AI].channel = CHANNEL_YOWL;
    force_roll(&battle, &roll, 80u);
    assert(!hiss_channel_succeeds(&battle));
}

static void test_ai_warning_lasts_exactly_forty_two_frames(void)
{
    BattleState battle;
    ForcedRoll roll;
    unsigned int frame;

    battleInit(&battle, &orange, &tabby, 1u);
    force_roll(&battle, &roll, 0u);
    assert(battleSubmit(&battle, SIDE_AI, CMD_SCRATCH));
    battle.spec[SIDE_PLAYER].dodge_percent = 0;
    assert(battle.pending_scratch_frames == 42u);
    assert(battle.pending_scratch_source == SIDE_AI);
    for (frame = 0u; frame < 41u; ++frame) {
        assert(battleTick(&battle, 0, 0u) == 0u);
        assert(battle.pending_scratch_frames == 41u - frame);
        assert(battle.fighter[SIDE_PLAYER].hp == 60);
    }
    assert(battleTick(&battle, 0, 0u) == 0u);
    assert(battle.pending_scratch_frames == 0u);
    assert(battle.fighter[SIDE_PLAYER].hp == 60 - 16);
}

static void test_hiss_cancels_warning_and_stuns_without_cooldown(void)
{
    BattleState battle;
    ForcedRoll roll;

    battleInit(&battle, &orange, &tabby, 1u);
    force_roll(&battle, &roll, 0u);
    assert(battleSubmit(&battle, SIDE_AI, CMD_SCRATCH));
    battle.fighter[SIDE_AI].rage = 20;
    battle.fighter[SIDE_PLAYER].channel = CHANNEL_HEAL;
    assert(battleSubmit(&battle, SIDE_PLAYER, CMD_HISS));
    assert(battle.pending_scratch_frames == 0u);
    assert(battle.fighter[SIDE_AI].rage == 0);
    assert(battle.fighter[SIDE_AI].stun == 120u);
    assert(battle.fighter[SIDE_PLAYER].cooldown == 0u);
    assert(battle.fighter[SIDE_PLAYER].channel == CHANNEL_NONE);
}

static void test_irrelevant_hiss_stops_channel_and_starts_cooldown(void)
{
    BattleState battle;

    battleInit(&battle, &orange, &tabby, 1u);
    battle.fighter[SIDE_PLAYER].channel = CHANNEL_YOWL;
    assert(battleSubmit(&battle, SIDE_PLAYER, CMD_HISS));
    assert(battle.fighter[SIDE_PLAYER].channel == CHANNEL_NONE);
    assert(battle.fighter[SIDE_PLAYER].cooldown == 120u);
}

static void test_dodge_preserves_target_channel(void)
{
    BattleState battle;
    ForcedRoll roll;

    battleInit(&battle, &orange, &tabby, 1u);
    battle.spec[SIDE_AI].dodge_percent = 100;
    battle.fighter[SIDE_PLAYER].rage = 14;
    battle.fighter[SIDE_AI].channel = CHANNEL_HEAL;
    force_roll(&battle, &roll, 0u);
    assert(battleSubmit(&battle, SIDE_PLAYER, CMD_SCRATCH));
    assert(battle.fighter[SIDE_AI].hp == 65);
    assert(battle.fighter[SIDE_AI].channel == CHANNEL_HEAL);
}

static void test_hit_stops_target_channel_without_clearing_rage(void)
{
    BattleState battle;

    battleInit(&battle, &orange, &tabby, 1u);
    battle.fighter[SIDE_PLAYER].rage = 14;
    battle.fighter[SIDE_AI].rage = 20;
    battle.fighter[SIDE_AI].channel = CHANNEL_HEAL;
    assert(battleSubmit(&battle, SIDE_PLAYER, CMD_SCRATCH));
    assert(battle.fighter[SIDE_AI].channel == CHANNEL_NONE);
    assert(battle.fighter[SIDE_AI].rage == 20);
}

static void test_immediate_counter_cancels_player_scratch_damage(void)
{
    BattleState battle;
    ForcedRoll roll;

    battleInit(&battle, &orange, &tabby, 1u);
    force_roll(&battle, &roll, 39u);
    assert(battleSubmit(&battle, SIDE_PLAYER, CMD_SCRATCH));
    assert(battle.fighter[SIDE_AI].hp == 65);
    assert(battle.fighter[SIDE_PLAYER].stun == 120u);
}

static void test_unavailable_defender_cannot_automatic_counter(void)
{
    BattleState battle;
    ForcedRoll roll;

    battleInit(&battle, &orange, &tabby, 1u);
    battle.fighter[SIDE_AI].cooldown = 1u;
    battle.spec[SIDE_AI].dodge_percent = 0;
    force_roll(&battle, &roll, 0u);
    assert(battleSubmit(&battle, SIDE_PLAYER, CMD_SCRATCH));
    assert(battle.fighter[SIDE_AI].hp == 65 - 15);
    assert(battle.fighter[SIDE_PLAYER].stun == 0u);
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
    assert(battle.fighter[SIDE_PLAYER].cooldown == 120u);
    assert(battle.fighter[SIDE_AI].hp == 65);
    tick(&battle, 120u);
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
    ForcedRoll roll;

    battleInit(&battle, &orange, &tabby, 1u);
    assert(battleSubmit(&battle, SIDE_PLAYER, CMD_YOWL));
    battle.fighter[SIDE_PLAYER].stun = 4u;
    battle.paused = true;
    tick(&battle, 60u);
    assert(battle.fighter[SIDE_PLAYER].cooldown == 120u);
    assert(battle.fighter[SIDE_PLAYER].stun == 4u);
    assert(battle.fighter[SIDE_PLAYER].channel_frames == 0u);
    assert(battle.fighter[SIDE_PLAYER].rage == 0);

    battleInit(&battle, &orange, &tabby, 1u);
    force_roll(&battle, &roll, 0u);
    assert(battleSubmit(&battle, SIDE_AI, CMD_SCRATCH));
    battle.paused = true;
    tick(&battle, 60u);
    assert(battle.pending_scratch_frames == 42u);
    assert(battle.fighter[SIDE_AI].cooldown == 120u);
}

static void test_death_emits_ordered_events_and_stops_frame_resolution(void)
{
    BattleState battle;
    BattleEvent events[5];
    ForcedRoll roll;
    size_t count;

    battleInit(&battle, &orange, &tabby, 1u);
    battle.fighter[SIDE_AI].hp = 16;
    battle.fighter[SIDE_PLAYER].rage = 14;
    battle.fighter[SIDE_PLAYER].channel = CHANNEL_YOWL;
    battle.fighter[SIDE_PLAYER].channel_frames = 29u;
    battle.fighter[SIDE_AI].channel = CHANNEL_HEAL;
    battle.fighter[SIDE_AI].channel_frames = 59u;
    battle.spec[SIDE_AI].dodge_percent = 0;
    force_roll(&battle, &roll, 0u);
    assert(battleSubmit(&battle, SIDE_PLAYER, CMD_SCRATCH));
    count = battleTick(&battle, events, sizeof(events) / sizeof(events[0]));
    assert(count == 5u);
    assert(events[0].type == EVENT_CHANNEL_STOP &&
           events[0].source == SIDE_PLAYER);
    assert(events[1].type == EVENT_HIT && events[1].amount == 17);
    assert(events[2].type == EVENT_CHANNEL_STOP &&
           events[2].target == SIDE_AI);
    assert(events[3].type == EVENT_DAMAGE && events[3].amount == 17);
    assert(events[4].type == EVENT_BATTLE_END);
    assert(battle.finished);
    assert(battle.fighter[SIDE_PLAYER].rage == 14);
    assert(battle.fighter[SIDE_AI].hp == 0);
}

static void test_tiny_buffer_eventually_delivers_lethal_scratch_events(void)
{
    BattleState battle;
    BattleEvent event;
    ForcedRoll roll;

    battleInit(&battle, &orange, &tabby, 1u);
    battle.fighter[SIDE_AI].hp = 16;
    battle.fighter[SIDE_PLAYER].rage = 14;
    battle.spec[SIDE_AI].dodge_percent = 0;
    force_roll(&battle, &roll, 0u);
    assert(battleSubmit(&battle, SIDE_PLAYER, CMD_SCRATCH));

    assert(battleTick(&battle, &event, 1u) == 1u);
    assert(event.type == EVENT_HIT && event.source == SIDE_PLAYER);
    assert(battleTick(&battle, &event, 1u) == 1u);
    assert(event.type == EVENT_DAMAGE && event.source == SIDE_PLAYER);
    assert(battleTick(&battle, &event, 1u) == 1u);
    assert(event.type == EVENT_BATTLE_END && event.source == SIDE_PLAYER);
    assert(battleTick(&battle, &event, 1u) == 0u);
}

static void test_tiny_buffer_delivers_simultaneous_channel_events_without_losing_frames(void)
{
    BattleState battle;
    BattleEvent event;

    battleInit(&battle, &orange, &tabby, 1u);
    assert(battleSubmit(&battle, SIDE_PLAYER, CMD_YOWL));
    assert(battleSubmit(&battle, SIDE_AI, CMD_YOWL));
    tick(&battle, 29u);

    assert(battleTick(&battle, &event, 1u) == 1u);
    assert(event.type == EVENT_RAGE && event.source == SIDE_PLAYER);
    assert(battle.fighter[SIDE_PLAYER].rage == 5);
    assert(battle.fighter[SIDE_AI].rage == 5);
    assert(battle.fighter[SIDE_PLAYER].channel_frames == 30u);
    assert(battle.fighter[SIDE_AI].channel_frames == 30u);

    assert(battleTick(&battle, &event, 1u) == 1u);
    assert(event.type == EVENT_RAGE && event.source == SIDE_AI);
    assert(battle.fighter[SIDE_PLAYER].channel_frames == 31u);
    assert(battle.fighter[SIDE_AI].channel_frames == 31u);
    assert(battleTick(&battle, &event, 1u) == 0u);
    assert(battle.fighter[SIDE_PLAYER].channel_frames == 32u);
    assert(battle.fighter[SIDE_AI].channel_frames == 32u);
}

static void test_no_output_ticks_continue_timing_and_discard_events(void)
{
    BattleState battle;
    unsigned int frame;

    battleInit(&battle, &orange, &tabby, 1u);
    assert(battleSubmit(&battle, SIDE_PLAYER, CMD_YOWL));
    battle.fighter[SIDE_PLAYER].stun = 125u;
    for (frame = 0u; frame < 120u; ++frame) {
        assert(battleTick(&battle, 0, 0u) == 0u);
    }
    assert(battle.fighter[SIDE_PLAYER].cooldown == 0u);
    assert(battle.fighter[SIDE_PLAYER].stun == 5u);
    assert(battle.fighter[SIDE_PLAYER].channel_frames == 120u);
    assert(battle.fighter[SIDE_PLAYER].rage == 20);
    assert(battle.pending_event_count == 0u);

    for (frame = 0u; frame < 60u; ++frame) {
        assert(battleTick(&battle, 0, 0u) == 0u);
    }
    assert(battle.fighter[SIDE_PLAYER].stun == 0u);
    assert(battle.fighter[SIDE_PLAYER].channel_frames == 180u);
    assert(battle.fighter[SIDE_PLAYER].rage == 30);
}

int main(void)
{
    test_counter_percent_is_clamped_by_rage();
    test_hiss_channel_probability_boundaries();
    test_ai_warning_lasts_exactly_forty_two_frames();
    test_hiss_cancels_warning_and_stuns_without_cooldown();
    test_irrelevant_hiss_stops_channel_and_starts_cooldown();
    test_dodge_preserves_target_channel();
    test_hit_stops_target_channel_without_clearing_rage();
    test_immediate_counter_cancels_player_scratch_damage();
    test_unavailable_defender_cannot_automatic_counter();
    test_rng_is_seeded_and_deterministic();
    test_yowl_gains_rage_every_thirty_frames();
    test_heal_ticks_at_sixty_frames_and_caps_at_maximum_hp();
    test_scratch_is_the_only_direct_damage_action();
    test_scratch_interrupts_the_target_channel_without_clearing_rage();
    test_commands_are_rejected_during_cooldown_or_stun();
    test_invalid_commands_do_not_change_the_fighter();
    test_pause_freezes_all_timers_and_channels();
    test_death_emits_ordered_events_and_stops_frame_resolution();
    test_tiny_buffer_eventually_delivers_lethal_scratch_events();
    test_tiny_buffer_delivers_simultaneous_channel_events_without_losing_frames();
    test_no_output_ticks_continue_timing_and_discard_events();
    return 0;
}
