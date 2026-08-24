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

static bool battleCanQueueEvents(const BattleState *battle, size_t count)
{
    return count <= BATTLE_PENDING_EVENT_CAPACITY - battle->pending_event_count;
}

static void battleQueueEvents(BattleState *battle, const BattleEvent *events,
                              size_t event_count)
{
    size_t index;

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
    BattleEvent damage_event;
    BattleEvent end_event;
    size_t event_count;

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

    if (command == CMD_YOWL) {
        battleStopChannel(actor);
        actor->cooldown = battle->spec[side].action_cd_frames;
        actor->channel = CHANNEL_YOWL;
        return true;
    }
    if (command == CMD_HEAL) {
        battleStopChannel(actor);
        actor->cooldown = battle->spec[side].action_cd_frames;
        actor->channel = CHANNEL_HEAL;
        return true;
    }

    spec = &battle->spec[side];
    target_side = battleOpponent(side);
    target = &battle->fighter[target_side];
    damage = spec->attack + (actor->rage / 10) * 2;
    if (damage < 0) {
        damage = 0;
    }
    event_count = target->hp <= damage ? 2u : 1u;
    if (!battleCanQueueEvents(battle, event_count)) {
        return false;
    }

    damage_event.type = EVENT_DAMAGE;
    damage_event.source = side;
    damage_event.target = target_side;
    damage_event.amount = damage;
    battleStopChannel(actor);
    actor->cooldown = spec->action_cd_frames;
    battleStopChannel(target);
    target->hp -= damage;
    if (target->hp <= 0) {
        target->hp = 0;
        battle->finished = true;
        battle->winner = side;
        end_event.type = EVENT_BATTLE_END;
        end_event.source = side;
        end_event.target = target_side;
        end_event.amount = 0;
        battleQueueEvents(battle, &damage_event, 1u);
        battleQueueEvents(battle, &end_event, 1u);
    } else {
        battleQueueEvents(battle, &damage_event, 1u);
    }
    return true;
}

size_t battleTick(BattleState *battle, BattleEvent *events,
                  size_t event_capacity)
{
    size_t event_count;
    BattleEvent frame_events[SIDE_COUNT];
    Side side;

    if (battle == 0 || battle->paused || (events == 0 && event_capacity != 0u)) {
        return 0u;
    }

    if (battle->pending_event_count > 0u) {
        return battleFlushEvents(battle, events, event_capacity);
    }
    if (battle->finished) {
        return 0u;
    }

    event_count = 0u;
    for (side = SIDE_PLAYER; side < SIDE_COUNT; ++side) {
        const FighterState *fighter = &battle->fighter[side];
        uint32_t next_frame = fighter->channel_frames + 1u;

        if ((fighter->channel == CHANNEL_YOWL &&
             next_frame % BATTLE_RAGE_TICK_FRAMES == 0u) ||
            (fighter->channel == CHANNEL_HEAL &&
             next_frame % BATTLE_HEAL_TICK_FRAMES == 0u)) {
            ++event_count;
        }
    }
    if (!battleCanQueueEvents(battle, event_count)) {
        return 0u;
    }

    event_count = 0u;
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
            frame_events[event_count].type = EVENT_RAGE;
            frame_events[event_count].source = side;
            frame_events[event_count].target = side;
            frame_events[event_count].amount = fighter->rage - old_rage;
            ++event_count;
        } else if (fighter->channel == CHANNEL_HEAL &&
                   fighter->channel_frames % BATTLE_HEAL_TICK_FRAMES == 0u) {
            int old_hp = fighter->hp;

            fighter->hp += spec->heal_per_tick;
            if (fighter->hp > spec->max_hp) {
                fighter->hp = spec->max_hp;
            }
            frame_events[event_count].type = EVENT_HEAL;
            frame_events[event_count].source = side;
            frame_events[event_count].target = side;
            frame_events[event_count].amount = fighter->hp - old_hp;
            ++event_count;
        }
    }

    battleQueueEvents(battle, frame_events, event_count);
    return battleFlushEvents(battle, events, event_capacity);
}
