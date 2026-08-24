#include "battle.h"

#include <assert.h>
#include <string.h>

typedef struct ScratchResolution {
    bool countered;
    bool dodged;
    bool hit;
    bool lethal;
    int damage;
} ScratchResolution;

static bool battleSideIsValid(Side side)
{
    return side == SIDE_PLAYER || side == SIDE_AI;
}

static Side battleOpponent(Side side)
{
    return side == SIDE_PLAYER ? SIDE_AI : SIDE_PLAYER;
}

static bool battleStopChannel(FighterState *fighter)
{
    bool was_active = fighter->channel != CHANNEL_NONE;

    fighter->channel = CHANNEL_NONE;
    fighter->channel_frames = 0u;
    return was_active;
}

static void battleAddEvent(BattleEvent *events, size_t *count,
                           BattleEventType type, Side source, Side target,
                           int amount)
{
    events[*count].type = type;
    events[*count].source = source;
    events[*count].target = target;
    events[*count].amount = amount;
    ++*count;
}

static bool battleCanQueueEvents(const BattleState *battle, size_t count)
{
    return count <= BATTLE_PENDING_EVENT_CAPACITY - battle->pending_event_count;
}

static void battleQueueEvents(BattleState *battle, const BattleEvent *events,
                              size_t event_count)
{
    size_t index;

    assert(battleCanQueueEvents(battle, event_count));
    for (index = 0u; index < event_count; ++index) {
        battle->pending_events[battle->pending_event_count++] = events[index];
    }
}

static size_t battleFlushEvents(BattleState *battle, BattleEvent *events,
                                size_t event_capacity)
{
    size_t count = 0u;

    while (battle->pending_event_count > 0u && count < event_capacity) {
        BattleEvent event = battle->pending_events[0];
        size_t index;

        events[count++] = event;
        for (index = 1u; index < battle->pending_event_count; ++index) {
            battle->pending_events[index - 1u] = battle->pending_events[index];
        }
        --battle->pending_event_count;
        if (event.type == EVENT_BATTLE_END) {
            break;
        }
    }

    return count;
}

static uint16_t battleDefaultRandom(void *context, uint16_t upper_exclusive)
{
    BattleRng *rng = context;

    if (upper_exclusive == 0u) {
        return 0u;
    }
    return (uint16_t)(battleRngNext(rng) % upper_exclusive);
}

static uint16_t battleRoll(BattleState *battle)
{
    uint16_t roll;

    assert(battle->random != 0);
    roll = battle->random(battle->random_context, 100u);
    return roll < 100u ? roll : 99u;
}

static bool battleRollPercent(BattleState *battle, int percent)
{
    if (percent < 0) {
        return false;
    }
    if (percent == 0) {
        return false;
    }
    if (percent >= 100) {
        return true;
    }
    return battleRoll(battle) < (uint16_t)percent;
}

static int battleCounterPercent(int base_percent, int rage)
{
    int percent;

    if (base_percent < 0) {
        base_percent = 0;
    } else if (base_percent > 100) {
        base_percent = 100;
    }
    percent = base_percent - rage * 3;
    return percent < 0 ? 0 : percent;
}

int counterPercent(int rage)
{
    int percent = battleCounterPercent(40, rage);

    return percent > 100 ? 100 : percent;
}

static int battleScratchDamage(const FighterState *actor,
                               const FighterSpec *spec)
{
    int damage = spec->attack + (actor->rage / 10) * 2;

    return damage < 0 ? 0 : damage;
}

static ScratchResolution battlePrepareScratch(BattleState *battle, Side side,
                                              bool allow_counter,
                                              BattleEvent *events,
                                              size_t *event_count)
{
    FighterState *actor = &battle->fighter[side];
    Side target_side = battleOpponent(side);
    FighterState *target = &battle->fighter[target_side];
    ScratchResolution result = { false, false, false, false, 0 };

    if (allow_counter && target->cooldown == 0u && target->stun == 0u &&
        battleRollPercent(battle,
                          battleCounterPercent(
                              battle->spec[target_side].counter_percent,
                              actor->rage))) {
        result.countered = true;
        battleAddEvent(events, event_count, EVENT_HISS_SUCCESS, target_side,
                       side, 0);
        if (target->channel != CHANNEL_NONE) {
            battleAddEvent(events, event_count, EVENT_CHANNEL_STOP,
                           target_side, target_side, 0);
        }
        battleAddEvent(events, event_count, EVENT_STUN, target_side, side,
                       BATTLE_STUN_FRAMES);
        return result;
    }
    if (battleRollPercent(battle, battle->spec[target_side].dodge_percent)) {
        result.dodged = true;
        battleAddEvent(events, event_count, EVENT_DODGE, side, target_side, 0);
        return result;
    }

    result.hit = true;
    result.damage = battleScratchDamage(actor, &battle->spec[side]);
    result.lethal = target->hp <= result.damage;
    battleAddEvent(events, event_count, EVENT_HIT, side, target_side,
                   result.damage);
    if (target->channel != CHANNEL_NONE) {
        battleAddEvent(events, event_count, EVENT_CHANNEL_STOP, side,
                       target_side, 0);
    }
    battleAddEvent(events, event_count, EVENT_DAMAGE, side, target_side,
                   result.damage);
    if (result.lethal) {
        battleAddEvent(events, event_count, EVENT_BATTLE_END, side,
                       target_side, 0);
    }
    return result;
}

static void battleApplyScratch(BattleState *battle, Side side,
                               const ScratchResolution *result)
{
    Side target_side = battleOpponent(side);
    FighterState *actor = &battle->fighter[side];
    FighterState *target = &battle->fighter[target_side];

    if (result->countered) {
        (void)battleStopChannel(target);
        actor->rage = 0;
        actor->stun = BATTLE_STUN_FRAMES;
        return;
    }
    if (result->dodged) {
        return;
    }

    assert(result->hit);
    (void)battleStopChannel(target);
    target->hp -= result->damage;
    if (result->lethal) {
        target->hp = 0;
        battle->finished = true;
        battle->winner = side;
    }
}

static size_t battlePrepareChannelEvents(const BattleState *battle,
                                         BattleEvent *events)
{
    size_t count = 0u;
    Side side;

    for (side = SIDE_PLAYER; side < SIDE_COUNT; ++side) {
        const FighterState *fighter = &battle->fighter[side];
        const FighterSpec *spec = &battle->spec[side];
        uint32_t next_frame;

        if (fighter->channel == CHANNEL_NONE) {
            continue;
        }
        next_frame = fighter->channel_frames + 1u;
        if (fighter->channel == CHANNEL_YOWL &&
            next_frame % BATTLE_RAGE_TICK_FRAMES == 0u) {
            int new_rage = fighter->rage + spec->rage_per_tick;

            if (new_rage > spec->rage_cap) {
                new_rage = spec->rage_cap;
            }
            battleAddEvent(events, &count, EVENT_RAGE, side, side,
                           new_rage - fighter->rage);
        } else if (fighter->channel == CHANNEL_HEAL &&
                   next_frame % BATTLE_HEAL_TICK_FRAMES == 0u) {
            int new_hp = fighter->hp + spec->heal_per_tick;

            if (new_hp > spec->max_hp) {
                new_hp = spec->max_hp;
            }
            battleAddEvent(events, &count, EVENT_HEAL, side, side,
                           new_hp - fighter->hp);
        }
    }
    return count;
}

static void battleAdvanceFighters(BattleState *battle)
{
    Side side;

    for (side = SIDE_PLAYER; side < SIDE_COUNT; ++side) {
        FighterState *fighter = &battle->fighter[side];
        const FighterSpec *spec = &battle->spec[side];

        if (fighter->cooldown > 0u) {
            --fighter->cooldown;
        }
        if (fighter->stun > 0u) {
            --fighter->stun;
        }
        if (fighter->channel == CHANNEL_NONE) {
            continue;
        }

        ++fighter->channel_frames;
        if (fighter->channel == CHANNEL_YOWL &&
            fighter->channel_frames % BATTLE_RAGE_TICK_FRAMES == 0u) {
            fighter->rage += spec->rage_per_tick;
            if (fighter->rage > spec->rage_cap) {
                fighter->rage = spec->rage_cap;
            }
        } else if (fighter->channel == CHANNEL_HEAL &&
                   fighter->channel_frames % BATTLE_HEAL_TICK_FRAMES == 0u) {
            fighter->hp += spec->heal_per_tick;
            if (fighter->hp > spec->max_hp) {
                fighter->hp = spec->max_hp;
            }
        }
    }
}

void battleInit(BattleState *battle, const FighterSpec *player,
                const FighterSpec *ai, uint32_t seed)
{
    if (battle == 0) {
        return;
    }

    memset(battle, 0, sizeof(*battle));
    if (player == 0 || ai == 0) {
        return;
    }

    battle->spec[SIDE_PLAYER] = *player;
    battle->spec[SIDE_AI] = *ai;
    battle->fighter[SIDE_PLAYER].hp = player->max_hp;
    battle->fighter[SIDE_AI].hp = ai->max_hp;
    battle->winner = SIDE_PLAYER;
    battle->pending_scratch_source = (Side)SIDE_COUNT;
    battleRngSeed(&battle->rng, seed);
    battle->random = battleDefaultRandom;
    battle->random_context = &battle->rng;
}

bool battleSubmit(BattleState *battle, Side side, BattleCommand command)
{
    FighterState *actor;
    FighterState *target;
    Side target_side;
    BattleEvent action_events[5];
    size_t event_count = 0u;
    ScratchResolution result;
    bool hiss_success;

    if (battle == 0 || !battleSideIsValid(side) || command < CMD_HISS ||
        command > CMD_HEAL || battle->paused || battle->finished) {
        return false;
    }

    actor = &battle->fighter[side];
    if (actor->cooldown != 0u || actor->stun != 0u) {
        return false;
    }
    target_side = battleOpponent(side);
    target = &battle->fighter[target_side];

    if (command == CMD_HISS) {
        hiss_success = battle->pending_scratch_frames != 0u &&
                       battle->pending_scratch_source == target_side;
        if (!hiss_success && target->channel != CHANNEL_NONE) {
            hiss_success = battleRollPercent(battle, BATTLE_HISS_CHANNEL_PERCENT);
        }
        if (actor->channel != CHANNEL_NONE) {
            battleAddEvent(action_events, &event_count, EVENT_CHANNEL_STOP,
                           side, side, 0);
        }
        if (hiss_success) {
            battleAddEvent(action_events, &event_count, EVENT_HISS_SUCCESS,
                           side, target_side, 0);
            if (target->channel != CHANNEL_NONE) {
                battleAddEvent(action_events, &event_count,
                               EVENT_CHANNEL_STOP, side, target_side, 0);
            }
            battleAddEvent(action_events, &event_count, EVENT_STUN, side,
                           target_side, BATTLE_STUN_FRAMES);
        } else {
            battleAddEvent(action_events, &event_count, EVENT_HISS_FAIL, side,
                           target_side, 0);
        }
        if (!battleCanQueueEvents(battle, event_count)) {
            return false;
        }
        (void)battleStopChannel(actor);
        if (hiss_success) {
            battle->pending_scratch_frames = 0u;
            battle->pending_scratch_source = (Side)SIDE_COUNT;
            (void)battleStopChannel(target);
            target->rage = 0;
            target->stun = BATTLE_STUN_FRAMES;
        } else {
            actor->cooldown = battle->spec[side].action_cd_frames;
        }
        battleQueueEvents(battle, action_events, event_count);
        return true;
    }

    if (command == CMD_YOWL || command == CMD_HEAL) {
        if (actor->channel != CHANNEL_NONE) {
            battleAddEvent(action_events, &event_count, EVENT_CHANNEL_STOP,
                           side, side, 0);
        }
        if (!battleCanQueueEvents(battle, event_count)) {
            return false;
        }
        (void)battleStopChannel(actor);
        actor->cooldown = battle->spec[side].action_cd_frames;
        actor->channel = command == CMD_YOWL ? CHANNEL_YOWL : CHANNEL_HEAL;
        battleQueueEvents(battle, action_events, event_count);
        return true;
    }

    if (side == SIDE_AI &&
        battleRollPercent(battle, battle->spec[target_side].warning_percent)) {
        if (actor->channel != CHANNEL_NONE) {
            battleAddEvent(action_events, &event_count, EVENT_CHANNEL_STOP,
                           side, side, 0);
        }
        battleAddEvent(action_events, &event_count, EVENT_WARNING, side,
                       target_side, BATTLE_WARNING_FRAMES);
        if (!battleCanQueueEvents(battle, event_count)) {
            return false;
        }
        (void)battleStopChannel(actor);
        actor->cooldown = battle->spec[side].action_cd_frames;
        battle->pending_scratch_frames = BATTLE_WARNING_FRAMES;
        battle->pending_scratch_source = side;
        battleQueueEvents(battle, action_events, event_count);
        return true;
    }

    if (actor->channel != CHANNEL_NONE) {
        battleAddEvent(action_events, &event_count, EVENT_CHANNEL_STOP, side,
                       side, 0);
    }
    result = battlePrepareScratch(battle, side, side == SIDE_PLAYER,
                                  action_events, &event_count);
    if (!battleCanQueueEvents(battle, event_count)) {
        return false;
    }
    (void)battleStopChannel(actor);
    actor->cooldown = battle->spec[side].action_cd_frames;
    battleApplyScratch(battle, side, &result);
    battleQueueEvents(battle, action_events, event_count);
    return true;
}

size_t battleTick(BattleState *battle, BattleEvent *events,
                  size_t event_capacity)
{
    BattleEvent frame_events[4];
    size_t event_count;
    size_t frame_event_count;

    if (battle == 0 || battle->paused || (events == 0 && event_capacity != 0u)) {
        return 0u;
    }

    if (event_capacity == 0u) {
        battle->pending_event_count = 0u;
        event_count = 0u;
    } else {
        event_count = battleFlushEvents(battle, events, event_capacity);
    }
    if (battle->finished) {
        return event_count;
    }

    if (battle->pending_scratch_frames > 0u) {
        --battle->pending_scratch_frames;
        if (battle->pending_scratch_frames == 0u) {
            Side source = battle->pending_scratch_source;
            ScratchResolution result;

            assert(battleSideIsValid(source));
            frame_event_count = 0u;
            result = battlePrepareScratch(battle, source, false, frame_events,
                                          &frame_event_count);
            if (event_capacity != 0u) {
                assert(battleCanQueueEvents(battle, frame_event_count));
                battleQueueEvents(battle, frame_events, frame_event_count);
            }
            battle->pending_scratch_source = (Side)SIDE_COUNT;
            battleApplyScratch(battle, source, &result);
            if (battle->finished) {
                if (event_capacity != 0u && event_count < event_capacity) {
                    event_count += battleFlushEvents(
                        battle, &events[event_count], event_capacity - event_count);
                }
                return event_count;
            }
        }
    }

    frame_event_count = battlePrepareChannelEvents(battle, frame_events);
    if (event_capacity != 0u) {
        assert(battleCanQueueEvents(battle, frame_event_count));
        battleQueueEvents(battle, frame_events, frame_event_count);
    }
    battleAdvanceFighters(battle);
    if (event_capacity != 0u && event_count < event_capacity) {
        event_count += battleFlushEvents(battle, &events[event_count],
                                         event_capacity - event_count);
    }
    return event_count;
}
