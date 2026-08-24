#ifndef BATTLE_RNG_H
#define BATTLE_RNG_H

#include <stdint.h>

typedef struct BattleRng {
    uint32_t state;
} BattleRng;

void battleRngSeed(BattleRng *rng, uint32_t seed);
uint32_t battleRngNext(BattleRng *rng);

#endif
