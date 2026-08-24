#ifndef BATTLE_SCENE_INTERNAL_H
#define BATTLE_SCENE_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "battle.h"
#include "game_config.h"
#include "graphics_service.h"

typedef struct BattleHudSnapshot {
    int hp[SIDE_COUNT];
    int rage[SIDE_COUNT];
    unsigned int cooldown_tenths;
    uint32_t stun;
    Channel channel;
    bool paused;
    bool available;
} BattleHudSnapshot;

typedef struct BattleHud {
    BattleHudSnapshot shown;
    bool base_drawn;
    bool status_valid;
    bool buttons_valid;
} BattleHud;

typedef enum BattleHudDirtyRegion {
    BATTLE_HUD_DIRTY_NONE = 0,
    BATTLE_HUD_DIRTY_STATUS = 1,
    BATTLE_HUD_DIRTY_BUTTONS = 2
} BattleHudDirtyRegion;

typedef struct BattleFighterAnimation {
    CatAction action;
    uint8_t frames;
    uint8_t duration;
    uint8_t flash_frames;
    uint8_t afterimage_frames;
    int8_t dodge_direction;
} BattleFighterAnimation;

typedef struct BattleAnimation {
    BattleFighterAnimation fighter[SIDE_COUNT];
    BattleEventType caption;
    Side caption_side;
    int caption_amount;
    uint8_t caption_frames;
    uint8_t warning_frames;
    uint32_t frame;
} BattleAnimation;

unsigned int battleHudCooldownTenths(uint32_t frames);
unsigned int battleHudDirtyRegions(const BattleHudSnapshot *shown,
                                   const BattleHudSnapshot *next,
                                   bool status_valid, bool buttons_valid);
void battleHudInit(BattleHud *hud);
void battleHudDraw(BattleHud *hud, const BattleState *battle);

void battleAnimationInit(BattleAnimation *animation);
void battleAnimationOnAction(BattleAnimation *animation, Side side,
                             BattleCommand command);
void battleAnimationOnEvents(BattleAnimation *animation,
                             const BattleEvent *events, size_t event_count);
void battleAnimationTick(BattleAnimation *animation, bool paused);
void battleAnimationDraw(const BattleAnimation *animation,
                         const BattleState *battle, CatId player_cat,
                         CatId enemy_cat);

#endif
