#ifndef BATTLE_SCENE_H
#define BATTLE_SCENE_H

#include <stdbool.h>
#include <stdint.h>

#include "battle.h"
#include "game_config.h"

typedef struct BattleSetup {
    FighterSpec player;
    FighterSpec enemy;
    CatId player_cat;
    CatId enemy_cat;
    uint8_t crisis;
    uint32_t seed;
    bool debug_ai;
} BattleSetup;

typedef enum BattleResult {
    BATTLE_PLAYER_WIN,
    BATTLE_PLAYER_DEAD,
    BATTLE_ABORTED
} BattleResult;

BattleCommand touchCommandAt(int x, int y);
BattleResult battleSceneRun(const BattleSetup *setup);

#endif
