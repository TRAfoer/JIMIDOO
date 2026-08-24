#include "battle.h"

#include <string.h>

static bool battleSideIsValid(Side side)
{
    return side == SIDE_PLAYER || side == SIDE_AI;
}

static Side battleOpponent(Side side)
{
    return side == SIDE_PLAYER ? SIDE_AI : SIDE_PLAYER;
}

static void battleStopChannel(FighterState *fighter)
{
    fighter->channel = CHANNEL_NONE;
    fighter->channel_frames = 0u;
}

static void battleQueueEvent(BattleState *battle, BattleEventType type,
                             Side source, Side target, int amount)
{
    BattleEvent *event;

    if (battle->pending_event_count >= BATTLE_PENDING_EVENT_CAPACITY) {
        return;
    }

    event = &battle->pending_events[battle->pending_event_count++];
    event->type = type;
    event->source = source;
    event->target = target;
    event->amount = amount;
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

static void battleEmit(BattleEvent *events, size_t event_capacity,
                       size_t *event_count, BattleEventType type, Side source,
                       Side target, int amount)
{
    if (*event_count >= event_capacity) {
        return;
    }

    events[*event_count].type = type;
    events[*event_count].source = source;
    events[*event_count].target = target;
    events[*event_count].amount = amount;
    ++*event_count;
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
    battleRngSeed(&battle->rng, seed);
}

bool battleSubmit(BattleState *battle, Side side, BattleCommand command)
{
    FighterState *actor;
    FighterState *target;
    const FighterSpec *spec;
    Side target_side;
    int damage;

    if (battle == 0 || !battleSideIsValid(side) || command < CMD_HISS ||
        command > CMD_HEAL || battle->paused || battle->finished) {
        return false;
    }

    actor = &battle->fighter[side];
    if (actor->cooldown != 0u || actor->stun != 0u) {
        return false;
    }

    if (command == CMD_HISS) {
        return true;
    }

    spec = &battle->spec[side];
    battleStopChannel(actor);
    actor->cooldown = spec->action_cd_frames;

    if (command == CMD_YOWL) {
        actor->channel = CHANNEL_YOWL;
        return true;
    }
    if (command == CMD_HEAL) {
        actor->channel = CHANNEL_HEAL;
        return true;
    }
    if (command != CMD_SCRATCH) {
        actor->cooldown = 0u;
        return false;
    }

    target_side = battleOpponent(side);
    target = &battle->fighter[target_side];
    battleStopChannel(target);
    damage = spec->attack + (actor->rage / 10) * 2;
    if (damage < 0) {
        damage = 0;
    }
    target->hp -= damage;
    if (target->hp < 0) {
        target->hp = 0;
    }
    battleQueueEvent(battle, EVENT_DAMAGE, side, target_side, damage);
    if (target->hp == 0) {
        battle->finished = true;
        battle->winner = side;
        battleQueueEvent(battle, EVENT_BATTLE_END, side, target_side, 0);
    }
    return true;
}

size_t battleTick(BattleState *battle, BattleEvent *events,
                  size_t event_capacity)
{
    size_t event_count;
    Side side;

    if (battle == 0 || battle->paused || (events == 0 && event_capacity != 0u)) {
        return 0u;
    }

    event_count = battleFlushEvents(battle, events, event_capacity);
    if (battle->finished) {
        return event_count;
    }

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
            int old_rage = fighter->rage;

            fighter->rage += spec->rage_per_tick;
            if (fighter->rage > spec->rage_cap) {
                fighter->rage = spec->rage_cap;
            }
            battleEmit(events, event_capacity, &event_count, EVENT_RAGE, side,
                       side, fighter->rage - old_rage);
        } else if (fighter->channel == CHANNEL_HEAL &&
                   fighter->channel_frames % BATTLE_HEAL_TICK_FRAMES == 0u) {
            int old_hp = fighter->hp;

            fighter->hp += spec->heal_per_tick;
            if (fighter->hp > spec->max_hp) {
                fighter->hp = spec->max_hp;
            }
            battleEmit(events, event_capacity, &event_count, EVENT_HEAL, side,
                       side, fighter->hp - old_hp);
        }
    }

    return event_count;
}
