#include "ui_scene.h"

TitleSceneInitStatus titleSceneStatusForCatLoad(bool cat_loaded)
{
    return cat_loaded ? TITLE_SCENE_INIT_READY :
        TITLE_SCENE_INIT_CAT_UNAVAILABLE;
}

bool titleSceneCanRun(TitleSceneInitStatus status)
{
    return status == TITLE_SCENE_INIT_READY;
}
