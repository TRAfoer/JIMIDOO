#ifndef AI_H
#define AI_H

#include "battle.h"

#include <stdint.h>

enum {
    AI_SCORE_FORBIDDEN = -1,
    AI_SCORE_MAX = 10000
};

typedef struct AiScores {
    int score[CMD_HEAL + 1];
} AiScores;

typedef enum AiProfile {
    AI_PROFILE_AGGRESSIVE,
    AI_PROFILE_COUNTER,
    AI_PROFILE_RAGE,
    AI_PROFILE_SURVIVAL,
    AI_PROFILE_OPPORTUNIST,
    AI_PROFILE_TRICKSTER,
    AI_PROFILE_COUNT
} AiProfile;

enum {
    AI_MEMORY_CAPACITY = 4,
    AI_TICKET_TOTAL = 10000,
    AI_OPENING_PATIENCE_FRAMES = 120,
    AI_MAX_OBSERVE_FRAMES = 36
};

typedef struct AiMemory {
    BattleCommand player[AI_MEMORY_CAPACITY];
    uint8_t player_count;
    BattleCommand last_ai_action;
    uint8_t ai_repeat_count;
} AiMemory;

typedef struct AiBrain {
    BattleRng rng;
    AiProfile profile;
    AiMemory memory;
    uint16_t last_ticket[CMD_HEAL + 1];
    uint16_t opening_frames_remaining;
    uint16_t observe_frames_remaining;
    bool opening_choice_made;
    bool opening_waiting;
    bool observing;
} AiBrain;

typedef struct AiDebugSnapshot {
    AiProfile profile;
    BattleCommand player_history[AI_MEMORY_CAPACITY];
    uint8_t player_history_count;
    uint16_t opening_frames_remaining;
    uint16_t observe_frames_remaining;
    uint16_t ticket[CMD_HEAL + 1];
} AiDebugSnapshot;

AiScores aiScoreActions(const BattleState *battle, Side side);
uint16_t aiActionProbabilityCap(uint8_t crisis);
void aiBrainInit(AiBrain *brain, uint32_t battle_seed);
void aiBrainRecordAccepted(AiBrain *brain, Side side, BattleCommand command);
BattleCommand aiBrainChooseNow(AiBrain *brain, const BattleState *battle,
                               Side side, uint8_t crisis);
BattleCommand aiBrainTick(AiBrain *brain, const BattleState *battle,
                          Side side, uint8_t crisis);
void aiBrainSnapshot(const AiBrain *brain, AiDebugSnapshot *snapshot);

#endif
