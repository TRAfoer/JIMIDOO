#include "audio_service.h"

#include <stdlib.h>

#include <filesystem.h>
#include <maxmod9.h>
#include <nds.h>

#include "soundbank.h"

bool musicStreamStart(MusicId id);
void musicStreamUpdate(void);
void musicStreamSetVolume(unsigned int volume);
void musicStreamClose(void);

#define MUSIC_FADE_FRAMES 30
#define MUSIC_MAX_VOLUME 127

typedef enum FadeMode {
    FADE_NONE,
    FADE_IN,
    FADE_OUT
} FadeMode;

static bool audio_ready;
static MusicId current_music = MUSIC_NONE;
static MusicId target_music = MUSIC_NONE;
static FadeMode fade_mode = FADE_NONE;
static unsigned int fade_frame;
static unsigned int fade_start_volume;
static unsigned int music_volume;

static const unsigned int physical_samples[] = {
    SFX_START,
    SFX_YOWL,
    SFX_SCRATCH_1,
    SFX_SCRATCH_2,
    SFX_HEAL,
    SFX_DEATH,
    SFX_MAODIE_COMBINED,
    SFX_BANANA_ATTACK,
    SFX_BANANA_HEAL,
    SFX_BANANA_DEATH
};

static const unsigned int logical_samples[SFX_ID_COUNT] = {
    [SFX_ID_START] = SFX_START,
    [SFX_ID_YOWL] = SFX_YOWL,
    [SFX_ID_NORMAL_HISS] = SFX_YOWL,
    [SFX_ID_SCRATCH_1] = SFX_SCRATCH_1,
    [SFX_ID_SCRATCH_2] = SFX_SCRATCH_2,
    [SFX_ID_HEAL] = SFX_HEAL,
    [SFX_ID_DEATH] = SFX_DEATH,
    [SFX_ID_MAODIE_COMBINED] = SFX_MAODIE_COMBINED,
    [SFX_ID_BANANA_ATTACK] = SFX_BANANA_ATTACK,
    [SFX_ID_BANANA_HEAL] = SFX_BANANA_HEAL,
    [SFX_ID_BANANA_DEATH] = SFX_BANANA_DEATH
};

static void setMusicVolume(unsigned int volume)
{
    music_volume = volume;
    musicStreamSetVolume(volume);
}

bool audioInit(void)
{
    if (audio_ready) {
        return true;
    }
    if (!nitroFSInit(NULL)) {
        return false;
    }
    if (!mmInitDefault("nitro:/soundbank.bin")) {
        return false;
    }

    unsigned int loaded = 0;
    const unsigned int sample_count =
        (unsigned int)(sizeof(physical_samples) / sizeof(physical_samples[0]));
    for (; loaded < sample_count; ++loaded) {
        if (mmLoadEffect(physical_samples[loaded]) != 0) {
            while (loaded > 0) {
                --loaded;
                mmUnloadEffect(physical_samples[loaded]);
            }
            return false;
        }
    }

    current_music = MUSIC_NONE;
    target_music = MUSIC_NONE;
    fade_mode = FADE_NONE;
    fade_frame = 0;
    fade_start_volume = 0;
    music_volume = 0;
    soundEnable();
    audio_ready = true;
    return true;
}

void audioPlaySfx(SfxId id)
{
    if (!audio_ready || (unsigned int)id >= SFX_ID_COUNT) {
        return;
    }
    mmEffect(logical_samples[id]);
}

void audioPlayScratch(CatId cat)
{
    if (cat == CAT_MAODIE) {
        audioPlaySfx(SFX_ID_MAODIE_COMBINED);
    }
    else if (cat == CAT_BANANA) {
        audioPlaySfx(SFX_ID_BANANA_ATTACK);
    }
    else {
        audioPlaySfx((rand() & 1) ? SFX_ID_SCRATCH_1 : SFX_ID_SCRATCH_2);
    }
}

void audioPlayHiss(CatId cat)
{
    audioPlaySfx(cat == CAT_MAODIE ? SFX_ID_MAODIE_COMBINED : SFX_ID_NORMAL_HISS);
}

void audioPlayHeal(CatId cat)
{
    audioPlaySfx(cat == CAT_BANANA ? SFX_ID_BANANA_HEAL : SFX_ID_HEAL);
}

void audioPlayDeath(CatId cat)
{
    audioPlaySfx(cat == CAT_BANANA ? SFX_ID_BANANA_DEATH : SFX_ID_DEATH);
}

void audioSetMusic(MusicId id)
{
    if (!audio_ready || (unsigned int)id >= MUSIC_COUNT || id == target_music) {
        return;
    }

    target_music = id;
    fade_frame = 0;
    fade_start_volume = music_volume;
    fade_mode = current_music == MUSIC_NONE ? FADE_NONE : FADE_OUT;
}

static void beginTargetMusic(void)
{
    if (target_music == MUSIC_NONE) {
        fade_mode = FADE_NONE;
        return;
    }
    if (!musicStreamStart(target_music)) {
        target_music = MUSIC_NONE;
        fade_mode = FADE_NONE;
        return;
    }

    current_music = target_music;
    fade_frame = 0;
    fade_start_volume = 0;
    fade_mode = FADE_IN;
    setMusicVolume(0);
}

void audioUpdate(void)
{
    if (!audio_ready) {
        return;
    }

    if (current_music != MUSIC_NONE) {
        musicStreamUpdate();
    }

    if (current_music == MUSIC_NONE) {
        beginTargetMusic();
        return;
    }

    if (fade_mode == FADE_OUT) {
        ++fade_frame;
        if (fade_frame >= MUSIC_FADE_FRAMES) {
            setMusicVolume(0);
            musicStreamClose();
            current_music = MUSIC_NONE;
            fade_mode = FADE_NONE;
            beginTargetMusic();
        }
        else {
            setMusicVolume(fade_start_volume *
                (MUSIC_FADE_FRAMES - fade_frame) / MUSIC_FADE_FRAMES);
        }
    }
    else if (fade_mode == FADE_IN) {
        ++fade_frame;
        if (fade_frame >= MUSIC_FADE_FRAMES) {
            setMusicVolume(MUSIC_MAX_VOLUME);
            fade_mode = FADE_NONE;
        }
        else {
            setMusicVolume(MUSIC_MAX_VOLUME * fade_frame / MUSIC_FADE_FRAMES);
        }
    }
}

void audioShutdown(void)
{
    if (!audio_ready) {
        return;
    }

    if (current_music != MUSIC_NONE) {
        musicStreamClose();
    }
    mmEffectCancelAll();
    const unsigned int sample_count =
        (unsigned int)(sizeof(physical_samples) / sizeof(physical_samples[0]));
    for (unsigned int i = 0; i < sample_count; ++i) {
        mmUnloadEffect(physical_samples[i]);
    }
    soundDisable();

    audio_ready = false;
    current_music = MUSIC_NONE;
    target_music = MUSIC_NONE;
    fade_mode = FADE_NONE;
    fade_frame = 0;
    fade_start_volume = 0;
    music_volume = 0;
}
