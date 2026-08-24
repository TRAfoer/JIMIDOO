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

---

## Review Fix Round (2026-08-24)

Base commit: `26ea905bc3fb47b6cd985ce826254b4d96d892f3`.

### Findings resolved

1. Added a pure scene presentation router. An automatic-counter
   `EVENT_HISS_SUCCESS` now presents the defender's HISS pose and calls the
   defender cat's `audioPlayHiss` route. A submitted hiss is presented at
   acceptance and its own later success/fail event is suppressed, preventing
   duplicate animation/audio. A same-frame ordering regression ensures an
   earlier automatic counter is not mistaken for the later submitted result.
2. Accepted warning scratches now record a deferred scratch and present
   neither pose nor audio at submission. `EVENT_HIT` or `EVENT_DODGE` presents
   the attacker scratch when the 42-frame warning resolves. A successful hiss
   clears the deferred scratch, so canceled warnings never play attack
   animation/audio.
3. Added a real GL2D scaled-sprite graphics path. Hiss/yowl/heal poses pulse to
   about 1.12x, hit poses compress around their center, and warning rendering
   draws a blinking exclamation mark from fixed rectangles next to the
   localized warning label.
4. Added a pure terminal lifecycle with exactly 60 additional frames. Once
   battle end occurs, input scanning, submissions, AI, and battle ticks stop;
   each hold frame still runs VBlank, dirty HUD rendering, animation/audio
   update, and top rendering. The first hold VBlank paints final 0 HP and
   terminal button availability, while the dead texture remains visible.
5. Title language initialization now defaults to Simplified Chinese only on
   first startup. Subsequent title initialization after battle reapplies the
   selected language instead of resetting it.
6. Added named default modifier constants in `game_config.h`; debug fighters
   use dodge 0%, warning 30%, and counter 40%. Main contains no raw modifier
   balance values.

### Review-round RED / GREEN record

- Presentation routing RED: `test_battle_scene_policy` linked with `LNK2019`
  for undefined route functions. GREEN covers automatic counter, explicit
  hiss no-double, warning hit/dodge resolution, warning cancellation, and
  rejected actions.
- Terminal lifecycle RED: the policy test linked with `LNK2019` for undefined
  lifecycle functions. GREEN proves the terminal frame starts a 60-frame hold,
  all first 59 hold frames continue, and frame 60 terminates with zero
  remaining.
- Language RED: `test_title_scene_draw.exe` aborted on
  `last_language == LANG_EN` after a successful reinit. GREEN preserves and
  reapplies English.
- Configuration RED: `test_config.c` failed compilation because all three
  default modifier identifiers were absent. GREEN asserts exact `0/30/40`
  values and main consumes them.
- Scale RED: `test_texture_cache` linked with `LNK2019` for undefined
  `catTextureDrawScaled`. GREEN confirms the loaded texture is drawn through
  `glSpriteScaleXY` with the requested fixed-point X/Y scale.
- Event-order RED: the policy test asserted because a same-side explicit hiss
  suppressed an earlier automatic-counter event. GREEN pairs the submitted
  hiss with only the last queued hiss result for that side.

### Fresh fix-round verification

Focused command:

```text
C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && make -C tests/host test_touch_layout test_battle_scene_policy && make -C tests/host test_config.exe test_title_scene_draw.exe test_texture_cache.exe && cd tests/host && ./test_config.exe && ./test_title_scene_draw.exe && ./test_texture_cache.exe"
```

Result: PASS, exit 0.

Full host command:

```text
C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && env TEMP='C:/Desktop/JiMiDoo/build/tmp' TMP='C:/Desktop/JiMiDoo/build/tmp' make host-test"
```

Result: PASS, exit 0. All 13 C executables, all Python suites (including 8
ROM build-contract tests), host tool discovery, and audio format checks pass.

Forced ROM command:

```text
C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && env PATH='/opt/wonderful/bin:/opt/wonderful/toolchain/gcc-arm-none-eabi/bin:/usr/bin:/bin' BLOCKSDS='/opt/wonderful/thirdparty/blocksds/core' WONDERFUL_TOOLCHAIN='/opt/wonderful' PYTHON='/c/Users/lhf19/AppData/Local/Programs/Python/Python312/python.exe' TEMP='/c/Desktop/JiMiDoo/build/tmp' TMP='/c/Desktop/JiMiDoo/build/tmp' make -j2"
```

Result: PASS, exit 0; compiled the changed scene object, linked the ELF, and
packaged `PussiFight.nds`.

- Size: 8,204,288 bytes
- SHA-256: `0CED85026A2E4E89D8EF9CF657401DF75223C1F4D3B0C229B22905B76E1B59FD`

### Fix-round self-review and remaining concerns

- The no-flicker ruling remains intact: lower writes begin immediately after
  each VBlank; the full lower bitmap is cleared only on initial battle HUD
  setup; later writes remain cached status/button regions.
- Normal HUD values become visible at the next VBlank after simulation, which
  is the expected retained-bitmap frame boundary. Reordering simulation ahead
  of lower writes would weaken the binding that lower writes happen
  immediately after VBlank. The terminal hold specifically guarantees the
  final state receives that next VBlank.
- `BATTLE_ABORTED` still returns to the title without a dedicated message.
  There is no appropriate existing localization ID for texture-load failure,
  and Task 4 forbids adding natural-language UI literals, so this minor was not
  broadened into a localization/error-screen change.
- melonDS remains unavailable in this environment. Visual scale/icon quality,
  terminal hold timing, flicker, and audio sequencing still need emulator or
  hardware smoke testing.
- `Audios/` remained untouched and untracked throughout the fix round.

---

## Final Warning-Cancellation Fix (2026-08-24)

Base commit: `0bc766a57570aece73874d274c79319b7eb2df38`.

Whole-stage review found that the battle core canceled a pending warned
scratch on successful hiss while the presentation layer retained its warning
timer. The stale blinking exclamation and warning label could therefore remain
visible after the attack no longer existed.

### RED / GREEN

- Added `test_battle_animation.c` using the real pure animation state.
- RED: `make -C tests/host test_battle_animation` compiled and linked, then
  aborted on `animation.warning_frames == 0u`; the value was still 42 after
  `EVENT_HISS_SUCCESS`.
- GREEN: `battleAnimationOnEvents` now treats `EVENT_HISS_SUCCESS` as
  authoritative cancellation of the single global warning presentation and
  clears `warning_frames` before showing the counter caption.
- The paired regression proves `EVENT_HISS_FAIL` preserves the active warning.

### Fresh verification

- Focused `make -C tests/host test_battle_animation`: PASS.
- Full host suite with workspace `TEMP`/`TMP`: PASS; all 14 C executables,
  Python suites, 8 ROM build-contract tests, tool discovery, and audio format
  checks passed.
- Forced ROM build with the exact Task 4 environment: PASS; compiled
  `battle_animation.c`, linked `build/PussiFight.elf`, and packaged the ROM.
- ROM size: 8,204,288 bytes.
- ROM SHA-256:
  `49D8A7F1AC187A2E486B4C702985939DA35622B985155FAA4BD8AF3888752808`.

### Self-review

- Only successful hiss clears the warning presentation; failed hiss does not.
- The single global warning timer matches the battle core's single pending
  warned scratch.
- No HUD, simulation timing, balance constants, or deferred review minors were
  changed.
- The lower-screen retained-bitmap/no-flicker path is untouched.
- `Audios/` remains untouched and untracked.
- melonDS remains unavailable, so visual emulator smoke testing is still
  pending.
