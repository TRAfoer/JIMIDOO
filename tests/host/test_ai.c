#include "ai.h"
#include "battle.h"

#include <assert.h>

static const FighterSpec player = { 60, 15, 5, 15, 100, 120, 0, 0, 0 };
static const FighterSpec ai = { 65, 16, 5, 12, 100, 120, 0, 0, 0 };

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
    assert(scores.score[CMD_SCRATCH] == AI_SCORE_MAX);
    assert(scores.score[CMD_SCRATCH] == max_allowed_score(scores));
}

static void test_nonlethal_scratch_score_increases_with_target_damage_pressure(void)
{
    BattleState battle;
    AiScores high_hp_scores;
    AiScores low_hp_scores;
    AiScores low_rage_scores;
    AiScores high_rage_scores;

    battleInit(&battle, &player, &ai, 1u);
    battle.fighter[SIDE_PLAYER].hp = 50;
    high_hp_scores = aiScoreActions(&battle, SIDE_AI);
    battle.fighter[SIDE_PLAYER].hp = 17;
    low_hp_scores = aiScoreActions(&battle, SIDE_AI);
    assert(low_hp_scores.score[CMD_SCRATCH] >
           high_hp_scores.score[CMD_SCRATCH]);
    assert(low_hp_scores.score[CMD_SCRATCH] < AI_SCORE_MAX);

    battle.fighter[SIDE_PLAYER].hp = 60;
    battle.fighter[SIDE_AI].rage = 0;
    low_rage_scores = aiScoreActions(&battle, SIDE_AI);
    battle.fighter[SIDE_AI].rage = 30;
    high_rage_scores = aiScoreActions(&battle, SIDE_AI);
    assert(high_rage_scores.score[CMD_SCRATCH] >
           low_rage_scores.score[CMD_SCRATCH]);
    assert(high_rage_scores.score[CMD_SCRATCH] < AI_SCORE_MAX);
}

static void test_crisis_exposes_the_action_probability_cap(void)
{
    assert(aiActionProbabilityCap(1u) == 35u);
    assert(aiActionProbabilityCap(24u) == 35u);
    assert(aiActionProbabilityCap(25u) == 55u);
    assert(aiActionProbabilityCap(75u) == 70u);
    assert(aiActionProbabilityCap(150u) == 80u);
    assert(aiActionProbabilityCap(225u) == 85u);
    assert(aiActionProbabilityCap(255u) == 85u);
}

int main(void)
{
    test_scores_forbid_full_hp_heal_and_prioritize_interrupts();
    test_lethal_scratch_has_the_maximum_allowed_score();
    test_nonlethal_scratch_score_increases_with_target_damage_pressure();
    test_crisis_exposes_the_action_probability_cap();
    return 0;
}
