#ifndef AI_INTERNAL_H
#define AI_INTERNAL_H

#include "ai.h"

typedef struct AiWeights {
    uint64_t value[CMD_HEAL + 1];
} AiWeights;

typedef struct AiTickets {
    uint16_t value[CMD_HEAL + 1];
} AiTickets;

AiWeights aiPolicyWeights(const BattleState *battle, Side side,
                          uint8_t crisis, AiProfile profile,
                          const AiMemory *memory,
                          const int8_t noise_percent[CMD_HEAL + 1]);
AiTickets aiPolicyTickets(AiWeights weights, uint16_t cap_percent);

#endif
