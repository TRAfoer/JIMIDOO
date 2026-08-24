#include "battle_scene_internal.h"

#include <string.h>

unsigned int battleHudCooldownTenths(uint32_t frames)
{
    return (unsigned int)((frames + 5u) / 6u);
}

unsigned int battleHudDirtyRegions(const BattleHudSnapshot *shown,
                                   const BattleHudSnapshot *next,
                                   bool status_valid, bool buttons_valid)
{
    unsigned int dirty = BATTLE_HUD_DIRTY_NONE;
    bool shown_stunned;
    bool next_stunned;

    if (shown == NULL || next == NULL) {
        return BATTLE_HUD_DIRTY_STATUS | BATTLE_HUD_DIRTY_BUTTONS;
    }
    shown_stunned = shown->stun != 0u;
    next_stunned = next->stun != 0u;

    if (!status_valid || shown->hp[SIDE_PLAYER] != next->hp[SIDE_PLAYER] ||
        shown->hp[SIDE_AI] != next->hp[SIDE_AI] ||
        shown->rage[SIDE_PLAYER] != next->rage[SIDE_PLAYER] ||
        shown->rage[SIDE_AI] != next->rage[SIDE_AI] ||
        shown_stunned != next_stunned || shown->paused != next->paused) {
        dirty |= BATTLE_HUD_DIRTY_STATUS;
    }
    if (!buttons_valid ||
        shown->cooldown_tenths != next->cooldown_tenths ||
        shown_stunned != next_stunned || shown->channel != next->channel ||
        shown->paused != next->paused ||
        shown->available != next->available) {
        dirty |= BATTLE_HUD_DIRTY_BUTTONS;
    }
    return dirty;
}

#ifdef __NDS__

#include <stdio.h>

#include <nds.h>

#include "font_layout.h"
#include "graphics_service.h"
#include "localization.h"

enum {
    HUD_SCREEN_WIDTH = 256,
    HUD_STATUS_HEIGHT = 64,
    HUD_BUTTON_WIDTH = 128,
    HUD_BUTTON_HEIGHT = 64
};

static BattleHudSnapshot hudSnapshot(const BattleState *battle)
{
    BattleHudSnapshot snapshot;
    const FighterState *player = &battle->fighter[SIDE_PLAYER];

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.hp[SIDE_PLAYER] = player->hp;
    snapshot.hp[SIDE_AI] = battle->fighter[SIDE_AI].hp;
    snapshot.rage[SIDE_PLAYER] = player->rage;
    snapshot.rage[SIDE_AI] = battle->fighter[SIDE_AI].rage;
    snapshot.cooldown_tenths = battleHudCooldownTenths(player->cooldown);
    snapshot.stun = player->stun;
    snapshot.channel = player->channel;
    snapshot.paused = battle->paused;
    snapshot.available = !battle->finished && !battle->paused &&
                         player->cooldown == 0u && player->stun == 0u;
    return snapshot;
}

static int centeredSubTextX(const char *text, unsigned int scale,
                            int x, int width)
{
    return x + (width - fontTextWidth(text, scale)) / 2;
}

static void drawBar(int x, int y, int width, int value, int maximum,
                    uint16_t fill)
{
    graphicsSubFillRect(x, y, width, 5, RGB15(3, 3, 5));
    if (maximum > 0 && value > 0) {
        int filled = width * value / maximum;

        if (filled > width) {
            filled = width;
        }
        graphicsSubFillRect(x, y, filled, 5, fill);
    }
}

static void drawStatus(const BattleState *battle,
                       const BattleHudSnapshot *snapshot)
{
    char line[48];
    uint16_t cream = RGB15(31, 29, 23);
    uint16_t rage = RGB15(31, 17, 3);
    uint16_t hp = RGB15(7, 27, 10);

    graphicsSubFillRect(0, 0, HUD_SCREEN_WIDTH, HUD_STATUS_HEIGHT,
                        RGB15(5, 3, 9));

    snprintf(line, sizeof(line), "%s %d/%d", textGet(TEXT_STATUS_HP),
             snapshot->hp[SIDE_PLAYER], battle->spec[SIDE_PLAYER].max_hp);
    graphicsTextDrawSub(5, 4, FONT_SCALE_HALF, cream, line);
    drawBar(5, 18, 112, snapshot->hp[SIDE_PLAYER],
            battle->spec[SIDE_PLAYER].max_hp, hp);
    snprintf(line, sizeof(line), "%s %d/%d", textGet(TEXT_STATUS_RAGE),
             snapshot->rage[SIDE_PLAYER], battle->spec[SIDE_PLAYER].rage_cap);
    graphicsTextDrawSub(5, 27, FONT_SCALE_HALF, cream, line);
    drawBar(5, 42, 112, snapshot->rage[SIDE_PLAYER],
            battle->spec[SIDE_PLAYER].rage_cap, rage);

    snprintf(line, sizeof(line), "%s %d/%d", textGet(TEXT_STATUS_HP),
             snapshot->hp[SIDE_AI], battle->spec[SIDE_AI].max_hp);
    graphicsTextDrawSub(133, 4, FONT_SCALE_HALF, cream, line);
    drawBar(133, 18, 118, snapshot->hp[SIDE_AI],
            battle->spec[SIDE_AI].max_hp, hp);
    snprintf(line, sizeof(line), "%s %d/%d", textGet(TEXT_STATUS_RAGE),
             snapshot->rage[SIDE_AI], battle->spec[SIDE_AI].rage_cap);
    graphicsTextDrawSub(133, 27, FONT_SCALE_HALF, cream, line);
    drawBar(133, 42, 118, snapshot->rage[SIDE_AI],
            battle->spec[SIDE_AI].rage_cap, rage);

    if (snapshot->paused || snapshot->stun != 0u) {
        const char *status = textGet(snapshot->paused ? TEXT_STATUS_PAUSED :
                                    TEXT_STATUS_STUNNED);
        graphicsSubFillRect(72, 48, 112, 15, RGB15(13, 5, 4));
        graphicsTextDrawSub(centeredSubTextX(status, FONT_SCALE_HALF, 72, 112),
                            50, FONT_SCALE_HALF, cream, status);
    }
}

static Channel commandChannel(BattleCommand command)
{
    if (command == CMD_YOWL) {
        return CHANNEL_YOWL;
    }
    if (command == CMD_HEAL) {
        return CHANNEL_HEAL;
    }
    return CHANNEL_NONE;
}

static void drawButton(BattleCommand command, int x, int y,
                       const char *key_name, GameTextId label_id,
                       const BattleHudSnapshot *snapshot)
{
    char cooldown[48];
    const char *label = textGet(label_id);
    bool active = commandChannel(command) != CHANNEL_NONE &&
                  commandChannel(command) == snapshot->channel;
    uint16_t background = snapshot->available ? RGB15(13, 8, 19) :
                                                RGB15(5, 4, 7);
    uint16_t accent = active ? RGB15(9, 25, 24) : RGB15(31, 16, 4);
    uint16_t text = snapshot->available ? RGB15(31, 29, 23) :
                                          RGB15(14, 13, 13);

    graphicsSubFillRect(x, y, HUD_BUTTON_WIDTH, HUD_BUTTON_HEIGHT, background);
    graphicsSubFillRect(x, y, HUD_BUTTON_WIDTH, 2, accent);
    graphicsSubFillRect(x, y, 2, HUD_BUTTON_HEIGHT, accent);
    graphicsTextDrawSub(x + 6, y + 7, FONT_SCALE_HALF, accent, key_name);
    graphicsTextDrawSub(centeredSubTextX(label, FONT_SCALE_HALF, x,
                                         HUD_BUTTON_WIDTH),
                        y + 22, FONT_SCALE_HALF, text, label);
    if (snapshot->stun != 0u) {
        const char *stunned = textGet(TEXT_STATUS_STUNNED);
        graphicsTextDrawSub(centeredSubTextX(stunned, FONT_SCALE_HALF, x,
                                             HUD_BUTTON_WIDTH),
                            y + 43, FONT_SCALE_HALF, accent, stunned);
    } else if (snapshot->cooldown_tenths != 0u) {
        snprintf(cooldown, sizeof(cooldown), "%s %u.%u",
                 textGet(TEXT_STATUS_COOLDOWN),
                 snapshot->cooldown_tenths / 10u,
                 snapshot->cooldown_tenths % 10u);
        graphicsTextDrawSub(centeredSubTextX(cooldown, FONT_SCALE_HALF, x,
                                             HUD_BUTTON_WIDTH),
                            y + 43, FONT_SCALE_HALF, accent, cooldown);
    }
}

void battleHudInit(BattleHud *hud)
{
    if (hud != NULL) {
        memset(hud, 0, sizeof(*hud));
    }
}

void battleHudDraw(BattleHud *hud, const BattleState *battle)
{
    BattleHudSnapshot snapshot;
    unsigned int dirty;

    if (hud == NULL || battle == NULL) {
        return;
    }
    snapshot = hudSnapshot(battle);
    dirty = battleHudDirtyRegions(&hud->shown, &snapshot, hud->status_valid,
                                  hud->buttons_valid);

    if (!hud->base_drawn) {
        graphicsSubClear(RGB15(5, 3, 9));
        hud->base_drawn = true;
        dirty = BATTLE_HUD_DIRTY_STATUS | BATTLE_HUD_DIRTY_BUTTONS;
    }
    if ((dirty & BATTLE_HUD_DIRTY_STATUS) != 0u) {
        drawStatus(battle, &snapshot);
        hud->status_valid = true;
    }
    if ((dirty & BATTLE_HUD_DIRTY_BUTTONS) != 0u) {
        drawButton(CMD_HISS, 0, 64, "L", TEXT_ACTION_HISS, &snapshot);
        drawButton(CMD_SCRATCH, 128, 64, "R", TEXT_ACTION_SCRATCH, &snapshot);
        drawButton(CMD_YOWL, 0, 128, "Y", TEXT_ACTION_LAOWU_YOWL, &snapshot);
        drawButton(CMD_HEAL, 128, 128, "A", TEXT_ACTION_CHEW_GUM, &snapshot);
        hud->buttons_valid = true;
    }
    hud->shown = snapshot;
}

#else

void battleHudInit(BattleHud *hud)
{
    if (hud != NULL) {
        memset(hud, 0, sizeof(*hud));
    }
}

void battleHudDraw(BattleHud *hud, const BattleState *battle)
{
    (void)hud;
    (void)battle;
}

#endif
