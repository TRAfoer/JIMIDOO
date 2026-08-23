#include <assert.h>

#include "graphics_service.h"

_Static_assert(CAT_ACTION_COUNT == 7, "cat action count must match NitroFS assets");
_Static_assert(CAT_TEXTURE_COUNT == 35, "cat texture count must match NitroFS assets");
_Static_assert(CAT_BANANA * CAT_ACTION_COUNT + CAT_ACTION_IDLE == 34,
               "banana idle must be the final texture slot");

int main(void)
{
    assert(catTextureIndex(CAT_BANANA, CAT_ACTION_IDLE) == 34);
    assert(catTextureIndex(CAT_ORANGE, CAT_ACTION_YOWL) == 0);
    assert(catTextureIndex(CAT_COUNT, CAT_ACTION_IDLE) == CAT_TEXTURE_COUNT);
    assert(catTextureIndex(CAT_ORANGE, CAT_ACTION_COUNT) == CAT_TEXTURE_COUNT);
    return 0;
}
