#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <nds.h>

#include "font_layout.h"
#include "graphics_service.h"
#include "localization.h"
#include "battle_scene_internal.h"

static char top_text[16][96];
static unsigned int top_text_count;

int fontTextWidth(const char *text, unsigned int scale)
{
    (void)scale;
    return (int)strlen(text) * 4;
}

const char *textGet(GameTextId id)
{
    switch (id) {
        case TEXT_CAT_ORANGE: return "Orange Cat";
        case TEXT_CAT_TABBY: return "Tabby Cat";
        case TEXT_STATUS_PAUSED: return "Paused";
        case TEXT_STATUS_DODGED: return "Dodged";
        case TEXT_STATUS_COUNTERED: return "Countered";
        case TEXT_STATUS_WARNING: return "Warning";
        case TEXT_DEBUG_AI_PROFILE: return "Profile";
        case TEXT_DEBUG_AI_MEMORY: return "Memory";
        case TEXT_DEBUG_AI_OBSERVATION: return "Observation";
        case TEXT_AI_PROFILE_AGGRESSIVE: return "Aggressive";
        case TEXT_AI_PROFILE_COUNTER: return "Counter";
        case TEXT_AI_PROFILE_RAGE: return "Rage";
        case TEXT_AI_PROFILE_SURVIVAL: return "Survival";
        case TEXT_AI_PROFILE_OPPORTUNIST: return "Opportunist";
        case TEXT_AI_PROFILE_TRICKSTER: return "Trickster";
        default: return "?";
    }
}

void graphicsFrameBegin(void) {}
void graphicsFrameEnd(void) {}
void graphicsTopFillRect(int x, int y, int width, int height, uint16_t color)
{ (void)x; (void)y; (void)width; (void)height; (void)color; }
void graphicsTextDrawTop(int x, int y, unsigned int scale, uint16_t color,
                         const char *text)
{
    (void)x; (void)y; (void)scale; (void)color;
    assert(top_text_count < 16u);
    strcpy_s(top_text[top_text_count++], 96u, text);
}
void battleBackgroundDraw(void) {}
void catTextureDrawScaled(CatId cat, CatAction action, int x, int y,
                          bool horizontal_flip, unsigned int scale)
{
    (void)cat; (void)action; (void)x; (void)y;
    (void)horizontal_flip; (void)scale;
}

static bool contains_text(const char *wanted)
{
    unsigned int index;

    for (index = 0u; index < top_text_count; ++index) {
        if (strcmp(top_text[index], wanted) == 0) {
            return true;
        }
    }
    return false;
}

static BattleState draw_battle_fixture(void)
{
    static const FighterSpec player = {
        60, 15, 5, 15, 100, 120, 0, 0, 0
    };
    static const FighterSpec enemy = {
        65, 16, 5, 12, 100, 120, 0, 0, 0
    };
    BattleState battle;

    battleInit(&battle, &player, &enemy, UINT32_C(0x13579BDF));
    return battle;
}

static void test_null_debug_snapshot_draws_no_telemetry(void)
{
    BattleAnimation animation;
    BattleState battle = draw_battle_fixture();

    battleAnimationInit(&animation);
    top_text_count = 0u;
    battleAnimationDraw(&animation, &battle, CAT_ORANGE, CAT_TABBY, NULL);
    assert(top_text_count == 2u);
    assert(!contains_text("Profile"));
    assert(!contains_text("Memory"));
    assert(!contains_text("Observation"));
    assert(!contains_text("Counter"));
}

static void test_debug_snapshot_draws_profile_timing_tickets_and_history(void)
{
    BattleAnimation animation;
    BattleState battle = draw_battle_fixture();
    AiDebugSnapshot snapshot = {
        AI_PROFILE_COUNTER,
        { CMD_HISS, CMD_SCRATCH, CMD_YOWL, CMD_HEAL },
        4u,
        0u,
        12u,
        { 0u, 1111u, 2222u, 3333u, 4444u }
    };

    battleAnimationInit(&animation);
    top_text_count = 0u;
    battleAnimationDraw(&animation, &battle, CAT_ORANGE, CAT_TABBY,
                        &snapshot);
    assert(contains_text("Profile: Counter  Observation: 12"));
    assert(contains_text("L1111 R2222 Y3333 A4444"));
    assert(contains_text("Memory: L R Y A"));
}

static void test_successful_hiss_clears_active_warning(void)
{
    BattleAnimation animation;
    BattleEvent warning = {
        EVENT_WARNING, SIDE_AI, SIDE_PLAYER, BATTLE_WARNING_FRAMES
    };
    BattleEvent success = {
        EVENT_HISS_SUCCESS, SIDE_PLAYER, SIDE_AI, 0
    };

    battleAnimationInit(&animation);
    battleAnimationOnEvents(&animation, &warning, 1);
    assert(animation.warning_frames == BATTLE_WARNING_FRAMES);
    battleAnimationOnEvents(&animation, &success, 1);
    assert(animation.warning_frames == 0u);
}

static void test_failed_hiss_keeps_active_warning(void)
{
    BattleAnimation animation;
    BattleEvent warning = {
        EVENT_WARNING, SIDE_AI, SIDE_PLAYER, BATTLE_WARNING_FRAMES
    };
    BattleEvent fail = {
        EVENT_HISS_FAIL, SIDE_PLAYER, SIDE_AI, 0
    };

    battleAnimationInit(&animation);
    battleAnimationOnEvents(&animation, &warning, 1);
    battleAnimationOnEvents(&animation, &fail, 1);
    assert(animation.warning_frames == BATTLE_WARNING_FRAMES);
}

static void test_hit_flash_blinks_the_sprite_without_an_opaque_backdrop(void)
{
    assert(battleAnimationFighterVisible(0u, 0u));
    assert(!battleAnimationFighterVisible(6u, 1u));
    assert(battleAnimationFighterVisible(6u, 0u));
}

static void test_only_the_enemy_sprite_is_horizontally_flipped(void)
{
    assert(!battleAnimationHorizontalFlip(SIDE_PLAYER));
    assert(battleAnimationHorizontalFlip(SIDE_AI));
}

static void test_action_pose_covers_cooldown_without_stretching_motion(void)
{
    BattleAnimation animation;
    unsigned int frame;

    battleAnimationInit(&animation);
    battleAnimationOnAction(&animation, SIDE_PLAYER, CMD_SCRATCH, 120u);
    assert(animation.fighter[SIDE_PLAYER].action == CAT_ACTION_SCRATCH);
    assert(animation.fighter[SIDE_PLAYER].pose_frames == 120u);
    assert(animation.fighter[SIDE_PLAYER].effect_frames == 10u);

    for (frame = 0u; frame < 10u; ++frame) {
        battleAnimationTick(&animation, false);
    }
    assert(animation.fighter[SIDE_PLAYER].pose_frames == 110u);
    assert(animation.fighter[SIDE_PLAYER].effect_frames == 0u);
    assert(animation.fighter[SIDE_PLAYER].action == CAT_ACTION_SCRATCH);

    for (frame = 0u; frame < 110u; ++frame) {
        battleAnimationTick(&animation, false);
    }
    assert(animation.fighter[SIDE_PLAYER].pose_frames == 0u);
    assert(animation.fighter[SIDE_PLAYER].action == CAT_ACTION_IDLE);
}

static void test_zero_cooldown_hiss_keeps_the_minimum_action_pose(void)
{
    BattleAnimation animation;

    battleAnimationInit(&animation);
    battleAnimationOnAction(&animation, SIDE_AI, CMD_HISS, 0u);
    assert(animation.fighter[SIDE_AI].pose_frames == 10u);
    assert(animation.fighter[SIDE_AI].effect_frames == 10u);
}

static void test_hit_feedback_overrides_a_long_action_pose(void)
{
    BattleAnimation animation;
    BattleEvent hit = { EVENT_HIT, SIDE_AI, SIDE_PLAYER, 15 };

    battleAnimationInit(&animation);
    battleAnimationOnAction(&animation, SIDE_PLAYER, CMD_HEAL, 120u);
    battleAnimationOnEvents(&animation, &hit, 1u);
    assert(animation.fighter[SIDE_PLAYER].action == CAT_ACTION_HIT);
    assert(animation.fighter[SIDE_PLAYER].pose_frames == 8u);
    assert(animation.fighter[SIDE_PLAYER].effect_frames == 8u);
}

int main(void)
{
    test_null_debug_snapshot_draws_no_telemetry();
    test_debug_snapshot_draws_profile_timing_tickets_and_history();
    test_successful_hiss_clears_active_warning();
    test_failed_hiss_keeps_active_warning();
    test_hit_flash_blinks_the_sprite_without_an_opaque_backdrop();
    test_only_the_enemy_sprite_is_horizontally_flipped();
    test_action_pose_covers_cooldown_without_stretching_motion();
    test_zero_cooldown_hiss_keeps_the_minimum_action_pose();
    test_hit_feedback_overrides_a_long_action_pose();
    return 0;
}
