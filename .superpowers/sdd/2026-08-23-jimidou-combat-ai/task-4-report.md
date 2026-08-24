# Task 4 Report: NDS Battle Scene, Controls, HUD, and Program Animation

## Status

Implemented on `main` from base `d3f93b6d489c6386d7a9702396fed1496527aa29`.

The temporary title flow launches an orange player against a deterministic
tabby opponent at crisis 1. The battle scene returns to the title after either
fighter dies. Physical and touch input produce a single `BattleCommand` and
are submitted only on the current frame.

## RED / GREEN record

### Exact touch layout

- RED command: `make -C tests/host test_touch_layout`
- RED result: MSVC compiled `test_touch_layout.c`, then the linker failed with
  `LNK2019` because `touchCommandAt` was undefined.
- GREEN implementation: `touchCommandAt` rejects the status row and all
  out-of-range coordinates, splits at `x=128`, and splits the button rows at
  `y=128`.
- GREEN coverage: all four button centers, every binding edge, status edges,
  and screen-outside coordinates pass.

### Cooldown tenths

- RED result: the focused target failed with `LNK2019` because
  `battleHudCooldownTenths` was undefined.
- GREEN implementation: ceiling division converts 60 FPS frames to displayed
  tenths (`0 -> 0`, `1..6 -> 1`, `7 -> 2`, `60 -> 10`, `120 -> 20`).

### Retained HUD dirty regions

- RED result: the focused target failed with `LNK2019` because
  `battleHudDirtyRegions` was undefined.
- GREEN implementation: HP/rage changes dirty only the status region;
  cooldown/channel/availability changes dirty only buttons; pause and stun
  transitions dirty both. A nonzero stun countdown changing to another
  nonzero value produces no lower-screen writes.

### Bare ROM build contract

- RED result: a behavioral dry-run of bare `make --always-make` never reached
  `NDSTOOL`; Make had selected the first generated object dependency as its
  implicit default goal.
- GREEN implementation: `.DEFAULT_GOAL := all` makes the required bare
  `make -j2` command traverse compilation, linking, and ROM packaging.
- GREEN result: the new contract test passes and the full contract suite now
  reports 8 passing tests.

## Implementation notes

- `battle_scene.c` owns input, pause, battle/AI stepping, event delivery, and
  cat-aware audio routing. It uses `keysDown()` only, so actions are not
  buffered.
- START toggles `BattleState.paused`; the paused branch calls neither
  `aiChoose` nor `battleTick`. Audio update and top rendering continue.
- Both selected cats are reset and all seven textures per cat are loaded
  before entering the combat loop. Animation performs no heap allocation or
  file I/O.
- `battle_animation.c` consumes accepted actions and battle events. Static cat
  textures receive bounded 6-12 frame lunges, shake, flash, afterimages,
  particles, damage/heal numbers, localized dodge/counter/warning captions,
  and persistent dead poses using integer-only frame logic.
- Action submissions use existing cat-aware scratch, hiss, and heal routes;
  yowl uses the existing yowl sample; battle-end events use cat-aware death
  audio.
- All UI words come from existing `GameTextId` entries. Only physical key
  glyphs and numeric format punctuation are literals.

## No-flicker audit

- The lower screen remains the existing single 16-bit bitmap.
- `battleHudDraw` is the first operation after each battle VBlank.
- `graphicsSubClear` is called only for the initial static battle base.
- Later updates repaint only `y=0..63` status or one/all exact 128x64 button
  regions when their cached visible values change.
- No full lower-screen clear or per-frame whole-screen redraw exists in the
  battle loop.
- Top-screen GL2D rendering remains per-frame as permitted.

## Fresh verification

Focused host command:

```text
C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && make -C tests/host test_touch_layout"
```

Result: PASS; MSVC `/W4 /WX`, then `test_touch_layout.exe` exited 0.

Full host command used workspace-local temporary storage:

```text
C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && env TEMP='C:/Desktop/JiMiDoo/build/tmp' TMP='C:/Desktop/JiMiDoo/build/tmp' make host-test"
```

Result: PASS; all C executables passed, all Python suites passed, host tool
discovery passed, and audio asset format checks passed.

Forced ROM command:

```text
C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && env PATH='/opt/wonderful/bin:/opt/wonderful/toolchain/gcc-arm-none-eabi/bin:/usr/bin:/bin' BLOCKSDS='/opt/wonderful/thirdparty/blocksds/core' WONDERFUL_TOOLCHAIN='/opt/wonderful' PYTHON='/c/Users/lhf19/AppData/Local/Programs/Python/Python312/python.exe' TEMP='/c/Desktop/JiMiDoo/build/tmp' TMP='/c/Desktop/JiMiDoo/build/tmp' make -j2"
```

Result: PASS; compiled the changed battle animation/HUD/scene objects, linked
`build/PussiFight.elf`, and packaged `PussiFight.nds`.

ROM artifact:

- Size: 8,203,264 bytes
- SHA-256: `D447B029A8DE471BA298E1FF2A51FB8082B24F7745E546D4BB3E98B032A3983A`

## Self-review

- Confirmed the exact touch coordinate contract and physical L/R/Y/A mapping.
- Confirmed touch and physical controls converge before one `battleSubmit`.
- Confirmed successful actions are not queued for later frames.
- Confirmed pause gates player submission, AI choice, battle tick, cooldown,
  channel, and warning progress while audio/render remain active.
- Confirmed the demo setup is orange versus seeded tabby with `crisis = 1`.
- Confirmed selected textures load before the loop and event animation uses
  only fixed-size stack/static state.
- Confirmed lower writes occur after VBlank and only on cached visible changes.
- Confirmed no natural-language UI literal was introduced.
- Confirmed `Audios/` remains untouched and untracked.

## Concerns / unavailable checks

- melonDS was not found on PATH or in standard installation locations, so the
  requested emulator smoke test was unavailable. Physical/touch equivalence,
  pause timing, match completion, audio, and lower-screen flicker are covered
  by code review, host tests, and a successful ROM build, but still require a
  visual/input/audio smoke test in melonDS or on hardware.
- Final localized text fit and the perceptual quality of top-screen effects
  require emulator or hardware visual QA.
