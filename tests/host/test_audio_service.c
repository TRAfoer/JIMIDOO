#include "audio_service.h"

#include <assert.h>
#include <stdbool.h>

#include "soundbank.h"

static bool nitro_ok;
static bool maxmod_ok;
static int load_failure = -1;
static int last_effect = -1;
static int effect_calls;
static int stream_updates;
static int stream_starts;
static int stream_closes;
static int last_music = MUSIC_NONE;
static unsigned int stream_volume;
static int sound_enable_calls;
static int gameplay_random_calls;
static int maxmod_init_calls;
static int unload_calls;
static int cancel_calls;
static int sound_disable_calls;

int testRand(void)
{
    ++gameplay_random_calls;
    return 1;
}

bool nitroFSInit(const char *path)
{
    (void)path;
    return nitro_ok;
}

bool mmInitDefault(const char *path)
{
    (void)path;
    ++maxmod_init_calls;
    return maxmod_ok;
}

unsigned int mmLoadEffect(unsigned int sample_id)
{
    return (int)sample_id == load_failure ? 1U : 0U;
}

unsigned int mmUnloadEffect(unsigned int sample_id)
{
    (void)sample_id;
    ++unload_calls;
    return 0;
}

unsigned int mmEffect(unsigned int sample_id)
{
    last_effect = (int)sample_id;
    ++effect_calls;
    return 1;
}

void mmEffectCancelAll(void) { ++cancel_calls; }
void soundDisable(void) { ++sound_disable_calls; }
void soundEnable(void) { ++sound_enable_calls; }

void musicStreamUpdate(void)
{
    ++stream_updates;
}

void musicStreamClose(void) { ++stream_closes; }
void musicStreamSetVolume(unsigned int volume) { stream_volume = volume; }
bool musicStreamStart(MusicId id)
{
    last_music = id;
    ++stream_starts;
    return true;
}

static void expect_effect(int expected)
{
    assert(last_effect == expected);
    last_effect = -1;
}

int main(void)
{
    nitro_ok = false;
    maxmod_ok = true;
    assert(!audioInit());
    audioPlaySfx(SFX_ID_START);
    audioPlayScratch(CAT_ORANGE);
    audioPlayHiss(CAT_MAODIE);
    audioPlayHeal(CAT_BANANA);
    audioPlayDeath(CAT_BANANA);
    audioSetMusic(MUSIC_MENU);
    audioUpdate();
    audioShutdown();
    assert(effect_calls == 0);
    assert(stream_updates == 0);
    assert(gameplay_random_calls == 0);

    nitro_ok = true;
    maxmod_ok = false;
    assert(!audioInit());
    audioPlaySfx(SFX_ID_START);
    assert(effect_calls == 0);
    assert(maxmod_init_calls == 1);

    maxmod_ok = true;
    load_failure = SFX_SCRATCH_2;
    assert(!audioInit());
    audioPlaySfx(SFX_ID_START);
    assert(effect_calls == 0);
    assert(maxmod_init_calls == 2);
    assert(unload_calls == 3);
    audioShutdown();
    assert(cancel_calls == 1);
    assert(sound_disable_calls == 1);
    audioShutdown();
    assert(cancel_calls == 1);
    assert(sound_disable_calls == 1);

    load_failure = -1;
    assert(audioInit());
    assert(maxmod_init_calls == 2);
    assert(sound_enable_calls == 1);

    audioPlaySfx(SFX_ID_NORMAL_HISS);
    expect_effect(SFX_YOWL);

    audioPlayScratch(CAT_MAODIE);
    expect_effect(SFX_MAODIE_COMBINED);
    audioPlayScratch(CAT_BANANA);
    expect_effect(SFX_BANANA_ATTACK);
    audioPlayScratch(CAT_TABBY);
    assert(last_effect == SFX_SCRATCH_1 || last_effect == SFX_SCRATCH_2);
    last_effect = -1;
    assert(gameplay_random_calls == 0);

    audioPlayHiss(CAT_MAODIE);
    expect_effect(SFX_MAODIE_COMBINED);
    audioPlayHiss(CAT_ORANGE);
    expect_effect(SFX_YOWL);

    audioPlayHeal(CAT_BANANA);
    expect_effect(SFX_BANANA_HEAL);
    audioPlayHeal(CAT_CHOUJU);
    expect_effect(SFX_HEAL);

    audioPlayDeath(CAT_BANANA);
    expect_effect(SFX_BANANA_DEATH);
    audioPlayDeath(CAT_ORANGE);
    expect_effect(SFX_DEATH);

    audioPlaySfx((SfxId)-1);
    assert(last_effect == -1);
    audioPlaySfx(SFX_ID_COUNT);
    assert(last_effect == -1);

    audioSetMusic(MUSIC_MENU);
    audioUpdate();
    assert(stream_starts == 1);
    assert(last_music == MUSIC_MENU);
    assert(stream_volume == 0);
    for (int frame = 0; frame < 30; ++frame) {
        audioUpdate();
    }
    assert(stream_volume == 127);

    audioSetMusic(MUSIC_BATTLE);
    for (int frame = 0; frame < 29; ++frame) {
        audioUpdate();
        assert(stream_starts == 1);
    }
    audioUpdate();
    assert(stream_closes == 1);
    assert(stream_starts == 2);
    assert(last_music == MUSIC_BATTLE);
    assert(stream_volume == 0);
    for (int frame = 0; frame < 30; ++frame) {
        audioUpdate();
    }
    assert(stream_volume == 127);

    audioShutdown();
    assert(unload_calls == 13);
    assert(cancel_calls == 2);
    assert(sound_disable_calls == 2);
    audioPlaySfx(SFX_ID_START);
    assert(last_effect == -1);

    assert(audioInit());
    assert(maxmod_init_calls == 2);
    assert(sound_enable_calls == 2);
    audioShutdown();
    assert(unload_calls == 23);
    assert(sound_disable_calls == 3);
    return 0;
}
