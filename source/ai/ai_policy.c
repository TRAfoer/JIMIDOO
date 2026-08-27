#include "ai_internal.h"

#include <limits.h>
#include <string.h>

typedef struct AiProfileConfig {
    uint8_t percent[CMD_HEAL + 1];
    bool opportunist;
    bool trickster;
} AiProfileConfig;

static const AiProfileConfig ai_profile[AI_PROFILE_COUNT] = {
    [AI_PROFILE_AGGRESSIVE] = { { 0u, 90u, 145u, 80u, 75u }, false, false },
    [AI_PROFILE_COUNTER] = { { 0u, 150u, 100u, 85u, 90u }, false, false },
    [AI_PROFILE_RAGE] = { { 0u, 90u, 110u, 155u, 80u }, false, false },
    [AI_PROFILE_SURVIVAL] = { { 0u, 105u, 80u, 105u, 150u }, false, false },
    [AI_PROFILE_OPPORTUNIST] = { { 0u, 100u, 100u, 100u, 100u }, true, false },
    [AI_PROFILE_TRICKSTER] = { { 0u, 100u, 100u, 100u, 100u }, false, true }
};

static bool aiPolicySideIsValid(Side side)
{
    return side == SIDE_PLAYER || side == SIDE_AI;
}

static Side aiPolicyOpponent(Side side)
{
    return side == SIDE_PLAYER ? SIDE_AI : SIDE_PLAYER;
}

static AiProfile aiPolicyProfileOrDefault(AiProfile profile)
{
    if ((unsigned int)profile >= (unsigned int)AI_PROFILE_COUNT) {
        return AI_PROFILE_AGGRESSIVE;
    }
    return profile;
}

static uint64_t aiPolicyAddSaturating(uint64_t left, uint64_t right)
{
    return left > UINT64_MAX - right ? UINT64_MAX : left + right;
}

static uint64_t aiPolicyPercent(uint64_t value, int percent)
{
    uint64_t factor;

    if (value == 0u || percent <= 0) {
        return 0u;
    }
    factor = (uint64_t)percent;
    if (value > UINT64_MAX / factor) {
        return UINT64_MAX;
    }
    return value * factor / 100u;
}

static bool aiPolicyIsLethalScratch(const BattleState *battle, Side side)
{
    const FighterState *fighter;
    const FighterState *opponent;
    const FighterSpec *spec;
    int64_t damage;

    if (battle == 0 || !aiPolicySideIsValid(side)) {
        return false;
    }
    fighter = &battle->fighter[side];
    opponent = &battle->fighter[aiPolicyOpponent(side)];
    spec = &battle->spec[side];
    damage = (int64_t)spec->attack + ((int64_t)fighter->rage / 10) * 2;
    return damage > 0 && opponent->hp <= damage;
}

static bool aiPolicyIsInterruptingHiss(const BattleState *battle, Side side)
{
    Channel channel;

    if (battle == 0 || !aiPolicySideIsValid(side)) {
        return false;
    }
    channel = battle->fighter[aiPolicyOpponent(side)].channel;
    return channel == CHANNEL_YOWL || channel == CHANNEL_HEAL;
}

static uint8_t aiPolicyMemoryUsePercent(uint8_t crisis)
{
    if (crisis <= 24u) {
        return 25u;
    }
    if (crisis <= 74u) {
        return 45u;
    }
    if (crisis <= 149u) {
        return 65u;
    }
    if (crisis <= 224u) {
        return 80u;
    }
    return 100u;
}

static uint8_t aiPolicyNoiseLimit(uint8_t crisis)
{
    if (crisis <= 24u) {
        return 35u;
    }
    if (crisis <= 74u) {
        return 30u;
    }
    if (crisis <= 149u) {
        return 25u;
    }
    if (crisis <= 224u) {
        return 20u;
    }
    return 15u;
}

int8_t aiPolicyGenerateNoise(BattleRng *rng, uint8_t crisis)
{
    uint8_t limit;

    if (rng == 0) {
        return 0;
    }
    limit = aiPolicyNoiseLimit(crisis);
    return (int8_t)(battleRngNext(rng) % (limit * 2u + 1u)) -
           (int8_t)limit;
}

uint16_t aiActionProbabilityCap(uint8_t crisis)
{
    if (crisis <= 24u) {
        return 35u;
    }
    if (crisis <= 74u) {
        return 55u;
    }
    if (crisis <= 149u) {
        return 70u;
    }
    if (crisis <= 224u) {
        return 80u;
    }
    return 85u;
}

static void aiPolicyMemoryPercent(const BattleState *battle, Side side,
                                  const AiMemory *memory, int raw_percent[],
                                  bool hiss_suppression[])
{
    uint8_t index;
    const FighterState *fighter;
    const FighterSpec *spec;

    if (battle == 0 || memory == 0 || !aiPolicySideIsValid(side)) {
        return;
    }
    fighter = &battle->fighter[side];
    spec = &battle->spec[side];
    for (index = 0u; index < memory->player_count &&
                    index < AI_MEMORY_CAPACITY; ++index) {
        BattleCommand command = memory->player[index];
        int recency = (int)AI_MEMORY_CAPACITY - (int)index;

        if (command == CMD_YOWL || command == CMD_HEAL) {
            raw_percent[CMD_HISS] += recency * 20;
        } else if (command == CMD_HISS) {
            raw_percent[CMD_SCRATCH] += recency * 12;
            raw_percent[CMD_YOWL] -= recency * 8;
            raw_percent[CMD_HEAL] -= recency * 8;
            hiss_suppression[CMD_YOWL] = true;
            hiss_suppression[CMD_HEAL] = true;
        } else if (command == CMD_SCRATCH) {
            if ((int64_t)fighter->hp * 4 <= (int64_t)spec->max_hp * 3) {
                raw_percent[CMD_HEAL] += recency * 10;
            } else if ((int64_t)fighter->hp * 4 >
                           (int64_t)spec->max_hp * 3 &&
                       fighter->rage < spec->rage_cap) {
                raw_percent[CMD_YOWL] += recency * 8;
            } else {
                raw_percent[CMD_SCRATCH] += recency * 6;
            }
        }
    }
}

AiWeights aiPolicyWeights(const BattleState *battle, Side side,
                          uint8_t crisis, AiProfile profile,
                          const AiMemory *memory,
                          const int8_t noise_percent[CMD_HEAL + 1])
{
    AiWeights weights = { { 0u } };
    AiScores scores = aiScoreActions(battle, side);
    const AiProfileConfig *config;
    int raw_percent[CMD_HEAL + 1] = { 0 };
    bool hiss_suppression[CMD_HEAL + 1] = { false };
    BattleCommand command;

    profile = aiPolicyProfileOrDefault(profile);
    config = &ai_profile[profile];
    aiPolicyMemoryPercent(battle, side, memory, raw_percent, hiss_suppression);
    for (command = CMD_HISS; command <= CMD_HEAL; ++command) {
        uint64_t weight;
        uint64_t pre_memory;
        int effective_percent;

        if (scores.score[command] == AI_SCORE_FORBIDDEN) {
            continue;
        }
        weight = (uint64_t)(scores.score[command] > 0 ?
                            scores.score[command] : 0) + 100u;
        weight = aiPolicyPercent(weight, config->percent[command]);
        if (config->opportunist &&
            ((command == CMD_SCRATCH && aiPolicyIsLethalScratch(battle, side)) ||
             (command == CMD_HISS && aiPolicyIsInterruptingHiss(battle, side)))) {
            weight = aiPolicyPercent(weight, 175);
        }
        pre_memory = weight;
        effective_percent = raw_percent[command] *
                            (int)aiPolicyMemoryUsePercent(crisis) / 100;
        weight = aiPolicyPercent(weight, 100 + effective_percent);
        if (hiss_suppression[command] && weight < pre_memory / 4u) {
            weight = pre_memory / 4u;
        }
        if (memory != 0 && memory->last_ai_action == command &&
            memory->ai_repeat_count != 0u &&
            !((command == CMD_SCRATCH && aiPolicyIsLethalScratch(battle, side)) ||
              (command == CMD_HISS && aiPolicyIsInterruptingHiss(battle, side)))) {
            if (memory->ai_repeat_count == 1u) {
                weight = aiPolicyPercent(weight, config->trickster ? 55 : 70);
            } else {
                weight = aiPolicyPercent(weight, config->trickster ? 25 : 40);
            }
        }
        if (noise_percent != 0) {
            weight = aiPolicyPercent(weight,
                                     100 + (int)noise_percent[command]);
        }
        weights.value[command] = weight == 0u ? 1u : weight;
    }
    return weights;
}

static uint16_t aiPolicyShare(uint64_t weight, uint64_t total,
                              uint16_t available)
{
    uint64_t quotient;
    uint64_t remainder;
    uint64_t unit;
    uint16_t share;
    uint16_t index;

    if (weight == 0u || total == 0u || available == 0u) {
        return 0u;
    }
    quotient = weight / total;
    unit = weight % total;
    remainder = 0u;
    share = (uint16_t)(quotient * available);
    for (index = 0u; index < available; ++index) {
        if (remainder >= total - unit) {
            remainder -= total - unit;
            ++share;
        } else {
            remainder += unit;
        }
    }
    return share;
}

static void aiPolicyDistribute(AiTickets *tickets, const AiWeights *weights,
                               uint16_t available, BattleCommand excluded)
{
    uint64_t total = 0u;
    uint16_t allocated = 0u;
    uint16_t residue;
    BattleCommand command;

    for (command = CMD_HISS; command <= CMD_HEAL; ++command) {
        if (command != excluded) {
            total = aiPolicyAddSaturating(total, weights->value[command]);
        }
    }
    for (command = CMD_HISS; command <= CMD_HEAL; ++command) {
        uint16_t share;

        if (command == excluded || weights->value[command] == 0u) {
            continue;
        }
        share = aiPolicyShare(weights->value[command], total, available);
        tickets->value[command] = (uint16_t)(tickets->value[command] + share);
        allocated = (uint16_t)(allocated + share);
    }
    residue = (uint16_t)(available - allocated);
    for (command = CMD_HISS; residue != 0u && command <= CMD_HEAL; ++command) {
        if (command != excluded && weights->value[command] != 0u) {
            ++tickets->value[command];
            --residue;
        }
    }
}

static void aiPolicyDistributeCapped(AiTickets *tickets,
                                     const AiWeights *weights,
                                     uint16_t available,
                                     BattleCommand excluded, uint16_t cap)
{
    while (available != 0u) {
        uint64_t total = 0u;
        uint16_t allocated = 0u;
        BattleCommand command;

        for (command = CMD_HISS; command <= CMD_HEAL; ++command) {
            if (command != excluded && weights->value[command] != 0u &&
                tickets->value[command] < cap) {
                total = aiPolicyAddSaturating(total, weights->value[command]);
            }
        }
        if (total == 0u) {
            return;
        }
        for (command = CMD_HISS; command <= CMD_HEAL; ++command) {
            uint16_t share;
            uint16_t capacity;

            if (command == excluded || weights->value[command] == 0u ||
                tickets->value[command] >= cap) {
                continue;
            }
            share = aiPolicyShare(weights->value[command], total, available);
            capacity = (uint16_t)(cap - tickets->value[command]);
            if (share > capacity) {
                share = capacity;
            }
            tickets->value[command] = (uint16_t)(tickets->value[command] + share);
            allocated = (uint16_t)(allocated + share);
        }
        for (command = CMD_HISS; available > allocated &&
                                     command <= CMD_HEAL; ++command) {
            if (command != excluded && weights->value[command] != 0u &&
                tickets->value[command] < cap) {
                ++tickets->value[command];
                ++allocated;
            }
        }
        available = (uint16_t)(available - allocated);
    }
}

AiTickets aiPolicyTickets(AiWeights weights, uint16_t cap_percent)
{
    AiTickets tickets = { { 0u } };
    uint16_t legal_count = 0u;
    uint16_t cap;
    BattleCommand command;

    for (command = CMD_HISS; command <= CMD_HEAL; ++command) {
        if (weights.value[command] != 0u) {
            ++legal_count;
        }
    }
    if (legal_count == 0u) {
        return tickets;
    }
    if (legal_count == 1u) {
        for (command = CMD_HISS; command <= CMD_HEAL; ++command) {
            if (weights.value[command] != 0u) {
                tickets.value[command] = AI_TICKET_TOTAL;
                return tickets;
            }
        }
    }
    for (command = CMD_HISS; command <= CMD_HEAL; ++command) {
        if (weights.value[command] != 0u) {
            tickets.value[command] = 1u;
        }
    }
    aiPolicyDistribute(&tickets, &weights,
                       (uint16_t)(AI_TICKET_TOTAL - legal_count), CMD_NONE);

    cap = cap_percent > 100u ? AI_TICKET_TOTAL :
          (uint16_t)(cap_percent * 100u);
    if ((uint32_t)legal_count * cap < AI_TICKET_TOTAL) {
        cap = (uint16_t)((AI_TICKET_TOTAL + legal_count - 1u) / legal_count);
    }
    for (command = CMD_HISS; command <= CMD_HEAL; ++command) {
        if (tickets.value[command] > cap) {
            uint16_t excess = (uint16_t)(tickets.value[command] - cap);

            tickets.value[command] = cap;
            aiPolicyDistributeCapped(&tickets, &weights, excess, command, cap);
        }
    }
    return tickets;
}

void aiBrainInit(AiBrain *brain, uint32_t battle_seed)
{
    if (brain == 0) {
        return;
    }
    memset(brain, 0, sizeof(*brain));
    battleRngSeed(&brain->rng, battle_seed ^ UINT32_C(0xA17E5EED));
    brain->profile = (AiProfile)(battleRngNext(&brain->rng) % AI_PROFILE_COUNT);
    brain->opening_frames_remaining = AI_OPENING_PATIENCE_FRAMES;
}

void aiBrainRecordAccepted(AiBrain *brain, Side side, BattleCommand command)
{
    uint8_t index;

    if (brain == 0 || !aiPolicySideIsValid(side) || command < CMD_HISS ||
        command > CMD_HEAL) {
        return;
    }
    if (side == SIDE_PLAYER) {
        for (index = AI_MEMORY_CAPACITY - 1u; index > 0u; --index) {
            brain->memory.player[index] = brain->memory.player[index - 1u];
        }
        brain->memory.player[0] = command;
        if (brain->memory.player_count < AI_MEMORY_CAPACITY) {
            ++brain->memory.player_count;
        }
        if (!brain->opening_choice_made) {
            brain->opening_frames_remaining = 0u;
            brain->opening_choice_made = true;
            brain->opening_waiting = false;
        }
        return;
    }
    if (brain->memory.last_ai_action == command) {
        if (brain->memory.ai_repeat_count != UINT8_MAX) {
            ++brain->memory.ai_repeat_count;
        }
    } else {
        brain->memory.last_ai_action = command;
        brain->memory.ai_repeat_count = 1u;
    }
    if (command == CMD_YOWL) {
        brain->opening_frames_remaining = 0u;
        brain->opening_choice_made = true;
        brain->opening_waiting = false;
    }
}

BattleCommand aiBrainChooseNow(AiBrain *brain, const BattleState *battle,
                               Side side, uint8_t crisis)
{
    AiScores scores;
    int8_t noise[CMD_HEAL + 1] = { 0 };
    AiWeights weights;
    AiTickets tickets;
    uint16_t roll;
    uint16_t cumulative = 0u;
    BattleCommand command;

    if (brain == 0) {
        return CMD_NONE;
    }
    scores = aiScoreActions(battle, side);
    for (command = CMD_HISS; command <= CMD_HEAL; ++command) {
        if (scores.score[command] != AI_SCORE_FORBIDDEN) {
            noise[command] = aiPolicyGenerateNoise(&brain->rng, crisis);
        }
    }
    weights = aiPolicyWeights(battle, side, crisis, brain->profile,
                              &brain->memory, noise);
    tickets = aiPolicyTickets(weights, aiActionProbabilityCap(crisis));
    for (command = CMD_NONE; command <= CMD_HEAL; ++command) {
        brain->last_ticket[command] = tickets.value[command];
    }
    roll = (uint16_t)(battleRngNext(&brain->rng) % AI_TICKET_TOTAL);
    for (command = CMD_HISS; command <= CMD_HEAL; ++command) {
        cumulative = (uint16_t)(cumulative + tickets.value[command]);
        if (roll < cumulative) {
            return command;
        }
    }
    return CMD_NONE;
}
