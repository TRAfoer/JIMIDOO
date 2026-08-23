#ifndef LOCALIZATION_H
#define LOCALIZATION_H

#include "game_terms.h"

typedef enum Language {
    LANG_ZH_CN,
    LANG_EN,
    LANG_COUNT
} Language;

void textSetLanguage(Language language);
const char *textGet(GameTextId id);

#endif
