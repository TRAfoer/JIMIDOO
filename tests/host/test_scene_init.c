#include <assert.h>

#include "ui_scene.h"

int main(void)
{
    assert(titleSceneStatusForCatLoad(false) ==
           TITLE_SCENE_INIT_CAT_UNAVAILABLE);
    assert(titleSceneStatusForCatLoad(true) == TITLE_SCENE_INIT_READY);
    assert(!titleSceneCanRun(TITLE_SCENE_INIT_CAT_UNAVAILABLE));
    assert(titleSceneCanRun(TITLE_SCENE_INIT_READY));
    return 0;
}
