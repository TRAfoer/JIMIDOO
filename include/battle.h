#ifndef BATTLE_H
#define BATTLE_H

#include "battle_rng.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    BATTLE_FPS = 60,
    BATTLE_RAGE_TICK_FRAMES = BATTLE_FPS / 2,
    BATTLE_HEAL_TICK_FRAMES = BATTLE_FPS,
    BATTLE_PENDING_EVENT_CAPACITY = 2
};

typedef enum Side {
    SIDE_PLAYER,
    SIDE_AI,
    SIDE_COUNT
} Side;

typedef enum BattleCommand {
    CMD_NONE,
    CMD_HISS,
    CMD_SCRATCH,
    CMD_YOWL,
    CMD_HEAL
} BattleCommand;

typedef enum Channel {
    CHANNEL_NONE,
    CHANNEL_YOWL,
    CHANNEL_HEAL
} Channel;

typedef enum BattleEventType {
    EVENT_NONE,
    EVENT_DAMAGE,
    EVENT_HEAL,
    EVENT_RAGE,
    EVENT_BATTLE_END
} BattleEventType;

typedef struct FighterSpec {
    int max_hp;
    int attack;
    int rage_per_tick;
    int heal_per_tick;
    int rage_cap;
    uint32_t action_cd_frames;
    int dodge_percent;
    int warning_percent;
    int counter_percent;
} FighterSpec;

typedef struct FighterState {
    int hp;
    int rage;
    uint32_t cooldown;
    uint32_t stun;
    uint32_t channel_frames;
    Channel channel;
} FighterState;

typedef struct BattleEvent {
    BattleEventType type;
    Side source;
    Side target;
    int amount;
} BattleEvent;

typedef struct BattleState {
    FighterSpec spec[SIDE_COUNT];
    FighterState fighter[SIDE_COUNT];
    BattleRng rng;
    BattleEvent pending_events[BATTLE_PENDING_EVENT_CAPACITY];
    size_t pending_event_count;
    bool paused;
    bool finished;
    Side winner;
} BattleState;

void battleInit(BattleState *battle, const FighterSpec *player,
                const FighterSpec *ai, uint32_t seed);
bool battleSubmit(BattleState *battle, Side side, BattleCommand command);
size_t battleTick(BattleState *battle, BattleEvent *events,
                  size_t event_capacity);

#endif
