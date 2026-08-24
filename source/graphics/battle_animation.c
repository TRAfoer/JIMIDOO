#include "battle_scene_internal.h"

#include <string.h>

enum {
    ACTION_ANIMATION_FRAMES = 10,
    HIT_ANIMATION_FRAMES = 8,
    DODGE_ANIMATION_FRAMES = 10,
    CAPTION_FRAMES = 12
};

static bool animationSideIsValid(Side side)
{
    return side == SIDE_PLAYER || side == SIDE_AI;
}

static CatAction actionImage(BattleCommand command)
{
    switch (command) {
        case CMD_HISS:
            return CAT_ACTION_HISS;
        case CMD_SCRATCH:
            return CAT_ACTION_SCRATCH;
        case CMD_YOWL:
            return CAT_ACTION_YOWL;
        case CMD_HEAL:
            return CAT_ACTION_HEAL;
        default:
            return CAT_ACTION_IDLE;
    }
}

static void setAnimation(BattleFighterAnimation *fighter, CatAction action,
                         uint8_t duration)
{
    fighter->action = action;
    fighter->frames = duration;
    fighter->duration = duration;
}

void battleAnimationInit(BattleAnimation *animation)
{
    if (animation == NULL) {
        return;
    }
    memset(animation, 0, sizeof(*animation));
    animation->fighter[SIDE_PLAYER].action = CAT_ACTION_IDLE;
    animation->fighter[SIDE_AI].action = CAT_ACTION_IDLE;
}

void battleAnimationOnAction(BattleAnimation *animation, Side side,
                             BattleCommand command)
{
    if (animation == NULL || !animationSideIsValid(side) ||
        command == CMD_NONE) {
        return;
    }
    setAnimation(&animation->fighter[side], actionImage(command),
                 ACTION_ANIMATION_FRAMES);
    if (command == CMD_SCRATCH) {
        animation->fighter[side].afterimage_frames = 6;
    }
}

static void showCaption(BattleAnimation *animation, BattleEventType type,
                        Side side, int amount)
{
    animation->caption = type;
    animation->caption_side = side;
    animation->caption_amount = amount;
    animation->caption_frames = CAPTION_FRAMES;
}

void battleAnimationOnEvents(BattleAnimation *animation,
                             const BattleEvent *events, size_t event_count)
{
    size_t index;

    if (animation == NULL || (events == NULL && event_count != 0u)) {
        return;
    }
    for (index = 0u; index < event_count; ++index) {
        const BattleEvent *event = &events[index];

        if (!animationSideIsValid(event->source) ||
            !animationSideIsValid(event->target)) {
            continue;
        }
        switch (event->type) {
            case EVENT_WARNING:
                animation->warning_frames = (uint8_t)event->amount;
                showCaption(animation, EVENT_WARNING, event->target, 0);
                break;
            case EVENT_HIT:
            case EVENT_STUN:
                setAnimation(&animation->fighter[event->target],
                             CAT_ACTION_HIT, HIT_ANIMATION_FRAMES);
                animation->fighter[event->target].flash_frames = 6;
                break;
            case EVENT_DAMAGE:
                showCaption(animation, EVENT_DAMAGE, event->target,
                            event->amount);
                break;
            case EVENT_HEAL:
                setAnimation(&animation->fighter[event->source],
                             CAT_ACTION_HEAL, ACTION_ANIMATION_FRAMES);
                showCaption(animation, EVENT_HEAL, event->source,
                            event->amount);
                break;
            case EVENT_DODGE:
                animation->fighter[event->target].dodge_direction =
                    event->target == SIDE_PLAYER ? -1 : 1;
                setAnimation(&animation->fighter[event->target],
                             CAT_ACTION_IDLE, DODGE_ANIMATION_FRAMES);
                showCaption(animation, EVENT_DODGE, event->target, 0);
                break;
            case EVENT_HISS_SUCCESS:
                animation->warning_frames = 0u;
                showCaption(animation, EVENT_HISS_SUCCESS, event->source, 0);
                break;
            case EVENT_BATTLE_END:
                animation->fighter[event->target].action = CAT_ACTION_DEAD;
                animation->fighter[event->target].frames = 0;
                animation->fighter[event->target].duration = 0;
                break;
            default:
                break;
        }
    }
}

static void tickFighter(BattleFighterAnimation *fighter)
{
    if (fighter->frames > 0u) {
        --fighter->frames;
        if (fighter->frames == 0u && fighter->action != CAT_ACTION_DEAD) {
            fighter->action = CAT_ACTION_IDLE;
            fighter->duration = 0u;
            fighter->dodge_direction = 0;
        }
    }
    if (fighter->flash_frames > 0u) {
        --fighter->flash_frames;
    }
    if (fighter->afterimage_frames > 0u) {
        --fighter->afterimage_frames;
    }
}

void battleAnimationTick(BattleAnimation *animation, bool paused)
{
    if (animation == NULL || paused) {
        return;
    }
    ++animation->frame;
    tickFighter(&animation->fighter[SIDE_PLAYER]);
    tickFighter(&animation->fighter[SIDE_AI]);
    if (animation->caption_frames > 0u) {
        --animation->caption_frames;
    }
    if (animation->warning_frames > 0u) {
        --animation->warning_frames;
    }
}

#ifdef __NDS__

#include <stdio.h>

#include <nds.h>

#include "font_layout.h"
#include "graphics_service.h"
#include "localization.h"

static int animationProgressOffset(const BattleFighterAnimation *fighter,
                                   Side side)
{
    int elapsed;
    int direction = side == SIDE_PLAYER ? 1 : -1;

    if (fighter->frames == 0u || fighter->duration == 0u) {
        return 0;
    }
    elapsed = fighter->duration - fighter->frames;
    if (fighter->dodge_direction != 0) {
        int triangular = elapsed <= fighter->duration / 2 ? elapsed :
                         fighter->duration - elapsed;
        return fighter->dodge_direction * triangular * 2;
    }
    if (fighter->action == CAT_ACTION_SCRATCH) {
        int triangular = elapsed <= fighter->duration / 2 ? elapsed :
                         fighter->duration - elapsed;
        return direction * triangular * 2;
    }
    if (fighter->action == CAT_ACTION_HIT) {
        return (elapsed & 1) != 0 ? direction * 3 : -direction * 3;
    }
    if (fighter->action == CAT_ACTION_HISS ||
        fighter->action == CAT_ACTION_YOWL) {
        return (elapsed & 1) != 0 ? direction : -direction;
    }
    return 0;
}

static CatAction visibleAction(const BattleFighterAnimation *fighter,
                               const FighterState *state)
{
    if (state->hp <= 0 || fighter->action == CAT_ACTION_DEAD) {
        return CAT_ACTION_DEAD;
    }
    if (fighter->frames != 0u) {
        return fighter->action;
    }
    if (state->channel == CHANNEL_YOWL) {
        return CAT_ACTION_YOWL;
    }
    if (state->channel == CHANNEL_HEAL) {
        return CAT_ACTION_HEAL;
    }
    return CAT_ACTION_IDLE;
}

static unsigned int animationScale(const BattleFighterAnimation *fighter)
{
    int elapsed;
    int triangular;

    if (fighter->frames == 0u || fighter->duration == 0u) {
        return FONT_SCALE_ONE;
    }
    elapsed = fighter->duration - fighter->frames;
    triangular = elapsed <= fighter->duration / 2 ? elapsed :
                 fighter->duration - elapsed;
    if (fighter->action == CAT_ACTION_HISS ||
        fighter->action == CAT_ACTION_YOWL ||
        fighter->action == CAT_ACTION_HEAL) {
        return FONT_SCALE_ONE + (unsigned int)(triangular * 6);
    }
    if (fighter->action == CAT_ACTION_HIT) {
        return FONT_SCALE_ONE - (unsigned int)(triangular * 2);
    }
    return FONT_SCALE_ONE;
}

static void drawFighter(const BattleAnimation *animation,
                        const BattleState *battle, Side side, CatId cat)
{
    const BattleFighterAnimation *fighter = &animation->fighter[side];
    const FighterState *state = &battle->fighter[side];
    CatAction action = visibleAction(fighter, state);
    int base_x = side == SIDE_PLAYER ? 1 : 127;
    int base_y = side == SIDE_PLAYER ? 59 : 29;
    int offset = animationProgressOffset(fighter, side);
    unsigned int scale = animationScale(fighter);
    int scaled_size = 128 * (int)scale / (int)FONT_SCALE_ONE;
    int center_adjustment = (scaled_size - 128) / 2;
    bool flip = side == SIDE_AI;

    if (fighter->flash_frames != 0u &&
        ((fighter->flash_frames + animation->frame) & 1u) != 0u) {
        graphicsTopFillRect(base_x + offset + 8, base_y + 8, 112, 112,
                            RGB15(31, 28, 25));
    }
    if (fighter->afterimage_frames != 0u) {
        int trail = side == SIDE_PLAYER ? -7 : 7;
        catTextureDrawScaled(cat, action,
                             base_x + offset + trail - center_adjustment,
                             base_y - center_adjustment, flip, scale);
    }
    catTextureDrawScaled(cat, action, base_x + offset - center_adjustment,
                         base_y - center_adjustment, flip, scale);
}

static void drawParticles(const BattleAnimation *animation)
{
    unsigned int index;

    if (animation->caption_frames == 0u ||
        (animation->caption != EVENT_DAMAGE &&
         animation->caption != EVENT_HEAL)) {
        return;
    }
    for (index = 0u; index < 5u; ++index) {
        int direction = animation->caption_side == SIDE_PLAYER ? -1 : 1;
        int center = animation->caption_side == SIDE_PLAYER ? 92 : 164;
        int age = CAPTION_FRAMES - animation->caption_frames;
        int x = center + direction * (int)(index * 4u + (unsigned int)age);
        int y = 82 - age - (int)((index * 3u) & 7u);
        uint16_t color = animation->caption == EVENT_HEAL ?
                         RGB15(8, 31, 13) : RGB15(31, 9, 4);

        graphicsTopFillRect(x, y, 3, 3, color);
    }
}

static void drawCaption(const BattleAnimation *animation)
{
    char amount[16];
    const char *caption = NULL;
    uint16_t color = RGB15(31, 26, 8);

    if (animation->caption_frames == 0u) {
        return;
    }
    switch (animation->caption) {
        case EVENT_DAMAGE:
            snprintf(amount, sizeof(amount), "-%d", animation->caption_amount);
            caption = amount;
            color = RGB15(31, 8, 4);
            break;
        case EVENT_HEAL:
            snprintf(amount, sizeof(amount), "+%d", animation->caption_amount);
            caption = amount;
            color = RGB15(8, 31, 13);
            break;
        case EVENT_DODGE:
            caption = textGet(TEXT_STATUS_DODGED);
            break;
        case EVENT_HISS_SUCCESS:
            caption = textGet(TEXT_STATUS_COUNTERED);
            break;
        case EVENT_WARNING:
            caption = textGet(TEXT_STATUS_WARNING);
            break;
        default:
            break;
    }
    if (caption != NULL) {
        int x = (256 - fontTextWidth(caption, FONT_SCALE_HALF)) / 2;
        graphicsTextDrawTop(x, 45, FONT_SCALE_HALF, color, caption);
    }
}

void battleAnimationDraw(const BattleAnimation *animation,
                         const BattleState *battle, CatId player_cat,
                         CatId enemy_cat)
{
    const char *player_name;
    const char *enemy_name;
    uint16_t cream = RGB15(31, 29, 23);
    uint16_t orange = RGB15(31, 15, 4);

    if (animation == NULL || battle == NULL) {
        return;
    }
    player_name = textGet(TEXT_CAT_NAME(player_cat));
    enemy_name = textGet(TEXT_CAT_NAME(enemy_cat));

    graphicsFrameBegin();
    graphicsTopFillGradient(RGB15(8, 3, 12), RGB15(3, 2, 7),
                            RGB15(14, 5, 2), RGB15(23, 8, 3));
    graphicsTopFillRect(0, 0, 256, 27, RGB15(4, 3, 8));
    graphicsTextDrawTop(5, 6, FONT_SCALE_HALF, cream, player_name);
    graphicsTextDrawTop(251 - fontTextWidth(enemy_name, FONT_SCALE_HALF), 6,
                        FONT_SCALE_HALF, cream, enemy_name);
    if (animation->warning_frames != 0u &&
        ((animation->frame / 4u) & 1u) == 0u) {
        const char *warning = textGet(TEXT_STATUS_WARNING);
        int warning_x = (256 - fontTextWidth(warning, FONT_SCALE_HALF)) / 2;

        int icon_x = warning_x - 13;

        graphicsTopFillRect(icon_x - 4, 27,
                            fontTextWidth(warning, FONT_SCALE_HALF) + 22, 16,
                            RGB15(15, 3, 2));
        graphicsTopFillRect(icon_x, 29, 3, 8, orange);
        graphicsTopFillRect(icon_x, 39, 3, 3, orange);
        graphicsTextDrawTop(warning_x, 30, FONT_SCALE_HALF, orange, warning);
    }
    drawFighter(animation, battle, SIDE_PLAYER, player_cat);
    drawFighter(animation, battle, SIDE_AI, enemy_cat);
    drawParticles(animation);
    drawCaption(animation);
    if (battle->paused) {
        const char *paused = textGet(TEXT_STATUS_PAUSED);
        int paused_x = (256 - fontTextWidth(paused, FONT_SCALE_ONE)) / 2;

        graphicsTopFillRect(54, 75, 148, 43, RGB15(4, 3, 8));
        graphicsTextDrawTop(paused_x, 85, FONT_SCALE_ONE, cream, paused);
    }
    graphicsFrameEnd();
}

#else

void battleAnimationDraw(const BattleAnimation *animation,
                         const BattleState *battle, CatId player_cat,
                         CatId enemy_cat)
{
    (void)animation;
    (void)battle;
    (void)player_cat;
    (void)enemy_cat;
}

#endif
