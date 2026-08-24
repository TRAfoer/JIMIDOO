#include "battle_rng.h"

void battleRngSeed(BattleRng *rng, uint32_t seed)
{
    if (rng != 0) {
        rng->state = seed == 0u ? 0x6D2B79F5u : seed;
    }
}

uint32_t battleRngNext(BattleRng *rng)
{
    uint32_t value;

    if (rng == 0) {
        return 0u;
    }

    value = rng->state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    rng->state = value;
    return value;
}
