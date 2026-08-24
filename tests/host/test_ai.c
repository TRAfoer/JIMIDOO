#include "ai.h"
#include "battle.h"

#include <assert.h>
#include <stdint.h>

static const FighterSpec player = { 60, 15, 5, 15, 100, 120, 0, 0, 0 };
static const FighterSpec ai = { 65, 16, 5, 12, 100, 120, 0, 0, 0 };

typedef struct SeededRandom {
    BattleRng rng;
} SeededRandom;

static uint16_t seeded_random(void *context, uint16_t upper_exclusive)
{
    SeededRandom *random = context;

    if (upper_exclusive == 0u) {
        return 0u;
    }
    return (uint16_t)(battleRngNext(&random->rng) % upper_exclusive);
}

static uint16_t out_of_range_random(void *context, uint16_t upper_exclusive)
{
    (void)context;
    return (uint16_t)(upper_exclusive + 9u);
}

static int max_allowed_score(AiScores scores)
{
    int maximum = AI_SCORE_FORBIDDEN;
    BattleCommand command;

    for (command = CMD_HISS; command <= CMD_HEAL; ++command) {
        if (scores.score[command] > maximum) {
            maximum = scores.score[command];
        }
    }
    return maximum;
}

static void test_scores_forbid_full_hp_heal_and_prioritize_interrupts(void)
{
    BattleState battle;
    AiScores scores;

    battleInit(&battle, &player, &ai, 1u);
    scores = aiScoreActions(&battle, SIDE_AI);
    assert(scores.score[CMD_HEAL] == AI_SCORE_FORBIDDEN);

    battle.fighter[SIDE_PLAYER].channel = CHANNEL_YOWL;
    scores = aiScoreActions(&battle, SIDE_AI);
    assert(scores.score[CMD_HISS] > scores.score[CMD_YOWL]);
}

static void test_lethal_scratch_has_the_maximum_allowed_score(void)
{
    BattleState battle;
    AiScores scores;

    battleInit(&battle, &player, &ai, 1u);
    battle.fighter[SIDE_PLAYER].hp = 5;
    scores = aiScoreActions(&battle, SIDE_AI);
    assert(scores.score[CMD_SCRATCH] == max_allowed_score(scores));
}

static void test_unavailable_or_invalid_fighters_never_choose_an_action(void)
{
    BattleState battle;
    SeededRandom random;

    battleInit(&battle, &player, &ai, 1u);
    battleRngSeed(&random.rng, 4u);
    battle.fighter[SIDE_AI].cooldown = 1u;
    assert(aiChoose(&battle, SIDE_AI, 255u, seeded_random, &random) == CMD_NONE);
    battle.fighter[SIDE_AI].cooldown = 0u;
    battle.fighter[SIDE_AI].stun = 1u;
    assert(aiChoose(&battle, SIDE_AI, 255u, seeded_random, &random) == CMD_NONE);
    battle.fighter[SIDE_AI].stun = 0u;
    battle.paused = true;
    assert(aiChoose(&battle, SIDE_AI, 255u, seeded_random, &random) == CMD_NONE);
    battle.paused = false;
    battle.finished = true;
    assert(aiChoose(&battle, SIDE_AI, 255u, seeded_random, &random) == CMD_NONE);
    assert(aiChoose(&battle, (Side)SIDE_COUNT, 255u, seeded_random, &random) == CMD_NONE);
}

static void test_crisis_255_uses_a_reproducible_fifteen_percent_non_best_rate(void)
{
    BattleState battle;
    SeededRandom random;
    AiScores scores;
    unsigned int index;
    unsigned int non_best = 0u;

    battleInit(&battle, &player, &ai, 1u);
    battle.fighter[SIDE_AI].hp = 40;
    scores = aiScoreActions(&battle, SIDE_AI);
    battleRngSeed(&random.rng, 2u);
    for (index = 0u; index < 1000u; ++index) {
        BattleCommand command = aiChoose(&battle, SIDE_AI, 255u,
                                         seeded_random, &random);

        assert(command >= CMD_HISS && command <= CMD_HEAL);
        if (scores.score[command] < max_allowed_score(scores)) {
            ++non_best;
        }
    }
    assert(non_best >= 150u);
    assert(non_best <= 220u);
}

static void test_crisis_one_can_select_every_legal_action(void)
{
    BattleState battle;
    SeededRandom random;
    bool seen[CMD_HEAL + 1] = { false };
    unsigned int index;

    battleInit(&battle, &player, &ai, 1u);
    battle.fighter[SIDE_AI].hp = 40;
    battleRngSeed(&random.rng, 0x12345678u);
    for (index = 0u; index < 1000u; ++index) {
        BattleCommand command = aiChoose(&battle, SIDE_AI, 1u,
                                         seeded_random, &random);

        assert(command >= CMD_HISS && command <= CMD_HEAL);
        seen[command] = true;
    }
    assert(seen[CMD_HISS]);
    assert(seen[CMD_SCRATCH]);
    assert(seen[CMD_YOWL]);
    assert(seen[CMD_HEAL]);
}

static void test_rng_bounds_are_normalized_and_empty_legal_sets_are_safe(void)
{
    BattleState battle;
    BattleCommand command;
    AiScores scores;

    battleInit(&battle, &player, &ai, 1u);
    battle.fighter[SIDE_AI].hp = 40;
    command = aiChoose(&battle, SIDE_AI, 255u, out_of_range_random, 0);
    assert(command >= CMD_HISS && command <= CMD_HEAL);
    battle.fighter[SIDE_AI].cooldown = 1u;
    scores = aiScoreActions(&battle, SIDE_AI);
    assert(scores.score[CMD_HISS] == AI_SCORE_FORBIDDEN);
}

int main(void)
{
    test_scores_forbid_full_hp_heal_and_prioritize_interrupts();
    test_lethal_scratch_has_the_maximum_allowed_score();
    test_unavailable_or_invalid_fighters_never_choose_an_action();
    test_crisis_255_uses_a_reproducible_fifteen_percent_non_best_rate();
    test_crisis_one_can_select_every_legal_action();
    test_rng_bounds_are_normalized_and_empty_legal_sets_are_safe();
    return 0;
}
