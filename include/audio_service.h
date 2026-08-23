#ifndef AUDIO_SERVICE_H
#define AUDIO_SERVICE_H

#include <stdbool.h>

#include "game_config.h"

typedef enum SfxId {
    SFX_ID_START,
    SFX_ID_YOWL,
    SFX_ID_NORMAL_HISS,
    SFX_ID_SCRATCH_1,
    SFX_ID_SCRATCH_2,
    SFX_ID_HEAL,
    SFX_ID_DEATH,
    SFX_ID_MAODIE_COMBINED,
    SFX_ID_BANANA_ATTACK,
    SFX_ID_BANANA_HEAL,
    SFX_ID_BANANA_DEATH,
    SFX_ID_COUNT
} SfxId;

typedef enum MusicId {
    MUSIC_NONE,
    MUSIC_MENU,
    MUSIC_BATTLE,
    MUSIC_COUNT
} MusicId;

bool audioInit(void);
void audioPlaySfx(SfxId id);
void audioPlayScratch(CatId cat);
void audioPlayHiss(CatId cat);
void audioPlayHeal(CatId cat);
void audioPlayDeath(CatId cat);
void audioSetMusic(MusicId id);
void audioUpdate(void);
void audioShutdown(void);

#endif
