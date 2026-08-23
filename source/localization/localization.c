#include "localization.h"

extern const char *const text_table_zh_cn[TEXT_COUNT];
extern const char *const text_table_en[TEXT_COUNT];

static Language current_language = LANG_ZH_CN;

void textSetLanguage(Language language)
{
    current_language = language == LANG_EN ? LANG_EN : LANG_ZH_CN;
}

const char *textGet(GameTextId id)
{
    const char *const *table = current_language == LANG_EN ? text_table_en : text_table_zh_cn;

    if ((unsigned int)id >= TEXT_COUNT) {
        id = TEXT_INVALID;
    }

    return table[id];
}
