#ifndef AI_H
#define AI_H

#include "battle.h"

enum {
    AI_SCORE_FORBIDDEN = -1,
    AI_SCORE_MAX = 10000
};

typedef struct AiScores {
    int score[CMD_HEAL + 1];
} AiScores;

AiScores aiScoreActions(const BattleState *battle, Side side);
BattleCommand aiChoose(const BattleState *battle, Side side, uint8_t crisis,
                       BattleRandom random, void *random_context);

#endif
