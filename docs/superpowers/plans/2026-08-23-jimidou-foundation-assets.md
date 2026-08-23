# 基米斗 Foundation and Assets Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 生成首个可启动的 `PussiFight.nds`，并建立可重复的猫图、双语字库、音效和流式音乐流水线。

**Architecture:** NDS 平台代码只负责设备初始化和显示，资源生成由 Python/FFmpeg 在构建前完成。文本 ID、猫种资料和平台服务均以窄接口暴露，使后续纯 C 战斗代码可在主机端编译测试。

**Tech Stack:** C11、BlocksDS/Make、libnds、GL2D、Maxmod、NitroFS、Python 3/Pillow、FFmpeg

**Spec:** `docs/superpowers/specs/2026-08-23-jimidou-design.md`

## Global Constraints

- BlocksDS 根目录默认 `/opt/blocksds/core`，Wonderful 根目录默认 `/opt/wonderful`，但保留环境变量覆盖。
- 输出固定为 `PussiFight.nds`，使用 `$(BLOCKSDS)/sys/arm7/main_core/arm7_maxmod.elf`。
- 35 张源图保留 RGBA；生成图为 128×128、透明背景、256 色，敌方运行时水平翻转。
- SFX 为 16000 Hz 单声道 8-bit PCM；BGM 为 22050 Hz 单声道 16-bit PCM 并从 NitroFS 流式播放。
- `FFMPEG` 可覆盖；缺失时资源目标给出明确错误，普通 C 增量构建不硬编码本机路径。
- 所有用户可见文字位于中英文字符串表。

---

## File Map

- `Makefile`：NDS 构建、资源和主机测试入口。
- `include/game_config.h`：帧率、猫基础属性和资源常量。
- `include/game_terms.h`、`source/localization/*`：稳定文本 ID 和双语表。
- `tools/fetch_cats.py`、`tools/process_cats.py`：下载、裁切、缩放和清单校验。
- `tools/convert_audio.ps1`：FFmpeg 转换；`source/audio/*`：Maxmod SFX/BGM 服务。
- `source/graphics/*`、`source/ui/*`：上下屏初始化、纹理缓存和标题演示。
- `tests/host/*`：不依赖 libnds 的配置、本地化与资源清单测试。

### Task 1: BlocksDS Skeleton and Host Test Harness

**Files:**
- Create: `Makefile`, `.gitignore`, `icon.gif`, `source/main.c`, `include/game_config.h`
- Create: `tests/host/Makefile`, `tests/host/test_config.c`

**Interfaces:**
- Produces: `CatId`, `CatBaseStats`, `const CatBaseStats *configCatBase(CatId id)` and Make targets `host-test`, `assets`, `PussiFight.nds`.

- [ ] **Step 1: Write the failing configuration test**

```c
#include "game_config.h"
#include <assert.h>
int main(void) {
    const CatBaseStats *orange = configCatBase(CAT_ORANGE);
    const CatBaseStats *banana = configCatBase(CAT_BANANA);
    assert(orange->max_hp == 60 && orange->attack == 15);
    assert(orange->rage_per_tick == 5 && orange->heal_per_tick == 15);
    assert(banana->action_cd_frames == 90);
    assert(GAME_FPS == 60 && CAT_COUNT == 5);
    return 0;
}
```

- [ ] **Step 2: Run the test and verify the missing header failure**

Run: `C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && make host-test"`

Expected: FAIL with `game_config.h: No such file or directory`.

- [ ] **Step 3: Add the minimal config and build skeleton**

Define `CatId` in the order orange, tabby, maodie, chouju, banana. Store CD as frames `{120,120,120,120,90}` and the remaining spec values in one `static const CatBaseStats[CAT_COUNT]`; make `configCatBase()` bounds-safe by returning orange for an invalid ID. Base the NDS Makefile on BlocksDS `templates/rom_arm9_only`, with `NAME := PussiFight`, `INCLUDEDIRS := include`, `NITROFSDIR := nitrofs`, `AUDIODIRS := assets/audio/sfx`, and add `host-test: $(MAKE) -C tests/host`.

- [ ] **Step 4: Verify host and ROM builds**

Run: `C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && make host-test && make -j2"`

Expected: host executable exits 0 and `PussiFight.nds` is created.

- [ ] **Step 5: Commit**

```bash
git add Makefile .gitignore icon.gif include source tests
git commit -m "build: bootstrap BlocksDS project"
```

### Task 2: Stable Chinese/English Text Catalog

**Files:**
- Create: `include/game_terms.h`, `include/localization.h`
- Create: `source/localization/localization.c`, `source/localization/strings_zh_cn.c`, `source/localization/strings_en.c`
- Create: `tests/host/test_localization.c`, `tools/extract_glyphs.py`

**Interfaces:**
- Consumes: `CatId` from `game_config.h`.
- Produces: `Language { LANG_ZH_CN, LANG_EN }`, contiguous `GameTextId`, `void textSetLanguage(Language)`, `const char *textGet(GameTextId)`, and `assets/fonts/required_glyphs.txt`.

- [ ] **Step 1: Write completeness and switching tests**

```c
for (int lang = 0; lang < LANG_COUNT; ++lang) {
    textSetLanguage((Language)lang);
    for (int id = 0; id < TEXT_COUNT; ++id) {
        const char *s = textGet((GameTextId)id);
        assert(s && s[0]);
    }
}
textSetLanguage(LANG_ZH_CN); assert(strcmp(textGet(TEXT_GAME_TITLE), "基米斗") == 0);
textSetLanguage(LANG_EN); assert(strcmp(textGet(TEXT_GAME_TITLE), "PussiFight") == 0);
```

- [ ] **Step 2: Run and verify undefined text API failure**

Run: `C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && make -C tests/host test_localization"`

Expected: FAIL because `game_terms.h` does not exist.

- [ ] **Step 3: Implement the complete catalog**

Add IDs for title/menu/settings, five cats, four actions, eight buffs, eight boss templates, battle statuses, rules/tutorial, prebattle modifiers, save/audio errors and credits. Define both arrays as `[TEXT_COUNT]` designated initializers and make invalid IDs return the localized `TEXT_INVALID`. `extract_glyphs.py` must parse both C tables as UTF-8, collect unique Unicode code points, sort them, and write one line without duplicates.

- [ ] **Step 4: Run tests and glyph extraction**

Run: `C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && make host-test && python3 tools/extract_glyphs.py"`

Expected: PASS; `assets/fonts/required_glyphs.txt` contains `基米斗` characters and ASCII letters.

- [ ] **Step 5: Commit**

```bash
git add include source/localization tests/host tools/extract_glyphs.py assets/fonts/required_glyphs.txt
git commit -m "feat: add bilingual text catalog"
```

### Task 3: Deterministic Cat Image Pipeline

**Files:**
- Create: `tools/cat_manifest.json`, `tools/fetch_cats.py`, `tools/process_cats.py`, `tools/requirements-assets.txt`
- Create: `tests/host/test_asset_manifest.py`
- Generate: `assets_src/cats/*.png`, `assets/cats/*.png`, `assets/cats/*.grit`

**Interfaces:**
- Produces: filenames `{orange,tabby,chouju,maodie,banana}_{yowl,hiss,scratch,hit,heal,dead,idle}.png`, all exactly 128×128 RGBA.

- [ ] **Step 1: Write the manifest test**

```python
assert set(manifest["cats"]) == {"a", "b", "c", "d", "e"}
assert set(manifest["actions"]) == {str(i) for i in range(1, 8)}
assert len(list(output.glob("*.png"))) == 35
for path in output.glob("*.png"):
    with Image.open(path) as im:
        assert im.size == (128, 128) and im.mode == "RGBA"
        assert im.getbbox() is not None
```

- [ ] **Step 2: Verify it fails before generated assets exist**

Run: `C:\msys64\opt\wonderful\toolchain\python\python.exe -m unittest tests.host.test_asset_manifest -v`

Expected: FAIL with zero generated PNG files. If that Python path is absent, use the `python3` found inside Wonderful shell and record it in `ASSET_PYTHON`.

- [ ] **Step 3: Implement fetch and processing scripts**

Manifest maps `a..e` and `1..7` exactly as the spec. `fetch_cats.py --output assets_src/cats` downloads only missing files, validates HTTP status and PNG signature, and writes via `.part` then rename. `process_cats.py` trims alpha bounds, scales the longest visible dimension to 112 px with Lanczos, applies a light unsharp mask, centers on transparent 128×128, and never overwrites source files. Generate a `.grit` beside each output with `-g -gTFF00FF -gt -gB8 -m! -p!` settings appropriate to GL texture conversion.

- [ ] **Step 4: Generate and validate all images**

Run: `C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && python3 tools/fetch_cats.py --output assets_src/cats && python3 tools/process_cats.py && python3 -m unittest tests.host.test_asset_manifest -v"`

Expected: 35 sources, 35 processed images, all tests PASS. Visually inspect one contact sheet at native 256×192 scale and confirm no clipped paws/tails or opaque white boxes.

- [ ] **Step 5: Commit**

```bash
git add tools tests/host/test_asset_manifest.py assets_src/cats assets/cats
git commit -m "assets: add reproducible cat sprite pipeline"
```

### Task 4: Audio Conversion and Maxmod Services

**Files:**
- Create: `tools/convert_audio.ps1`, `tools/audio_manifest.json`, `tests/host/test_audio_assets.ps1`
- Generate: `assets/audio/sfx/*.wav`, `nitrofs/audio/menu.wav`, `nitrofs/audio/battle.wav`
- Create: `include/audio_service.h`, `source/audio/audio_service.c`, `source/audio/music_stream.c`

**Interfaces:**
- Produces: `SfxId` enumerating start/yowl/normal-hiss/scratch-1/scratch-2/heal/death/Maodie-combined/Banana-attack/Banana-heal/Banana-death as manifest aliases, `MusicId { MUSIC_NONE, MUSIC_MENU, MUSIC_BATTLE }`, `bool audioInit(void)`, `void audioPlaySfx(SfxId)`, `void audioPlayScratch(CatId)`, `void audioSetMusic(MusicId)`, `void audioUpdate(void)`, `void audioShutdown(void)`; all calls are safe after init failure. The two normal scratch files remain separate bank entries, while logical routing may share or alias entries, so the converted source-file count stays ten.

- [ ] **Step 1: Write WAV-format assertions**

```powershell
$sfx = Get-ChildItem assets/audio/sfx/*.wav
if ($sfx.Count -ne 10) { throw "expected 10 converted SFX" }
& $ffprobe -v error -select_streams a:0 -show_entries stream=sample_rate,channels,bits_per_sample -of csv=p=0 $sfx[0]
# Expected exact line: 16000,1,8
```

- [ ] **Step 2: Run conversion check and verify missing outputs**

Run: `powershell -ExecutionPolicy Bypass -File tests/host/test_audio_assets.ps1`

Expected: FAIL with `expected 10 converted SFX`.

- [ ] **Step 3: Implement conversion and playback**

Use `$env:FFMPEG` when set, else probe the verified Format Factory path, then `Get-Command ffmpeg`. Convert SFX with `-ar 16000 -ac 1 -c:a pcm_u8`; convert BGM with `-ar 22050 -ac 1 -c:a pcm_s16le`. Map the Chinese source filenames in JSON and preserve the full combined Maodie file for both hiss/scratch. Implement BGM streaming from `nitro:/audio/*.wav` using a 16 KiB circular buffer based on BlocksDS `examples/maxmod/streaming`; `audioUpdate()` fills outside IRQ and performs 30-frame volume fades. Random normal scratch selects one of two samples; Banana and Maodie route to their dedicated samples.

- [ ] **Step 4: Convert, test, and build ROM**

Run: `$env:FFMPEG='C:\Desktop\格式工厂_v5.15.0_x64\格式工厂_v5.15.0.0_x64\ffmpeg.exe'; powershell -ExecutionPolicy Bypass -File tools/convert_audio.ps1; powershell -ExecutionPolicy Bypass -File tests/host/test_audio_assets.ps1`

Run: `C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && make -j2"`

Expected: format checks PASS; `soundbank.h` is generated; ROM links without loading BGM into ARM9 memory.

- [ ] **Step 5: Commit**

```bash
git add tools tests/host/test_audio_assets.ps1 assets/audio nitrofs/audio include/audio_service.h source/audio Makefile
git commit -m "feat: add NDS audio conversion and streaming"
```

### Task 5: Graphics Shell and Bilingual Title Demo

**Files:**
- Create: `include/graphics_service.h`, `include/ui_scene.h`
- Create: `source/graphics/graphics_service.c`, `source/graphics/cat_textures.c`
- Create: `source/ui/title_scene.c`
- Modify: `source/main.c`

**Interfaces:**
- Consumes: localization and audio services, generated cat assets.
- Produces: `graphicsInit()`, `catTexturesLoad(CatId)`, `catTextureDraw(...)`, and a title scene that switches language with SELECT and exits only via power/reset.

- [ ] **Step 1: Add a compile-time texture manifest test**

Create `tests/host/test_texture_ids.c` with assertions that `CAT_ACTION_COUNT == 7`, `CAT_TEXTURE_COUNT == 35`, and `catTextureIndex(CAT_BANANA, CAT_ACTION_IDLE) == 34`; add it to `host-test`.

- [ ] **Step 2: Run and verify missing graphics API failure**

Run: `C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && make host-test"`

Expected: FAIL because `graphics_service.h` is missing.

- [ ] **Step 3: Implement the title demo**

Initialize video modes, VRAM banks, GL2D top screen and 2D sub screen. Load only orange idle on title, draw `textGet(TEXT_GAME_TITLE)`, and render `SELECT: 中文/English` using the generated glyph data. Play menu BGM after `audioInit()`, show a persistent localized muted warning if initialization fails, call `audioUpdate()` once per frame, and switch language immediately on SELECT.

- [ ] **Step 4: Build and manually smoke test**

Run: `C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && make host-test && make -j2"`

Expected: tests PASS and ROM builds. In melonDS, title appears on both screens, SELECT toggles all visible text without reboot, menu music loops, and the orange cutout is recognizable at native resolution.

- [ ] **Step 5: Commit**

```bash
git add include source tests Makefile
git commit -m "feat: add bilingual NDS title shell"
```
