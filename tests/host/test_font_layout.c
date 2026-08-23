#include <assert.h>
#include <stdint.h>

#include "font_layout.h"
#include "localization.h"

static void assertTitleLanguageFits(Language language)
{
    textSetLanguage(language);
    assert(fontTextWidth(textGet(TEXT_GAME_TITLE), FONT_SCALE_ONE) <= 224);
    assert(fontTextWidth(textGet(TEXT_TITLE_LANGUAGE_TOGGLE), FONT_SCALE_HALF) <= 224);
    assert(fontTextWidth(textGet(TEXT_ERROR_AUDIO_MUTED), FONT_SCALE_HALF) <= 224);
}

int main(void)
{
    const char *utf8 = "中A";
    uint32_t codepoint = 0;
    assert(fontUtf8Next(&utf8, &codepoint));
    assert(codepoint == UINT32_C(0x4E2D));
    assert(fontUtf8Next(&utf8, &codepoint));
    assert(codepoint == 'A');
    assert(!fontUtf8Next(&utf8, &codepoint));

    const JimiDooGlyphMetric *capital = fontGlyphFind('P');
    const JimiDooGlyphMetric *lowercase = fontGlyphFind('u');
    assert(capital != NULL);
    assert(lowercase != NULL);
    assert(capital->bearing_y != lowercase->bearing_y);
    assert(fontGlyphFind(UINT32_C(0x10FFFF)) == NULL);

    assertTitleLanguageFits(LANG_ZH_CN);
    assertTitleLanguageFits(LANG_EN);
    return 0;
}
