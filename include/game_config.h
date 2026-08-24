#ifndef GAME_CONFIG_H
#define GAME_CONFIG_H

#define GAME_FPS 60

/* Integer weights used by the deterministic combat AI. */
#define AI_WEIGHT_LETHAL_SCRATCH 10000
#define AI_WEIGHT_INTERRUPT_ACTIVE_CHANNEL 300
#define AI_WEIGHT_LOW_RAGE_YOWL 200
#define AI_WEIGHT_MISSING_HP_HEAL 20
#define AI_WEIGHT_THREAT_SUPPRESSION 200

typedef enum CatId {
    CAT_ORANGE,
    CAT_TABBY,
    CAT_MAODIE,
    CAT_CHOUJU,
    CAT_BANANA,
    CAT_COUNT
} CatId;

typedef struct CatBaseStats {
    int max_hp;
    int attack;
    int rage_per_tick;
    int heal_per_tick;
    int rage_cap;
    int action_cd_frames;
} CatBaseStats;

static const CatBaseStats cat_base_stats[CAT_COUNT] = {
    [CAT_ORANGE] = { 60, 15, 5, 15, 100, 120 },
    [CAT_TABBY] = { 65, 16, 5, 12, 100, 120 },
    [CAT_MAODIE] = { 70, 10, 5, 15, 75, 120 },
    [CAT_CHOUJU] = { 60, 10, 10, 12, 100, 120 },
    [CAT_BANANA] = { 60, 15, 5, 15, 100, 90 }
};

static inline const CatBaseStats *configCatBase(CatId id)
{
    if ((unsigned int)id >= CAT_COUNT) {
        return &cat_base_stats[CAT_ORANGE];
    }

    return &cat_base_stats[id];
}

#endif
