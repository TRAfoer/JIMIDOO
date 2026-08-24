#ifndef GRAPHICS_SERVICE_H
#define GRAPHICS_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "font_layout.h"
#include "game_config.h"

typedef enum CatAction {
    CAT_ACTION_YOWL,
    CAT_ACTION_HISS,
    CAT_ACTION_SCRATCH,
    CAT_ACTION_HIT,
    CAT_ACTION_HEAL,
    CAT_ACTION_DEAD,
    CAT_ACTION_IDLE,
    CAT_ACTION_COUNT
} CatAction;

#define CAT_TEXTURE_COUNT (CAT_COUNT * CAT_ACTION_COUNT)
#define CAT_TEXTURE_IMAGE_BYTES (128U * 128U)
#define CAT_TEXTURE_CACHE_CAT_LIMIT 2U
#define GRAPHICS_FONT_TEXTURE_MAX_BYTES (128U * 1024U)
#define GRAPHICS_TEXTURE_VRAM_BYTES (3U * 128U * 1024U)
#define GRAPHICS_TEXTURE_MAX_RESIDENT_BYTES \
    (GRAPHICS_FONT_TEXTURE_MAX_BYTES + \
     CAT_TEXTURE_CACHE_CAT_LIMIT * CAT_ACTION_COUNT * CAT_TEXTURE_IMAGE_BYTES)

static inline int catTextureIndex(CatId cat, CatAction action)
{
    if ((unsigned int)cat >= CAT_COUNT ||
        (unsigned int)action >= CAT_ACTION_COUNT) {
        return CAT_TEXTURE_COUNT;
    }

    return cat * CAT_ACTION_COUNT + action;
}

bool graphicsInit(void);
void graphicsFrameBegin(void);
void graphicsFrameEnd(void);
void graphicsTopFillGradient(uint16_t top_left, uint16_t bottom_left,
                             uint16_t bottom_right, uint16_t top_right);
void graphicsTopFillRect(int x, int y, int width, int height, uint16_t color);
void graphicsSubClear(uint16_t color);
void graphicsSubFillRect(int x, int y, int width, int height, uint16_t color);
void graphicsTextDrawTop(int x, int y, unsigned int scale, uint16_t color,
                         const char *text);
void graphicsTextDrawSub(int x, int y, unsigned int scale, uint16_t color,
                         const char *text);

bool catTextureLoad(CatId cat, CatAction action);
bool catTexturesLoad(CatId cat);
void catTexturesReset(void);
void catTextureDraw(CatId cat, CatAction action, int x, int y,
                    bool horizontal_flip);
void catTextureDrawScaled(CatId cat, CatAction action, int x, int y,
                          bool horizontal_flip, unsigned int scale);

#endif
