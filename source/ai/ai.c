#include "ai.h"
#include "game_config.h"

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
    int damage = spec->attack + (fighter->rage / 10) * 2;

    return damage > 0 ? damage : 0;
}

static int aiHealScore(const FighterState *fighter, const FighterSpec *spec)
{
    int missing_hp = spec->max_hp - fighter->hp;
    int maximum_missing = (AI_SCORE_MAX - 1) / AI_WEIGHT_MISSING_HP_HEAL;

    if (missing_hp <= 0) {
        return AI_SCORE_FORBIDDEN;
    }
    if (missing_hp >= maximum_missing) {
        return AI_SCORE_MAX - 1;
    }
    return missing_hp * AI_WEIGHT_MISSING_HP_HEAL;
}

AiScores aiScoreActions(const BattleState *battle, Side side)
{
    AiScores scores = aiForbiddenScores();
    const FighterState *fighter;
    const FighterState *opponent;
    const FighterSpec *spec;
    const FighterSpec *opponent_spec;
    int hiss_score = 0;

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
        hiss_score += AI_WEIGHT_INTERRUPT_ACTIVE_CHANNEL;
    }
    if (opponent->rage > opponent_spec->rage_cap / 2) {
        hiss_score += AI_WEIGHT_THREAT_SUPPRESSION;
    }
    scores.score[CMD_HISS] = hiss_score;

    if (fighter->rage < spec->rage_cap) {
        scores.score[CMD_YOWL] = AI_WEIGHT_LOW_RAGE_YOWL;
    }
    if (opponent->hp <= aiScratchDamage(fighter, spec)) {
        scores.score[CMD_SCRATCH] = AI_WEIGHT_LETHAL_SCRATCH;
    }
    return scores;
}

static uint16_t aiRoll(BattleRandom random, void *random_context,
                       uint16_t upper_exclusive)
{
    uint16_t value;

    if (upper_exclusive == 0u || random == 0) {
        return 0u;
    }
    value = random(random_context, upper_exclusive);
    return value < upper_exclusive ? value :
           (uint16_t)(value % upper_exclusive);
}

static uint16_t aiBestPercent(uint8_t crisis)
{
    if (crisis <= 24u) {
        return 35u;
    }
    if (crisis <= 74u) {
        return 55u;
    }
    if (crisis <= 149u) {
        return 70u;
    }
    if (crisis <= 224u) {
        return 80u;
    }
    return 85u;
}

BattleCommand aiChoose(const BattleState *battle, Side side, uint8_t crisis,
                       BattleRandom random, void *random_context)
{
    AiScores scores = aiScoreActions(battle, side);
    BattleCommand best[CMD_HEAL];
    BattleCommand non_best[CMD_HEAL];
    unsigned int best_count = 0u;
    unsigned int non_best_count = 0u;
    unsigned int command;
    int best_score = AI_SCORE_FORBIDDEN;

    for (command = (unsigned int)CMD_HISS;
         command <= (unsigned int)CMD_HEAL; ++command) {
        if (scores.score[command] > best_score) {
            best_score = scores.score[command];
        }
    }
    if (best_score == AI_SCORE_FORBIDDEN) {
        return CMD_NONE;
    }

    for (command = (unsigned int)CMD_HISS;
         command <= (unsigned int)CMD_HEAL; ++command) {
        if (scores.score[command] == AI_SCORE_FORBIDDEN) {
            continue;
        }
        if (scores.score[command] == best_score) {
            best[best_count++] = (BattleCommand)command;
        } else {
            non_best[non_best_count++] = (BattleCommand)command;
        }
    }

    if (non_best_count != 0u &&
        aiRoll(random, random_context, 100u) >= aiBestPercent(crisis)) {
        return non_best[aiRoll(random, random_context,
                               (uint16_t)non_best_count)];
    }
    return best[aiRoll(random, random_context, (uint16_t)best_count)];
}
