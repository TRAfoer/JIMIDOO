#include "game_config.h"

#include <assert.h>

int main(void)
{
    const CatBaseStats *orange = configCatBase(CAT_ORANGE);
    const CatBaseStats *banana = configCatBase(CAT_BANANA);

    assert(orange->max_hp == 60 && orange->attack == 15);
    assert(orange->rage_per_tick == 5 && orange->heal_per_tick == 15);
    assert(banana->action_cd_frames == 90);
    assert(GAME_FPS == 60 && CAT_COUNT == 5);
    assert(configCatBase((CatId)CAT_COUNT) == orange);
    return 0;
}
