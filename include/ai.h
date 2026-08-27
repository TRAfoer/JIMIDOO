#ifndef AI_H
#define AI_H

#include "battle.h"

#include <stdint.h>

enum {
    AI_SCORE_FORBIDDEN = -1,
    AI_SCORE_MAX = 10000
};

typedef struct AiScores {
    int score[CMD_HEAL + 1];
} AiScores;

typedef enum AiProfile {
    AI_PROFILE_AGGRESSIVE,
    AI_PROFILE_COUNTER,
    AI_PROFILE_RAGE,
    AI_PROFILE_SURVIVAL,
    AI_PROFILE_OPPORTUNIST,
    AI_PROFILE_TRICKSTER,
    AI_PROFILE_COUNT
} AiProfile;

enum {
    AI_MEMORY_CAPACITY = 4,
    AI_TICKET_TOTAL = 10000
};

typedef struct AiMemory {
    BattleCommand player[AI_MEMORY_CAPACITY];
    uint8_t player_count;
    BattleCommand last_ai_action;
    uint8_t ai_repeat_count;
} AiMemory;

typedef struct AiBrain {
    BattleRng rng;
    AiProfile profile;
    AiMemory memory;
    uint16_t last_ticket[CMD_HEAL + 1];
} AiBrain;

AiScores aiScoreActions(const BattleState *battle, Side side);
uint16_t aiBestActionPercent(uint8_t crisis);
uint16_t aiActionProbabilityCap(uint8_t crisis);
BattleCommand aiChoose(const BattleState *battle, Side side, uint8_t crisis,
                       BattleRandom random, void *random_context);
void aiBrainInit(AiBrain *brain, uint32_t battle_seed);
void aiBrainRecordAccepted(AiBrain *brain, Side side, BattleCommand command);
BattleCommand aiBrainChooseNow(AiBrain *brain, const BattleState *battle,
                               Side side, uint8_t crisis);

#endif
