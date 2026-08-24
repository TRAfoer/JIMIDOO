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
    BATTLE_WARNING_FRAMES = 42,
    BATTLE_STUN_FRAMES = BATTLE_FPS * 2,
    BATTLE_HISS_CHANNEL_PERCENT = 80,
    /* Five events are reachable before normal delivery; retain Task 2 headroom. */
    BATTLE_PENDING_EVENT_CAPACITY = 16
};

/*
 * A nonzero event capacity preserves ordered events for later delivery. Calling
 * battleTick with a zero event capacity intentionally discards events while
 * still advancing one simulation frame.
 */

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
    EVENT_BATTLE_END,
    EVENT_WARNING,
    EVENT_HISS_SUCCESS,
    EVENT_HISS_FAIL,
    EVENT_DODGE,
    EVENT_HIT,
    EVENT_CHANNEL_STOP,
    EVENT_STUN
} BattleEventType;

typedef uint16_t (*BattleRandom)(void *context, uint16_t upper_exclusive);

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
    BattleRandom random;
    void *random_context;
    uint32_t pending_scratch_frames;
    Side pending_scratch_source;
    BattleEvent pending_events[BATTLE_PENDING_EVENT_CAPACITY];
    size_t pending_event_count;
    bool paused;
    bool finished;
    Side winner;
} BattleState;

int counterPercent(int rage);
void battleInit(BattleState *battle, const FighterSpec *player,
                const FighterSpec *ai, uint32_t seed);
bool battleSubmit(BattleState *battle, Side side, BattleCommand command);
size_t battleTick(BattleState *battle, BattleEvent *events,
                  size_t event_capacity);

#endif
