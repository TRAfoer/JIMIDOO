#include "ai.h"

#include <string.h>

static const uint8_t opening_yowl_percent[AI_PROFILE_COUNT] = {
    20u, 30u, 70u, 35u, 45u, 50u
};

static const uint8_t observe_minimum[AI_PROFILE_COUNT] = {
    0u, 6u, 6u, 12u, 0u, 0u
};

static const uint8_t observe_maximum[AI_PROFILE_COUNT] = {
    18u, 30u, 24u, 36u, 24u, 36u
};

static bool aiBrainSideIsValid(Side side)
{
    return side == SIDE_PLAYER || side == SIDE_AI;
}

static bool aiBrainCanAct(const BattleState *battle, Side side)
{
    const FighterState *fighter;

    if (battle == 0 || !aiBrainSideIsValid(side) || battle->finished) {
        return false;
    }
    fighter = &battle->fighter[side];
    return fighter->hp > 0 && fighter->cooldown == 0u && fighter->stun == 0u;
}

static AiProfile aiBrainProfileOrDefault(AiProfile profile)
{
    if ((unsigned int)profile >= (unsigned int)AI_PROFILE_COUNT) {
        return AI_PROFILE_AGGRESSIVE;
    }
    return profile;
}

static void aiBrainFinishOpening(AiBrain *brain)
{
    brain->opening_frames_remaining = 0u;
    brain->opening_choice_made = true;
    brain->opening_waiting = false;
}

static void aiBrainCancelObservation(AiBrain *brain)
{
    brain->observe_frames_remaining = 0u;
    brain->observing = false;
}

static uint16_t aiBrainObserveDelay(AiBrain *brain)
{
    AiProfile profile = aiBrainProfileOrDefault(brain->profile);
    uint8_t minimum = observe_minimum[profile];
    uint8_t maximum = observe_maximum[profile];

    return (uint16_t)(minimum +
                      battleRngNext(&brain->rng) % (maximum - minimum + 1u));
}

BattleCommand aiBrainTick(AiBrain *brain, const BattleState *battle,
                          Side side, uint8_t crisis)
{
    AiProfile profile;
    uint16_t delay;

    if (brain == 0 || battle == 0 || !aiBrainSideIsValid(side)) {
        return CMD_NONE;
    }
    if (battle->paused) {
        return CMD_NONE;
    }
    if (!brain->opening_choice_made) {
        if (!brain->opening_waiting) {
            if (!aiBrainCanAct(battle, side)) {
                return CMD_NONE;
            }
            profile = aiBrainProfileOrDefault(brain->profile);
            if (battleRngNext(&brain->rng) % 100u <
                opening_yowl_percent[profile]) {
                aiBrainFinishOpening(brain);
                return CMD_YOWL;
            }
            brain->opening_waiting = true;
        }
        if (brain->opening_frames_remaining != 0u) {
            --brain->opening_frames_remaining;
        }
        if (brain->opening_frames_remaining == 0u) {
            aiBrainFinishOpening(brain);
        }
        return CMD_NONE;
    }
    if (!aiBrainCanAct(battle, side)) {
        aiBrainCancelObservation(brain);
        return CMD_NONE;
    }
    if (brain->observing) {
        if (brain->observe_frames_remaining != 0u) {
            --brain->observe_frames_remaining;
            return CMD_NONE;
        }
        brain->observing = false;
        return aiBrainChooseNow(brain, battle, side, crisis);
    }
    delay = aiBrainObserveDelay(brain);
    brain->observe_frames_remaining = delay;
    if (delay == 0u) {
        return aiBrainChooseNow(brain, battle, side, crisis);
    }
    brain->observing = true;
    return CMD_NONE;
}

void aiBrainSnapshot(const AiBrain *brain, AiDebugSnapshot *snapshot)
{
    if (snapshot == 0) {
        return;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->profile = AI_PROFILE_AGGRESSIVE;
    if (brain == 0) {
        return;
    }
    snapshot->profile = brain->profile;
    memcpy(snapshot->player_history, brain->memory.player,
           sizeof(snapshot->player_history));
    snapshot->player_history_count = brain->memory.player_count;
    snapshot->opening_frames_remaining = brain->opening_frames_remaining;
    snapshot->observe_frames_remaining = brain->observe_frames_remaining;
    memcpy(snapshot->ticket, brain->last_ticket, sizeof(snapshot->ticket));
}
