#include <nds.h>

#include "audio_service.h"
#include "battle_scene.h"
#include "game_config.h"
#include "graphics_service.h"
#include "ui_scene.h"

static void runSafeLoop(bool audio_available) __attribute__((noreturn));

static void runSafeLoop(bool audio_available)
{
    while (1) {
        swiWaitForVBlank();
        if (audio_available) {
            audioUpdate();
        }
    }
}

static FighterSpec fighterSpecForCat(CatId cat)
{
    const CatBaseStats *base = configCatBase(cat);
    FighterSpec fighter = {
        base->max_hp,
        base->attack,
        base->rage_per_tick,
        base->heal_per_tick,
        base->rage_cap,
        (uint32_t)base->action_cd_frames,
        10,
        30,
        40
    };

    return fighter;
}

static BattleSetup debugBattleSetup(void)
{
    BattleSetup setup;

    setup.player_cat = CAT_ORANGE;
    setup.enemy_cat = CAT_TABBY;
    setup.player = fighterSpecForCat(setup.player_cat);
    setup.enemy = fighterSpecForCat(setup.enemy_cat);
    setup.crisis = 1u;
    setup.seed = UINT32_C(0x4A694D69);
    return setup;
}

int main(void)
{
    if (!graphicsInit()) {
        runSafeLoop(false);
    }

    bool audio_available = audioInit();
    TitleSceneInitStatus title_status = titleSceneInit(audio_available);
    if (!titleSceneCanRun(title_status)) {
        runSafeLoop(audio_available);
    }

    if (audio_available) {
        audioSetMusic(MUSIC_MENU);
    }

    while (1) {
        uint32_t keys_down;

        swiWaitForVBlank();
        scanKeys();
        keys_down = keysDown();
        if ((keys_down & KEY_START) != 0u) {
            BattleSetup setup = debugBattleSetup();

            audioPlaySfx(SFX_ID_START);
            (void)battleSceneRun(&setup);
            title_status = titleSceneInit(audio_available);
            if (!titleSceneCanRun(title_status)) {
                runSafeLoop(audio_available);
            }
            if (audio_available) {
                audioSetMusic(MUSIC_MENU);
            }
            continue;
        }
        titleSceneUpdate(keys_down);
        audioUpdate();
        titleSceneDraw();
    }
}
