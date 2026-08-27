#include "ai.h"
#include "game_config.h"

#include <limits.h>

static bool aiSideIsValid(Side side)
{
    return side == SIDE_PLAYER || side == SIDE_AI;
}

static Side aiOpponent(Side side)
{
    return side == SIDE_PLAYER ? SIDE_AI : SIDE_PLAYER;
}

static bool aiCanAct(const BattleState *battle, Side side)
{
    const FighterState *fighter;

    if (battle == 0 || !aiSideIsValid(side) || battle->paused ||
        battle->finished) {
        return false;
    }
    fighter = &battle->fighter[side];
    return fighter->cooldown == 0u && fighter->stun == 0u;
}

static AiScores aiForbiddenScores(void)
{
    AiScores scores;
    unsigned int command;

    for (command = 0u; command <= (unsigned int)CMD_HEAL; ++command) {
        scores.score[command] = AI_SCORE_FORBIDDEN;
    }
    return scores;
}

static int aiScratchDamage(const FighterState *fighter,
                           const FighterSpec *spec)
{
    int64_t damage = (int64_t)spec->attack +
                     ((int64_t)fighter->rage / 10) * 2;

    if (damage <= 0) {
        return 0;
    }
    return damage >= INT_MAX ? INT_MAX : (int)damage;
}

static int aiPositiveDifference(int high, int low)
{
    int64_t difference = (int64_t)high - (int64_t)low;

    if (difference <= 0) {
        return 0;
    }
    return difference >= AI_SCORE_MAX - 1 ? AI_SCORE_MAX - 1 :
           (int)difference;
}

static int aiScoreAdd(int score, int value, int weight)
{
    int remaining = AI_SCORE_MAX - 1 - score;

    if (value <= 0 || weight <= 0 || remaining <= 0) {
        return score;
    }
    if (value > remaining / weight) {
        return AI_SCORE_MAX - 1;
    }
    return score + value * weight;
}

static int aiHealScore(const FighterState *fighter, const FighterSpec *spec)
{
    int missing_hp = aiPositiveDifference(spec->max_hp, fighter->hp);

    if (missing_hp <= 0) {
        return AI_SCORE_FORBIDDEN;
    }
    return aiScoreAdd(0, missing_hp, AI_WEIGHT_MISSING_HP_HEAL);
}

AiScores aiScoreActions(const BattleState *battle, Side side)
{
    AiScores scores = aiForbiddenScores();
    const FighterState *fighter;
    const FighterState *opponent;
    const FighterSpec *spec;
    const FighterSpec *opponent_spec;
    int hiss_score = 0;
    int scratch_score = 0;

    if (!aiCanAct(battle, side)) {
        return scores;
    }

    fighter = &battle->fighter[side];
    opponent = &battle->fighter[aiOpponent(side)];
    spec = &battle->spec[side];
    opponent_spec = &battle->spec[aiOpponent(side)];

    scores.score[CMD_HISS] = 0;
    scores.score[CMD_SCRATCH] = 0;
    scores.score[CMD_YOWL] = 0;
    scores.score[CMD_HEAL] = aiHealScore(fighter, spec);

    if (opponent->channel != CHANNEL_NONE) {
        hiss_score = aiScoreAdd(hiss_score, 1,
                                AI_WEIGHT_INTERRUPT_ACTIVE_CHANNEL);
    }
    if (opponent->rage > opponent_spec->rage_cap / 2) {
        hiss_score = aiScoreAdd(hiss_score, 1,
                                AI_WEIGHT_THREAT_SUPPRESSION);
    }
    scores.score[CMD_HISS] = hiss_score;

    if (fighter->rage < spec->rage_cap) {
        scores.score[CMD_YOWL] = aiScoreAdd(0, 1,
                                             AI_WEIGHT_LOW_RAGE_YOWL);
    }
    if (opponent->hp <= aiScratchDamage(fighter, spec)) {
        scores.score[CMD_SCRATCH] = AI_WEIGHT_LETHAL_SCRATCH;
    } else {
        scratch_score = aiScoreAdd(
            scratch_score,
            aiPositiveDifference(opponent_spec->max_hp, opponent->hp),
            AI_WEIGHT_SCRATCH_TARGET_DAMAGE_PRESSURE);
        scratch_score = aiScoreAdd(scratch_score, fighter->rage,
                                   AI_WEIGHT_SCRATCH_RAGE_PRESSURE);
        scores.score[CMD_SCRATCH] = scratch_score;
    }
    return scores;
}
