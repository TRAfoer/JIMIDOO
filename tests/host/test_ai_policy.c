#include "ai.h"
#include "ai_internal.h"

#include <assert.h>
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

static AiMemory memory_with(BattleCommand command, uint8_t count)
{
    AiMemory memory;
    uint8_t index;

    memset(&memory, 0, sizeof(memory));
    memory.player_count = count;
    for (index = 0u; index < count; ++index) {
        memory.player[index] = command;
    }
    return memory;
}

static unsigned int ticket_total(AiTickets tickets)
{
    unsigned int total = 0u;
    BattleCommand command;

    for (command = CMD_HISS; command <= CMD_HEAL; ++command) {
        total += tickets.value[command];
    }
    return total;
}

static uint16_t largest_ticket(AiTickets tickets)
{
    uint16_t largest = 0u;
    BattleCommand command;

    for (command = CMD_HISS; command <= CMD_HEAL; ++command) {
        if (tickets.value[command] > largest) {
            largest = tickets.value[command];
        }
    }
    return largest;
}

static void test_seeded_brains_and_player_history_are_deterministic(void)
{
    AiBrain first;
    AiBrain second;

    aiBrainInit(&first, UINT32_C(0x4A694D69));
    aiBrainInit(&second, UINT32_C(0x4A694D69));
    assert(first.profile == second.profile);
    assert(first.rng.state == second.rng.state);

    aiBrainRecordAccepted(&first, SIDE_PLAYER, CMD_HISS);
    aiBrainRecordAccepted(&first, SIDE_PLAYER, CMD_HEAL);
    aiBrainRecordAccepted(&first, SIDE_PLAYER, CMD_SCRATCH);
    aiBrainRecordAccepted(&first, SIDE_PLAYER, CMD_YOWL);
    aiBrainRecordAccepted(&first, SIDE_PLAYER, CMD_HEAL);
    assert(first.memory.player_count == 4u);
    assert(first.memory.player[0] == CMD_HEAL);
    assert(first.memory.player[1] == CMD_YOWL);
    assert(first.memory.player[2] == CMD_SCRATCH);
    assert(first.memory.player[3] == CMD_HEAL);
}

static void test_accepted_ai_actions_track_saturating_repetition(void)
{
    AiBrain brain;
    AiMemory before;

    aiBrainInit(&brain, UINT32_C(0x10203040));
    aiBrainRecordAccepted(&brain, SIDE_AI, CMD_HISS);
    assert(brain.memory.last_ai_action == CMD_HISS);
    assert(brain.memory.ai_repeat_count == 1u);
    aiBrainRecordAccepted(&brain, SIDE_AI, CMD_HISS);
    assert(brain.memory.ai_repeat_count == 2u);
    brain.memory.ai_repeat_count = UINT8_MAX;
    aiBrainRecordAccepted(&brain, SIDE_AI, CMD_HISS);
    assert(brain.memory.ai_repeat_count == UINT8_MAX);

    before = brain.memory;
    aiBrainRecordAccepted(&brain, SIDE_AI, CMD_NONE);
    assert(memcmp(&brain.memory, &before, sizeof(before)) == 0);
    aiBrainRecordAccepted(&brain, (Side)SIDE_COUNT, CMD_SCRATCH);
    assert(memcmp(&brain.memory, &before, sizeof(before)) == 0);
}

static void test_profile_multipliers_apply_to_base_weights(void)
{
    BattleState battle = battle_fixture();
    AiMemory memory = { { CMD_NONE }, 0u, CMD_NONE, 0u };
    const int8_t noise[CMD_HEAL + 1] = { 0 };
    AiWeights aggressive;
    AiWeights counter;
    AiWeights rage;
    AiWeights survival;
    AiWeights opportunist;
    AiWeights trickster;

    aggressive = aiPolicyWeights(&battle, SIDE_AI, 255u,
                                 AI_PROFILE_AGGRESSIVE, &memory, noise);
    counter = aiPolicyWeights(&battle, SIDE_AI, 255u, AI_PROFILE_COUNTER,
                              &memory, noise);
    rage = aiPolicyWeights(&battle, SIDE_AI, 255u, AI_PROFILE_RAGE, &memory,
                           noise);
    survival = aiPolicyWeights(&battle, SIDE_AI, 255u, AI_PROFILE_SURVIVAL,
                               &memory, noise);
    opportunist = aiPolicyWeights(&battle, SIDE_AI, 255u,
                                  AI_PROFILE_OPPORTUNIST, &memory, noise);
    trickster = aiPolicyWeights(&battle, SIDE_AI, 255u,
                                AI_PROFILE_TRICKSTER, &memory, noise);

    assert(aggressive.value[CMD_HISS] == UINT64_C(90));
    assert(aggressive.value[CMD_SCRATCH] == UINT64_C(145));
    assert(aggressive.value[CMD_YOWL] == UINT64_C(240));
    assert(counter.value[CMD_HISS] == UINT64_C(150));
    assert(counter.value[CMD_SCRATCH] == UINT64_C(100));
    assert(counter.value[CMD_YOWL] == UINT64_C(255));
    assert(rage.value[CMD_HISS] == UINT64_C(90));
    assert(rage.value[CMD_SCRATCH] == UINT64_C(110));
    assert(rage.value[CMD_YOWL] == UINT64_C(465));
    assert(survival.value[CMD_HISS] == UINT64_C(105));
    assert(survival.value[CMD_SCRATCH] == UINT64_C(80));
    assert(survival.value[CMD_YOWL] == UINT64_C(315));
    assert(opportunist.value[CMD_HISS] == UINT64_C(100));
    assert(opportunist.value[CMD_SCRATCH] == UINT64_C(100));
    assert(opportunist.value[CMD_YOWL] == UINT64_C(300));
    assert(trickster.value[CMD_HISS] == UINT64_C(100));
    assert(trickster.value[CMD_SCRATCH] == UINT64_C(100));
    assert(trickster.value[CMD_YOWL] == UINT64_C(300));
    assert(aggressive.value[CMD_HEAL] == 0u);
}

static void test_memory_responses_use_recency_and_crisis_scaling(void)
{
    BattleState battle = battle_fixture();
    AiMemory yowl_memory = memory_with(CMD_YOWL, 1u);
    AiMemory heal_memory = memory_with(CMD_HEAL, 1u);
    AiMemory hiss_memory = memory_with(CMD_HISS, 1u);
    AiMemory scratch_memory = memory_with(CMD_SCRATCH, 1u);
    const int8_t noise[CMD_HEAL + 1] = { 0 };
    AiWeights weights;

    weights = aiPolicyWeights(&battle, SIDE_AI, 255u,
                              AI_PROFILE_OPPORTUNIST, &yowl_memory, noise);
    assert(weights.value[CMD_HISS] == UINT64_C(180));
    weights = aiPolicyWeights(&battle, SIDE_AI, 24u,
                              AI_PROFILE_OPPORTUNIST, &heal_memory, noise);
    assert(weights.value[CMD_HISS] == UINT64_C(120));

    weights = aiPolicyWeights(&battle, SIDE_AI, 255u,
                              AI_PROFILE_OPPORTUNIST, &hiss_memory, noise);
    assert(weights.value[CMD_SCRATCH] == UINT64_C(148));
    assert(weights.value[CMD_YOWL] == UINT64_C(204));

    hiss_memory = memory_with(CMD_HISS, AI_MEMORY_CAPACITY);
    weights = aiPolicyWeights(&battle, SIDE_AI, 255u,
                              AI_PROFILE_OPPORTUNIST, &hiss_memory, noise);
    assert(weights.value[CMD_YOWL] == UINT64_C(75));

    battle.fighter[SIDE_AI].hp = 40;
    weights = aiPolicyWeights(&battle, SIDE_AI, 255u,
                              AI_PROFILE_OPPORTUNIST, &scratch_memory, noise);
    assert(weights.value[CMD_SCRATCH] == UINT64_C(100));
    assert(weights.value[CMD_YOWL] == UINT64_C(300));
    assert(weights.value[CMD_HEAL] == UINT64_C(840));

    battle.fighter[SIDE_AI].hp = ai.max_hp;
    weights = aiPolicyWeights(&battle, SIDE_AI, 255u,
                              AI_PROFILE_OPPORTUNIST, &scratch_memory, noise);
    assert(weights.value[CMD_YOWL] == UINT64_C(396));

    battle.fighter[SIDE_AI].rage = ai.rage_cap;
    weights = aiPolicyWeights(&battle, SIDE_AI, 255u,
                              AI_PROFILE_OPPORTUNIST, &scratch_memory, noise);
    assert(weights.value[CMD_SCRATCH] == UINT64_C(744));
}

static void test_repeat_penalties_preserve_decisive_actions(void)
{
    BattleState battle = battle_fixture();
    AiMemory memory = { { CMD_NONE }, 0u, CMD_YOWL, 1u };
    const int8_t noise[CMD_HEAL + 1] = { 0 };
    AiWeights weights;

    weights = aiPolicyWeights(&battle, SIDE_AI, 255u,
                              AI_PROFILE_OPPORTUNIST, &memory, noise);
    assert(weights.value[CMD_YOWL] == UINT64_C(210));
    weights = aiPolicyWeights(&battle, SIDE_AI, 255u,
                              AI_PROFILE_TRICKSTER, &memory, noise);
    assert(weights.value[CMD_YOWL] == UINT64_C(165));

    memory.last_ai_action = CMD_SCRATCH;
    memory.ai_repeat_count = 2u;
    battle.fighter[SIDE_PLAYER].hp = 5;
    weights = aiPolicyWeights(&battle, SIDE_AI, 255u,
                              AI_PROFILE_TRICKSTER, &memory, noise);
    assert(weights.value[CMD_SCRATCH] == UINT64_C(10100));

    memory.last_ai_action = CMD_YOWL;
    memory.ai_repeat_count = 0u;
    weights = aiPolicyWeights(&battle, SIDE_AI, 255u,
                              AI_PROFILE_OPPORTUNIST, &memory, noise);
    assert(weights.value[CMD_SCRATCH] == UINT64_C(17675));

    memory.last_ai_action = CMD_HISS;
    battle.fighter[SIDE_PLAYER].hp = player.max_hp;
    battle.fighter[SIDE_PLAYER].channel = CHANNEL_YOWL;
    weights = aiPolicyWeights(&battle, SIDE_AI, 255u,
                              AI_PROFILE_TRICKSTER, &memory, noise);
    assert(weights.value[CMD_HISS] == UINT64_C(400));
    memory.last_ai_action = CMD_YOWL;
    memory.ai_repeat_count = 0u;
    weights = aiPolicyWeights(&battle, SIDE_AI, 255u,
                              AI_PROFILE_OPPORTUNIST, &memory, noise);
    assert(weights.value[CMD_HISS] == UINT64_C(700));
}

static void test_ticket_normalization_is_capped_and_ordered(void)
{
    AiWeights weights = { { 0u, UINT64_C(1000), UINT64_C(1), UINT64_C(1),
                            0u } };
    AiWeights equal = { { 0u, UINT64_C(1), UINT64_C(1), UINT64_C(1), 0u } };
    AiTickets tickets;
    const uint8_t crises[] = { 1u, 25u, 75u, 150u, 225u };
    const uint16_t caps[] = { 3500u, 5500u, 7000u, 8000u, 8500u };
    unsigned int index;
    BattleCommand command;

    for (index = 0u; index < sizeof(crises) / sizeof(crises[0]); ++index) {
        tickets = aiPolicyTickets(weights, aiActionProbabilityCap(crises[index]));
        assert(ticket_total(tickets) == AI_TICKET_TOTAL);
        assert(tickets.value[CMD_HISS] != 0u);
        assert(tickets.value[CMD_SCRATCH] != 0u);
        assert(tickets.value[CMD_YOWL] != 0u);
        assert(tickets.value[CMD_HEAL] == 0u);
        assert(largest_ticket(tickets) <= caps[index]);
    }

    tickets = aiPolicyTickets(equal, 100u);
    assert(tickets.value[CMD_HISS] == 3334u);
    assert(tickets.value[CMD_SCRATCH] == 3333u);
    assert(tickets.value[CMD_YOWL] == 3333u);
    assert(tickets.value[CMD_HEAL] == 0u);
    for (command = CMD_HISS; command <= CMD_HEAL; ++command) {
        if (equal.value[command] == 0u) {
            assert(tickets.value[command] == 0u);
        }
    }

    weights = (AiWeights){ { 0u, UINT64_C(9), 0u, 0u, 0u } };
    tickets = aiPolicyTickets(weights, 35u);
    assert(tickets.value[CMD_HISS] == AI_TICKET_TOTAL);
}

static void test_feasible_caps_limit_every_action_and_infeasible_caps_balance(void)
{
    AiWeights adversarial = { { 0u, UINT64_C(500), UINT64_C(499),
                                UINT64_C(1), 0u } };
    AiWeights two_actions = { { 0u, UINT64_C(9), UINT64_C(1), 0u, 0u } };
    AiTickets tickets;

    tickets = aiPolicyTickets(adversarial, 35u);
    assert(ticket_total(tickets) == AI_TICKET_TOTAL);
    assert(tickets.value[CMD_HISS] == 3500u);
    assert(tickets.value[CMD_SCRATCH] == 3500u);
    assert(tickets.value[CMD_YOWL] == 3000u);
    assert(largest_ticket(tickets) == 3500u);

    tickets = aiPolicyTickets(two_actions, 35u);
    assert(ticket_total(tickets) == AI_TICKET_TOTAL);
    assert(tickets.value[CMD_HISS] == 5000u);
    assert(tickets.value[CMD_SCRATCH] == 5000u);
    assert(largest_ticket(tickets) == 5000u);
}

static void test_brain_selection_never_consumes_combat_rng(void)
{
    BattleState battle = battle_fixture();
    AiBrain brain;
    AiBrain second;
    uint32_t battle_rng_state = battle.rng.state;
    unsigned int index;

    aiBrainInit(&brain, UINT32_C(0x2468ACE0));
    aiBrainInit(&second, UINT32_C(0x2468ACE0));
    battle.fighter[SIDE_AI].hp = 40;
    for (index = 0u; index < 100u; ++index) {
        BattleCommand command = aiBrainChooseNow(&brain, &battle, SIDE_AI,
                                                 150u);
        BattleCommand matching = aiBrainChooseNow(&second, &battle, SIDE_AI,
                                                  150u);

        assert(command >= CMD_HISS && command <= CMD_HEAL);
        assert(command == matching);
    }
    assert(battle.rng.state == battle_rng_state);
}

int main(void)
{
    test_seeded_brains_and_player_history_are_deterministic();
    test_accepted_ai_actions_track_saturating_repetition();
    test_profile_multipliers_apply_to_base_weights();
    test_memory_responses_use_recency_and_crisis_scaling();
    test_repeat_penalties_preserve_decisive_actions();
    test_ticket_normalization_is_capped_and_ordered();
    test_feasible_caps_limit_every_action_and_infeasible_caps_balance();
    test_brain_selection_never_consumes_combat_rng();
    return 0;
}
