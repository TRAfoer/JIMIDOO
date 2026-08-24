#include "graphics_service.h"

#include <stddef.h>
#include <stdio.h>

#include <gl2d.h>

#define CAT_TEXTURE_WIDTH 128
#define CAT_TEXTURE_HEIGHT 128
#define CAT_IMAGE_BYTES (CAT_TEXTURE_WIDTH * CAT_TEXTURE_HEIGHT)
#define CAT_PALETTE_ENTRIES 256

_Static_assert(CAT_IMAGE_BYTES == CAT_TEXTURE_IMAGE_BYTES,
               "cat texture payload and VRAM budget disagree");

static const char *const cat_resource_names[CAT_COUNT] = {
    [CAT_ORANGE] = "orange",
    [CAT_TABBY] = "tabby",
    [CAT_MAODIE] = "maodie",
    [CAT_CHOUJU] = "chouju",
    [CAT_BANANA] = "banana"
};

static const char *const action_resource_names[CAT_ACTION_COUNT] = {
    [CAT_ACTION_YOWL] = "yowl",
    [CAT_ACTION_HISS] = "hiss",
    [CAT_ACTION_SCRATCH] = "scratch",
    [CAT_ACTION_HIT] = "hit",
    [CAT_ACTION_HEAL] = "heal",
    [CAT_ACTION_DEAD] = "dead",
    [CAT_ACTION_IDLE] = "idle"
};

static glImage cat_images[CAT_TEXTURE_COUNT];
static bool cat_loaded[CAT_TEXTURE_COUNT];
static uint8_t cat_image_buffer[CAT_IMAGE_BYTES] __attribute__((aligned(4)));
static uint16_t cat_palette_buffer[CAT_PALETTE_ENTRIES]
    __attribute__((aligned(4)));

typedef struct CatCacheSlot {
    CatId cat;
    unsigned int last_use;
    bool occupied;
} CatCacheSlot;

static CatCacheSlot cat_cache[CAT_TEXTURE_CACHE_CAT_LIMIT];
static unsigned int cat_cache_use;

static bool readExactFile(const char *path, void *destination, size_t size)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }
    bool valid = fread(destination, 1, size, file) == size && fgetc(file) == EOF;
    fclose(file);
    return valid;
}

static CatCacheSlot *findCatSlot(CatId cat)
{
    for (unsigned int index = 0; index < CAT_TEXTURE_CACHE_CAT_LIMIT; ++index) {
        if (cat_cache[index].occupied && cat_cache[index].cat == cat) {
            return &cat_cache[index];
        }
    }
    return NULL;
}

static void touchCatSlot(CatCacheSlot *slot)
{
    ++cat_cache_use;
    if (cat_cache_use == 0) {
        cat_cache_use = 1;
        for (unsigned int index = 0; index < CAT_TEXTURE_CACHE_CAT_LIMIT;
             ++index) {
            cat_cache[index].last_use = 0;
        }
    }
    slot->last_use = cat_cache_use;
}

static bool catHasLoadedTexture(CatId cat)
{
    for (CatAction action = CAT_ACTION_YOWL;
         action < CAT_ACTION_COUNT;
         action = (CatAction)(action + 1)) {
        if (cat_loaded[catTextureIndex(cat, action)]) {
            return true;
        }
    }
    return false;
}

static void releaseCatTextures(CatId cat)
{
    for (CatAction action = CAT_ACTION_YOWL;
         action < CAT_ACTION_COUNT;
         action = (CatAction)(action + 1)) {
        int texture_index = catTextureIndex(cat, action);
        if (cat_loaded[texture_index]) {
            int texture_id = cat_images[texture_index].textureID;
            glDeleteTextures(1, &texture_id);
            cat_images[texture_index] = (glImage){ 0 };
            cat_loaded[texture_index] = false;
        }
    }
}

static void releaseCatSlot(CatCacheSlot *slot)
{
    if (slot != NULL && slot->occupied) {
        releaseCatTextures(slot->cat);
        *slot = (CatCacheSlot){ 0 };
    }
}

static CatCacheSlot *acquireCatSlot(CatId cat)
{
    CatCacheSlot *slot = findCatSlot(cat);
    if (slot != NULL) {
        touchCatSlot(slot);
        return slot;
    }

    for (unsigned int index = 0; index < CAT_TEXTURE_CACHE_CAT_LIMIT; ++index) {
        if (!cat_cache[index].occupied) {
            slot = &cat_cache[index];
            break;
        }
    }
    if (slot == NULL) {
        slot = &cat_cache[0];
        for (unsigned int index = 1; index < CAT_TEXTURE_CACHE_CAT_LIMIT;
             ++index) {
            if (cat_cache[index].last_use < slot->last_use) {
                slot = &cat_cache[index];
            }
        }
        releaseCatSlot(slot);
    }

    slot->cat = cat;
    slot->occupied = true;
    touchCatSlot(slot);
    return slot;
}

bool catTextureLoad(CatId cat, CatAction action)
{
    int index = catTextureIndex(cat, action);
    if (index == CAT_TEXTURE_COUNT) {
        return false;
    }
    if (cat_loaded[index]) {
        CatCacheSlot *slot = findCatSlot(cat);
        if (slot != NULL) {
            touchCatSlot(slot);
        }
        return true;
    }

    char image_path[64];
    char palette_path[64];
    int image_length = snprintf(image_path, sizeof(image_path),
                                "nitro:/cats/%s_%s.img.bin",
                                cat_resource_names[cat],
                                action_resource_names[action]);
    int palette_length = snprintf(palette_path, sizeof(palette_path),
                                  "nitro:/cats/%s_%s.pal.bin",
                                  cat_resource_names[cat],
                                  action_resource_names[action]);
    if (image_length < 0 || (size_t)image_length >= sizeof(image_path) ||
        palette_length < 0 || (size_t)palette_length >= sizeof(palette_path) ||
        !readExactFile(image_path, cat_image_buffer, sizeof(cat_image_buffer)) ||
        !readExactFile(palette_path, cat_palette_buffer,
                       sizeof(cat_palette_buffer))) {
        return false;
    }

    CatCacheSlot *slot = acquireCatSlot(cat);
    int texture_id = glLoadTileSet(
        &cat_images[index], CAT_TEXTURE_WIDTH, CAT_TEXTURE_HEIGHT,
        CAT_TEXTURE_WIDTH, CAT_TEXTURE_HEIGHT, GL_RGB256,
        CAT_TEXTURE_WIDTH, CAT_TEXTURE_HEIGHT,
        TEXGEN_TEXCOORD | GL_TEXTURE_COLOR0_TRANSPARENT,
        CAT_PALETTE_ENTRIES, cat_palette_buffer, cat_image_buffer);
    if (texture_id < 0) {
        if (!catHasLoadedTexture(cat)) {
            *slot = (CatCacheSlot){ 0 };
        }
        return false;
    }

    cat_loaded[index] = true;
    touchCatSlot(slot);
    return true;
}

bool catTexturesLoad(CatId cat)
{
    if ((unsigned int)cat >= CAT_COUNT) {
        return false;
    }
    for (CatAction action = CAT_ACTION_YOWL;
         action < CAT_ACTION_COUNT;
         action = (CatAction)(action + 1)) {
        if (!catTextureLoad(cat, action)) {
            releaseCatSlot(findCatSlot(cat));
            return false;
        }
    }
    return true;
}

void catTexturesReset(void)
{
    for (unsigned int index = 0; index < CAT_TEXTURE_CACHE_CAT_LIMIT; ++index) {
        releaseCatSlot(&cat_cache[index]);
    }
    cat_cache_use = 0;
}

void catTextureDraw(CatId cat, CatAction action, int x, int y,
                    bool horizontal_flip)
{
    int index = catTextureIndex(cat, action);
    if (index == CAT_TEXTURE_COUNT || !cat_loaded[index]) {
        return;
    }

    CatCacheSlot *slot = findCatSlot(cat);
    if (slot != NULL) {
        touchCatSlot(slot);
    }

    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(0));
    glColor(RGB15(31, 31, 31));
    glSprite(x, y, horizontal_flip ? GL_FLIP_H : GL_FLIP_NONE,
             &cat_images[index]);
}

void catTextureDrawScaled(CatId cat, CatAction action, int x, int y,
                          bool horizontal_flip, unsigned int scale)
{
    int index = catTextureIndex(cat, action);

    if (index == CAT_TEXTURE_COUNT || !cat_loaded[index] || scale == 0u) {
        return;
    }

    CatCacheSlot *slot = findCatSlot(cat);
    if (slot != NULL) {
        touchCatSlot(slot);
    }

    glPolyFmt(POLY_ALPHA(31) | POLY_CULL_NONE | POLY_ID(0));
    glColor(RGB15(31, 31, 31));
    glSpriteScaleXY(x, y, (int)(scale << 4), (int)(scale << 4),
                    horizontal_flip ? GL_FLIP_H : GL_FLIP_NONE,
                    &cat_images[index]);
}
