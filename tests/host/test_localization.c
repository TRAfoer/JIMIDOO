#include "game_terms.h"
#include "localization.h"

#include <assert.h>
#include <string.h>

int main(void)
{
    for (int language = 0; language < LANG_COUNT; ++language) {
        textSetLanguage((Language)language);
        for (int id = 0; id < TEXT_COUNT; ++id) {
            const char *text = textGet((GameTextId)id);
            assert(text != 0 && text[0] != '\0');
        }
    }

    textSetLanguage(LANG_ZH_CN);
    assert(strcmp(textGet(TEXT_GAME_TITLE), "基米斗") == 0);
    assert(strcmp(textGet(TEXT_TITLE_LANGUAGE_TOGGLE), "SELECT: 中文/English") == 0);
    assert(strcmp(textGet(TEXT_CAT_NAME(CAT_MAODIE)), "耄耋") == 0);
    assert(strcmp(textGet((GameTextId)-1), textGet(TEXT_INVALID)) == 0);

    textSetLanguage(LANG_EN);
    assert(strcmp(textGet(TEXT_GAME_TITLE), "PussiFight") == 0);
    assert(strcmp(textGet(TEXT_TITLE_LANGUAGE_TOGGLE), "SELECT: 中文/English") == 0);
    assert(strcmp(textGet(TEXT_CAT_NAME(CAT_MAODIE)), "Maodie") == 0);
    assert(strcmp(textGet((GameTextId)TEXT_COUNT), textGet(TEXT_INVALID)) == 0);
    return 0;
}
