#include "ai.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static const FighterSpec player = { 60, 15, 5, 15, 100, 120, 0, 0, 0 };
static const FighterSpec ai = { 65, 16, 5, 12, 100, 120, 0, 0, 0 };

static BattleState battle_fixture(void)
{
    BattleState battle;

    battleInit(&battle, &player, &ai, UINT32_C(0x13579BDF));
    return battle;
}

static uint32_t opening_seed(AiProfile profile, bool wants_yowl)
{
    BattleState battle = battle_fixture();
    uint32_t seed;

    for (seed = 1u; seed <= 10000u; ++seed) {
        AiBrain brain;
        BattleCommand command;

        aiBrainInit(&brain, seed);
        brain.profile = profile;
        command = aiBrainTick(&brain, &battle, SIDE_AI, 1u);
        if ((command == CMD_YOWL) == wants_yowl) {
            return seed;
        }
    }
    assert(false);
    return 0u;
}

static uint32_t observation_seed(AiProfile profile, bool wants_zero)
{
    BattleState battle = battle_fixture();
    uint32_t seed;

    for (seed = 1u; seed <= 10000u; ++seed) {
        AiBrain brain;
        AiDebugSnapshot snapshot;

        aiBrainInit(&brain, seed);
        brain.profile = profile;
        brain.opening_choice_made = true;
        brain.opening_frames_remaining = 0u;
        assert(aiBrainTick(&brain, &battle, SIDE_AI, 1u) >= CMD_NONE);
        aiBrainSnapshot(&brain, &snapshot);
        if ((snapshot.observe_frames_remaining == 0u) == wants_zero) {
            return seed;
        }
    }
    assert(false);
    return 0u;
}

static AiBrain ready_brain(uint32_t seed, AiProfile profile)
{
    AiBrain brain;

    aiBrainInit(&brain, seed);
    brain.profile = profile;
    brain.opening_choice_made = true;
    brain.opening_frames_remaining = 0u;
    return brain;
}

static void test_opening_waits_for_exactly_120_unpaused_frames(void)
{
    BattleState battle = battle_fixture();
    AiDebugSnapshot snapshot;
    AiBrain brain;
    uint32_t seed = opening_seed(AI_PROFILE_AGGRESSIVE, false);
    unsigned int frame;

    aiBrainInit(&brain, seed);
    brain.profile = AI_PROFILE_AGGRESSIVE;
    for (frame = 0u; frame < 119u; ++frame) {
        assert(aiBrainTick(&brain, &battle, SIDE_AI, 1u) == CMD_NONE);
    }
    aiBrainSnapshot(&brain, &snapshot);
    assert(snapshot.opening_frames_remaining == 1u);
    assert(aiBrainTick(&brain, &battle, SIDE_AI, 1u) == CMD_NONE);
    aiBrainSnapshot(&brain, &snapshot);
    assert(snapshot.opening_frames_remaining == 0u);
}

static void test_opening_yowl_or_player_action_ends_patience(void)
{
    BattleState battle = battle_fixture();
    AiDebugSnapshot snapshot;
    AiBrain brain;
    uint32_t yowl_seed = opening_seed(AI_PROFILE_RAGE, true);
    uint32_t waiting_seed = opening_seed(AI_PROFILE_AGGRESSIVE, false);

    aiBrainInit(&brain, yowl_seed);
    brain.profile = AI_PROFILE_RAGE;
    assert(aiBrainTick(&brain, &battle, SIDE_AI, 1u) == CMD_YOWL);
    aiBrainSnapshot(&brain, &snapshot);
    assert(snapshot.opening_frames_remaining == 0u);

    aiBrainInit(&brain, waiting_seed);
    brain.profile = AI_PROFILE_AGGRESSIVE;
    assert(aiBrainTick(&brain, &battle, SIDE_AI, 1u) == CMD_NONE);
    aiBrainRecordAccepted(&brain, SIDE_PLAYER, CMD_HISS);
    aiBrainSnapshot(&brain, &snapshot);
    assert(snapshot.opening_frames_remaining == 0u);
}

static void test_opening_waiting_advances_during_an_unpaused_cooldown(void)
{
    BattleState battle = battle_fixture();
    AiDebugSnapshot snapshot;
    AiBrain brain;
    uint32_t seed = opening_seed(AI_PROFILE_AGGRESSIVE, false);

    aiBrainInit(&brain, seed);
    brain.profile = AI_PROFILE_AGGRESSIVE;
    assert(aiBrainTick(&brain, &battle, SIDE_AI, 1u) == CMD_NONE);
    battle.fighter[SIDE_AI].cooldown = 1u;
    assert(aiBrainTick(&brain, &battle, SIDE_AI, 1u) == CMD_NONE);
    aiBrainSnapshot(&brain, &snapshot);
    assert(snapshot.opening_frames_remaining == 118u);
}

static void test_each_profile_arms_only_its_inclusive_observation_range(void)
{
    static const uint16_t minimum[AI_PROFILE_COUNT] = { 0u, 6u, 6u,
                                                         12u, 0u, 0u };
    static const uint16_t maximum[AI_PROFILE_COUNT] = { 18u, 30u, 24u,
                                                         36u, 24u, 36u };
    BattleState battle = battle_fixture();
    unsigned int profile;
    unsigned int sample;

    for (profile = 0u; profile < AI_PROFILE_COUNT; ++profile) {
        for (sample = 1u; sample <= 1000u; ++sample) {
            AiDebugSnapshot snapshot;
            AiBrain brain = ready_brain(sample, (AiProfile)profile);

            assert(aiBrainTick(&brain, &battle, SIDE_AI, 1u) >= CMD_NONE);
            aiBrainSnapshot(&brain, &snapshot);
            assert(snapshot.observe_frames_remaining >= minimum[profile]);
            assert(snapshot.observe_frames_remaining <= maximum[profile]);
            assert(snapshot.observe_frames_remaining <= AI_MAX_OBSERVE_FRAMES);
        }
    }
}

static void test_pause_freezes_opening_and_observation_counters(void)
{
    BattleState battle = battle_fixture();
    AiDebugSnapshot before;
    AiDebugSnapshot after;
    AiBrain brain;

    aiBrainInit(&brain, 1u);
    battle.paused = true;
    aiBrainSnapshot(&brain, &before);
    assert(before.opening_frames_remaining == AI_OPENING_PATIENCE_FRAMES);
    assert(aiBrainTick(&brain, &battle, SIDE_AI, 1u) == CMD_NONE);
    aiBrainSnapshot(&brain, &after);
    assert(memcmp(&before.opening_frames_remaining,
                  &after.opening_frames_remaining,
                  sizeof(before.opening_frames_remaining)) == 0);

    battle = battle_fixture();
    brain = ready_brain(1u, AI_PROFILE_COUNTER);
    assert(aiBrainTick(&brain, &battle, SIDE_AI, 1u) == CMD_NONE);
    aiBrainSnapshot(&brain, &before);
    assert(before.observe_frames_remaining != 0u);
    battle.paused = true;
    assert(aiBrainTick(&brain, &battle, SIDE_AI, 1u) == CMD_NONE);
    aiBrainSnapshot(&brain, &after);
    assert(memcmp(&before.observe_frames_remaining,
                  &after.observe_frames_remaining,
                  sizeof(before.observe_frames_remaining)) == 0);
}

typedef enum ObservationCancellation {
    OBSERVATION_CANCEL_COOLDOWN,
    OBSERVATION_CANCEL_STUN,
    OBSERVATION_CANCEL_DEATH,
    OBSERVATION_CANCEL_FINISHED
} ObservationCancellation;

static void assert_observation_is_cancelled(ObservationCancellation cancellation)
{
    BattleState battle = battle_fixture();
    AiDebugSnapshot snapshot;
    AiBrain brain = ready_brain(1u, AI_PROFILE_COUNTER);

    assert(aiBrainTick(&brain, &battle, SIDE_AI, 1u) == CMD_NONE);
    assert(brain.observing);
    if (cancellation == OBSERVATION_CANCEL_COOLDOWN) {
        battle.fighter[SIDE_AI].cooldown = 1u;
    } else if (cancellation == OBSERVATION_CANCEL_STUN) {
        battle.fighter[SIDE_AI].stun = 1u;
    } else if (cancellation == OBSERVATION_CANCEL_DEATH) {
        battle.fighter[SIDE_AI].hp = 0;
    } else {
        battle.finished = true;
    }
    assert(aiBrainTick(&brain, &battle, SIDE_AI, 1u) == CMD_NONE);
    aiBrainSnapshot(&brain, &snapshot);
    assert(!brain.observing);
    assert(snapshot.observe_frames_remaining == 0u);
}

static void test_ineligible_or_finished_battle_cancels_observation(void)
{
    assert_observation_is_cancelled(OBSERVATION_CANCEL_COOLDOWN);
    assert_observation_is_cancelled(OBSERVATION_CANCEL_STUN);
    assert_observation_is_cancelled(OBSERVATION_CANCEL_DEATH);
    assert_observation_is_cancelled(OBSERVATION_CANCEL_FINISHED);
}

static void test_zero_observation_can_choose_on_arming_frame(void)
{
    BattleState battle = battle_fixture();
    AiBrain brain;
    BattleCommand command;
    uint32_t seed = observation_seed(AI_PROFILE_AGGRESSIVE, true);

    brain = ready_brain(seed, AI_PROFILE_AGGRESSIVE);
    command = aiBrainTick(&brain, &battle, SIDE_AI, 1u);
    assert(command >= CMD_HISS);
    assert(command <= CMD_HEAL);
    assert(!brain.observing);
}

static void test_positive_observation_waits_for_all_sampled_frames(void)
{
    BattleState battle = battle_fixture();
    AiDebugSnapshot snapshot;
    AiBrain brain = ready_brain(1u, AI_PROFILE_COUNTER);
    BattleCommand command;
    unsigned int frame;

    assert(aiBrainTick(&brain, &battle, SIDE_AI, 1u) == CMD_NONE);
    aiBrainSnapshot(&brain, &snapshot);
    assert(snapshot.observe_frames_remaining >= 6u);
    for (frame = 0u; frame < snapshot.observe_frames_remaining; ++frame) {
        assert(aiBrainTick(&brain, &battle, SIDE_AI, 1u) == CMD_NONE);
    }
    command = aiBrainTick(&brain, &battle, SIDE_AI, 1u);
    assert(command >= CMD_HISS);
    assert(command <= CMD_HEAL);
}

static void test_reinitialization_clears_history_repetition_and_snapshot_is_read_only(void)
{
    AiDebugSnapshot snapshot;
    AiDebugSnapshot null_snapshot;
    AiBrain before;
    AiBrain brain;

    aiBrainInit(&brain, UINT32_C(0x9ABCDEF0));
    aiBrainRecordAccepted(&brain, SIDE_PLAYER, CMD_SCRATCH);
    aiBrainRecordAccepted(&brain, SIDE_AI, CMD_HISS);
    aiBrainRecordAccepted(&brain, SIDE_AI, CMD_HISS);
    before = brain;
    aiBrainSnapshot(&brain, &snapshot);
    assert(memcmp(&brain, &before, sizeof(brain)) == 0);
    assert(snapshot.player_history_count == 1u);
    assert(snapshot.player_history[0] == CMD_SCRATCH);
    assert(snapshot.ticket[CMD_HISS] == brain.last_ticket[CMD_HISS]);

    aiBrainSnapshot(0, &null_snapshot);
    assert(null_snapshot.profile == AI_PROFILE_AGGRESSIVE);
    assert(null_snapshot.player_history_count == 0u);
    assert(null_snapshot.opening_frames_remaining == 0u);
    assert(null_snapshot.observe_frames_remaining == 0u);

    aiBrainInit(&brain, UINT32_C(0x01234567));
    assert(brain.memory.player_count == 0u);
    assert(brain.memory.last_ai_action == CMD_NONE);
    assert(brain.memory.ai_repeat_count == 0u);
    assert(brain.opening_frames_remaining == AI_OPENING_PATIENCE_FRAMES);
}

int main(void)
{
    test_opening_waits_for_exactly_120_unpaused_frames();
    test_opening_yowl_or_player_action_ends_patience();
    test_opening_waiting_advances_during_an_unpaused_cooldown();
    test_each_profile_arms_only_its_inclusive_observation_range();
    test_pause_freezes_opening_and_observation_counters();
    test_ineligible_or_finished_battle_cancels_observation();
    test_zero_observation_can_choose_on_arming_frame();
    test_positive_observation_waits_for_all_sampled_frames();
    test_reinitialization_clears_history_repetition_and_snapshot_is_read_only();
    return 0;
}
