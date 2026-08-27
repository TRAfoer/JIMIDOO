# Adaptive Battle AI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the stateless best-or-random opponent with a deterministic, fair AI that has a hidden per-battle personality, four-action memory, anti-repetition, a two-second opening patience phase, and variable observation delay.

**Architecture:** Keep `aiScoreActions()` as the visible-state utility layer. Add a pure policy layer that converts scores, profile biases, memory, anti-repeat rules, and injected noise into exactly 10,000 weighted tickets, then place a stateful `AiBrain` scheduler above it. The NDS battle scene records only accepted actions and asks the brain for a command once per frame; normal battles hide telemetry while Debug-launched battles pass a read-only snapshot to the existing renderer.

**Tech Stack:** C11, BlocksDS/libnds, fixed 60 FPS integer simulation, existing `BattleRng`, MSVC host tests, Wonderful Toolchain ROM build.

**Spec:** `docs/superpowers/specs/2026-08-23-jimidou-design.md` sections 9, 9.1, 9.2, 19, and 20.

## Global Constraints

- AI reads only committed battle state and accepted actions; it never reads current keys, touch state, future RNG values, or unsubmitted player intent.
- AI owns an RNG seeded with `battle_seed ^ UINT32_C(0xA17E5EED)`; it never consumes `BattleState.rng`.
- Normal UI never reveals the hidden profile. Only a battle launched through the Debug crisis menu may render telemetry.
- History capacity is exactly four accepted player actions, newest first; it is discarded when the battle ends.
- Opening patience is exactly 120 unpaused frames. The only permitted immediate opener is `CMD_YOWL`.
- Observation delay stays within the selected profile's inclusive range and never exceeds 36 unpaused frames.
- Pause freezes opening and observation timers. Cooldown, stun, death, or terminal battle state cancels an armed observation.
- Existing hiss, warning, dodge, cooldown, stun, channel, event FIFO, and 60-frame terminal presentation rules remain unchanged.
- No allocation, file access, or floating-point arithmetic occurs in AI or battle frames.
- All new visible text uses `GameTextId` and both localization tables.
- Do not stage, delete, rename, or overwrite the existing untracked `assets/cats/pixel64/`, action-sheet, revision, converter, or converter-test files.

---

### Task 1: Pure Adaptive Policy and Independent Brain RNG

**Files:**
- Modify: `include/ai.h`
- Create: `include/ai_internal.h`
- Modify: `source/ai/ai.c`
- Create: `source/ai/ai_policy.c`
- Create: `tests/host/test_ai_policy.c`
- Modify: `tests/host/Makefile`

**Interfaces:**
- Consumes: `AiScores aiScoreActions(const BattleState *, Side)` and `BattleRng` from the existing combat layer.
- Produces:

```c
typedef enum AiProfile {
    AI_PROFILE_AGGRESSIVE,
    AI_PROFILE_COUNTER,
    AI_PROFILE_RAGE,
    AI_PROFILE_SURVIVAL,
    AI_PROFILE_OPPORTUNIST,
    AI_PROFILE_TRICKSTER,
    AI_PROFILE_COUNT
} AiProfile;

enum {
    AI_MEMORY_CAPACITY = 4,
    AI_TICKET_TOTAL = 10000
};

typedef struct AiMemory {
    BattleCommand player[AI_MEMORY_CAPACITY]; /* newest first */
    uint8_t player_count;
    BattleCommand last_ai_action;
    uint8_t ai_repeat_count;
} AiMemory;

typedef struct AiBrain {
    BattleRng rng;
    AiProfile profile;
    AiMemory memory;
    uint16_t last_ticket[CMD_HEAL + 1];
} AiBrain;

typedef struct AiWeights { uint64_t value[CMD_HEAL + 1]; } AiWeights;
typedef struct AiTickets { uint16_t value[CMD_HEAL + 1]; } AiTickets;

void aiBrainInit(AiBrain *brain, uint32_t battle_seed);
void aiBrainRecordAccepted(AiBrain *brain, Side side, BattleCommand command);
BattleCommand aiBrainChooseNow(AiBrain *brain, const BattleState *battle,
                               Side side, uint8_t crisis);
uint16_t aiActionProbabilityCap(uint8_t crisis);
AiWeights aiPolicyWeights(const BattleState *battle, Side side,
                          uint8_t crisis, AiProfile profile,
                          const AiMemory *memory,
                          const int8_t noise_percent[CMD_HEAL + 1]);
AiTickets aiPolicyTickets(AiWeights weights, uint16_t cap_percent);
```

- Preserve the existing `aiChoose()` only until Task 3 switches the NDS loop; remove it in Task 3 so there is one decision path.

- [ ] **Step 1: Register the focused host executable**

Add `test_ai_policy.exe` to `TESTS` and add this real-source rule:

```make
test_ai_policy.exe: test_ai_policy.c ../../include/ai.h ../../include/ai_internal.h ../../source/ai/ai.c ../../source/ai/ai_policy.c ../../source/battle/battle_core.c ../../source/battle/battle_rng.c
	$$env:TEMP = (Get-Location).Path; $$env:TMP = $$env:TEMP; cmd.exe /c '$(MSVC_SETUP)cl.exe /nologo /std:c11 /W4 /WX /I../../include test_ai_policy.c ../../source/ai/ai.c ../../source/ai/ai_policy.c ../../source/battle/battle_core.c ../../source/battle/battle_rng.c /Fe:$@'
```

- [ ] **Step 2: Write RED tests for public brain state and pure policy**

Use literal expectations rather than reproducing the implementation. Cover:

```c
AiBrain first, second;
aiBrainInit(&first, UINT32_C(0x4A694D69));
aiBrainInit(&second, UINT32_C(0x4A694D69));
assert(first.profile == second.profile);
assert(first.rng.state == second.rng.state);

aiBrainRecordAccepted(&first, SIDE_PLAYER, CMD_HISS);
aiBrainRecordAccepted(&first, SIDE_PLAYER, CMD_HEAL);
aiBrainRecordAccepted(&first, SIDE_PLAYER, CMD_SCRATCH);
aiBrainRecordAccepted(&first, SIDE_PLAYER, CMD_YOWL);
aiBrainRecordAccepted(&first, SIDE_PLAYER, CMD_HEAL);
assert(first.memory.player_count == 4u);
assert(first.memory.player[0] == CMD_HEAL);
assert(first.memory.player[1] == CMD_YOWL);
assert(first.memory.player[2] == CMD_SCRATCH);
assert(first.memory.player[3] == CMD_HEAL);
```

Build controlled `BattleState` fixtures and zero-noise arrays to assert profile multipliers, each memory response, normal/trickster repeat penalties, lethal scratch exemption, active-channel hiss exemption, and forbidden heal weight zero. Assert `aiPolicyTickets()` totals exactly 10000, every legal action has at least one ticket, forbidden actions have zero, and the largest ticket count is at most 3500/5500/7000/8000/8500 for the five crisis tiers when multiple actions are legal. Save `battle.rng.state`, make 100 `aiBrainChooseNow()` calls, and assert the combat RNG state never changes.

- [ ] **Step 3: Run the focused test and verify RED**

Run:

```powershell
make -C tests/host test_ai_policy.exe
```

Expected: compilation fails because `AiBrain`, `AiProfile`, and `aiPolicyWeights` do not exist.

- [ ] **Step 4: Implement profile configuration and memory recording**

Place the six exact percentage rows from spec section 9.1 in a private `static const` table. `aiBrainInit()` must zero the complete struct, seed the independent RNG, and use the first `battleRngNext() % AI_PROFILE_COUNT` result for the uniform profile. `aiBrainRecordAccepted()` ignores `CMD_NONE`, invalid sides, and null brains; player actions shift the fixed array right, while accepted AI actions update `last_ai_action` and saturate `ai_repeat_count` at `UINT8_MAX`.

- [ ] **Step 5: Implement exact integer weights and memory modifiers**

Start legal commands at `max(score, 0) + 100`, then apply in order: profile percent, crisis-scaled additive memory percent, repeat percent, and supplied signed noise percent. Use saturating `uint64_t` helpers. Apply recency points `4,3,2,1`; scale the accumulated memory percent with:

```c
effective_percent = raw_percent * memory_use_percent(crisis) / 100;
```

Clamp hiss-induced yowl/heal suppression to 25% of the pre-memory weight. Exempt lethal scratch and hiss against `CHANNEL_YOWL`/`CHANNEL_HEAL` from repeat penalties.

- [ ] **Step 6: Implement deterministic 10,000-ticket normalization and cap**

Give each legal command one ticket, proportionally allocate the remaining tickets with integer division, then distribute division residue in command order `HISS`, `SCRATCH`, `YOWL`, `HEAL`. If the largest command exceeds `aiActionProbabilityCap(crisis) * 100`, remove the excess and redistribute it among the other legal commands by their pre-cap weights, again resolving residue in command order. With one legal command, assign it all 10,000 tickets.

- [ ] **Step 7: Implement seeded noise and roulette selection**

`aiBrainChooseNow()` generates one signed inclusive noise value per legal command using only `brain->rng`, calls the pure policy functions, copies tickets to `last_ticket`, draws one value in `0..9999`, and walks cumulative tickets in command order. It returns `CMD_NONE` if the side cannot act or no command is legal. Replace the body of `aiBestActionPercent()` with `aiActionProbabilityCap()` forwarding during this task so existing callers remain green.

- [ ] **Step 8: Run policy and existing AI tests GREEN**

Run:

```powershell
make -C tests/host test_ai_policy.exe test_ai.exe
./tests/host/test_ai_policy.exe
./tests/host/test_ai.exe
```

Expected: both executables exit 0 with `/W4 /WX`; policy distributions are deterministic and combat RNG isolation passes.

- [ ] **Step 9: Commit Task 1**

```powershell
git add include/ai.h include/ai_internal.h source/ai/ai.c source/ai/ai_policy.c tests/host/test_ai_policy.c tests/host/Makefile
git commit -m "feat: add adaptive AI policy and memory"
```

### Task 2: Opening Patience and Observation Scheduler

**Files:**
- Modify: `include/ai.h`
- Create: `source/ai/ai_brain.c`
- Create: `tests/host/test_ai_brain.c`
- Modify: `tests/host/Makefile`

**Interfaces:**
- Consumes: `AiBrain`, `aiBrainChooseNow()`, `aiBrainRecordAccepted()`, and the profile configuration from Task 1.
- Extends `AiBrain` with `opening_frames_remaining`, `observe_frames_remaining`, `opening_choice_made`, `opening_waiting`, and `observing`.
- Produces:

```c
enum {
    AI_OPENING_PATIENCE_FRAMES = 120,
    AI_MAX_OBSERVE_FRAMES = 36
};

typedef struct AiDebugSnapshot {
    AiProfile profile;
    BattleCommand player_history[AI_MEMORY_CAPACITY];
    uint8_t player_history_count;
    uint16_t opening_frames_remaining;
    uint16_t observe_frames_remaining;
    uint16_t ticket[CMD_HEAL + 1];
} AiDebugSnapshot;

BattleCommand aiBrainTick(AiBrain *brain, const BattleState *battle,
                          Side side, uint8_t crisis);
void aiBrainSnapshot(const AiBrain *brain, AiDebugSnapshot *snapshot);
```

- [ ] **Step 1: Add and register `test_ai_brain.exe`**

Compile it against `ai.c`, `ai_policy.c`, `ai_brain.c`, `battle_core.c`, and `battle_rng.c`, add it to `TESTS`, and add a `test_ai_brain` phony runner.

- [ ] **Step 2: Write RED scheduler tests**

After `aiBrainInit()`, tests may assign `brain.profile` directly to cover every profile without a test-only production API. Cover these exact behaviors:

```c
for (unsigned frame = 0; frame < 119; ++frame)
    assert(aiBrainTick(&brain, &battle, SIDE_AI, 1u) == CMD_NONE);
assert(snapshot.opening_frames_remaining == 1u);
assert(aiBrainTick(&brain, &battle, SIDE_AI, 1u) == CMD_NONE);
assert(snapshot.opening_frames_remaining == 0u);
```

Use seeds that exercise both waiting and immediate yowl. Record an accepted player action during waiting and assert the opening timer becomes zero immediately. For each profile, sample 1,000 armed delays and assert every value is inside its exact inclusive range and never above 36. Assert pause leaves both counters byte-for-byte unchanged, stun/cooldown/finished cancel `observing`, a zero-frame observation may choose on the same eligible frame, an N-frame observation emits no action during the N waiting frames, and battle reinitialization clears history and repetition.

- [ ] **Step 3: Run scheduler test and verify RED**

Run:

```powershell
make -C tests/host test_ai_brain.exe
```

Expected: compilation fails because `aiBrainTick()` and timing fields are missing.

- [ ] **Step 4: Implement the opening state machine**

Initialize `opening_frames_remaining` to 120. On the first eligible unpaused tick, consume one AI RNG roll against the selected profile's exact yowl probability `20/30/70/35/45/50`. Return `CMD_YOWL` and finish opening when chosen; otherwise mark waiting. While waiting, decrement once per unpaused frame and never return hiss, scratch, or heal. Extend `aiBrainRecordAccepted()` so any accepted player action ends waiting immediately; accepted AI yowl also finishes opening.

- [ ] **Step 5: Implement observation arming, freezing, and cancellation**

After opening, when the fighter first becomes actionable, draw an inclusive delay from its profile range. A zero draw calls `aiBrainChooseNow()` immediately. A positive draw sets `observing`; each later eligible unpaused tick decrements once, and selection occurs only after all sampled waiting frames elapsed. Pause returns `CMD_NONE` without mutation. Cooldown, stun, death, or `battle.finished` clears `observing` and the delay; the next eligible tick draws a fresh delay.

- [ ] **Step 6: Implement read-only snapshots**

`aiBrainSnapshot()` copies profile, history, timing, and last tickets without consuming RNG or mutating the brain. Null input produces a zeroed snapshot with `AI_PROFILE_AGGRESSIVE` as the safe enum value.

- [ ] **Step 7: Run focused and full AI tests GREEN**

Run:

```powershell
make -C tests/host test_ai_policy.exe test_ai_brain.exe test_ai.exe
./tests/host/test_ai_policy.exe
./tests/host/test_ai_brain.exe
./tests/host/test_ai.exe
```

Expected: all three exit 0; exact 120-frame and inclusive delay boundary assertions pass.

- [ ] **Step 8: Commit Task 2**

```powershell
git add include/ai.h source/ai/ai_brain.c tests/host/test_ai_brain.c tests/host/Makefile
git commit -m "feat: add AI opening patience and timing"
```

### Task 3: Battle Integration, Hidden Debug Telemetry, and ROM Verification

**Files:**
- Modify: `include/ai.h`
- Modify: `include/battle_scene.h`
- Modify: `include/battle_scene_internal.h`
- Modify: `include/game_terms.h`
- Modify: `source/ai/ai.c`
- Modify: `source/ui/battle_scene.c`
- Modify: `source/graphics/battle_animation.c`
- Modify: `source/ui/debug_scene_view.c`
- Modify: `source/main.c`
- Modify: `source/localization/strings_zh_cn.c`
- Modify: `source/localization/strings_en.c`
- Modify: `tests/host/test_ai.c`
- Modify: `tests/host/test_battle_scene_policy.c`
- Modify: `tests/host/test_battle_animation.c`
- Modify: `tests/host/test_debug_scene_draw.c`
- Modify: `tests/host/Makefile`
- Regenerate: `assets/fonts/required_glyphs.txt`
- Regenerate: `assets/fonts/jimidou_subset.ttf`
- Regenerate: `assets/fonts/jimidou_font_atlas.png`
- Regenerate: `include/generated/jimidou_font_metrics.h`
- Regenerate: `nitrofs/fonts/jimidou_font.a5i3.bin`

**Interfaces:**
- Consumes: `AiBrain`, `aiBrainTick()`, `aiBrainRecordAccepted()`, and `aiBrainSnapshot()` from Tasks 1–2.
- Extends `BattleSetup` with `bool debug_ai`.
- Produces this host-testable submission boundary:

```c
bool battleSceneSubmit(BattleState *battle, BattleSceneRouteState *route,
                       AiBrain *brain, Side side, BattleCommand command,
                       BattlePresentation *presentation);
```

- Extends `battleAnimationDraw()` with nullable `const AiDebugSnapshot *debug_ai`; null means no telemetry and is mandatory for normal battles.

- [ ] **Step 1: Write RED scene-submission tests**

In `test_battle_scene_policy.c`, initialize a real battle, route, and brain. Assert a rejected player command does not enter history; an accepted player command appears at history index zero and ends opening waiting; an accepted AI command updates repetition; deferred scratch still returns no immediate presentation; and all existing route ordering assertions remain unchanged.

- [ ] **Step 2: Write RED hidden/debug rendering tests**

Add localized IDs for six profile names plus profile, memory, observation, and probability-cap labels. In `test_battle_animation.c`, capture text draws and assert a null snapshot draws none of those labels. Pass a snapshot with profile, `HISS/SCRATCH/YOWL/HEAL` history, delay 12, and literal tickets; assert the debug-only panel draws localized profile text and numeric telemetry. Update `test_debug_scene_draw.c` so the existing `55%` crisis-25 value is described and tested as an action probability cap, not a fixed best-choice rate.

- [ ] **Step 3: Run scene/render tests and verify RED**

Run:

```powershell
make -C tests/host test_battle_scene_policy.exe test_battle_animation.exe test_debug_scene_draw.exe
```

Expected: compilation fails on the missing submission API, snapshot draw parameter, and localization IDs.

- [ ] **Step 4: Extract the real accepted-submission boundary**

Move `battleSubmit()` plus deferred-scratch detection and `battleSceneRouteSubmitted()` into `battleSceneSubmit()` outside `#ifdef __NDS__`. Zero the output presentation first. Record into `AiBrain` only after `battleSubmit()` returns true. Keep the NDS-only wrapper responsible solely for animation/audio presentation.

- [ ] **Step 5: Replace stateless NDS decisions with one `AiBrain`**

Initialize the brain from `setup->seed` beside battle initialization. Each unpaused frame: submit the player command, call `aiBrainTick()`, submit its returned command, then run `battleTick()` exactly once. Remove `aiChoose()` and its old best/non-best arrays after all callers use the brain. Quick START setup sets `debug_ai=false`; L+START Debug setup sets `debug_ai=true`.

- [ ] **Step 6: Render telemetry only for Debug-launched battles**

Before drawing, call `aiBrainSnapshot()` only when `setup->debug_ai`. Pass null otherwise. Draw a compact three-line panel at the bottom of the top screen after fighters/captions and before the pause overlay: localized profile and observation frames, `L/R/Y/A` ticket counts, and the four remembered action keys newest first (`-` for empty slots). Use `FONT_SCALE_HALF`; normal battle rendering must be pixel-for-pixel unchanged outside regenerated font data.

- [ ] **Step 7: Update localization and regenerate the subset font**

Populate every new `GameTextId` in both tables, rename `TEXT_DEBUG_BEST_RATE` to `TEXT_DEBUG_ACTION_CAP`, and run the existing deterministic font chain with bundled `assets_src/fonts/FZG_CN.ttf`:

```powershell
C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && env PATH=/opt/wonderful/bin:/opt/wonderful/toolchain/gcc-arm-none-eabi/bin:/usr/bin:/bin TEMP=/c/Desktop/JiMiDoo/build/tmp TMP=/c/Desktop/JiMiDoo/build/tmp BLOCKSDS=/opt/wonderful/thirdparty/blocksds/core WONDERFUL_TOOLCHAIN=/opt/wonderful PYTHON=/c/Users/lhf19/AppData/Local/Programs/Python/Python312/python.exe make -B font-runtime-assets"
```

- [ ] **Step 8: Run focused tests GREEN**

Run:

```powershell
make -C tests/host test_ai_policy.exe test_ai_brain.exe test_ai.exe test_battle_scene_policy.exe test_battle_animation.exe test_debug_scene_draw.exe
./tests/host/test_ai_policy.exe
./tests/host/test_ai_brain.exe
./tests/host/test_ai.exe
./tests/host/test_battle_scene_policy.exe
./tests/host/test_battle_animation.exe
./tests/host/test_debug_scene_draw.exe
```

Expected: all six exit 0; normal visibility, debug telemetry, accepted-action memory, and timing tests pass.

- [ ] **Step 9: Run the complete host regression suite**

Run:

```powershell
C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && env TEMP='C:/Desktop/JiMiDoo/build/tmp' TMP='C:/Desktop/JiMiDoo/build/tmp' make host-test"
```

Expected: every C executable, deterministic font/resource test, ROM contract, host-tool discovery check, and audio-format check exits 0.

- [ ] **Step 10: Force a clean Wonderful ROM build**

Run:

```powershell
C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && env PATH=/opt/wonderful/bin:/opt/wonderful/toolchain/gcc-arm-none-eabi/bin:/usr/bin:/bin TEMP=/c/Desktop/JiMiDoo/build/tmp TMP=/c/Desktop/JiMiDoo/build/tmp BLOCKSDS=/opt/wonderful/thirdparty/blocksds/core WONDERFUL_TOOLCHAIN=/opt/wonderful PYTHON=/c/Users/lhf19/AppData/Local/Programs/Python/Python312/python.exe make -B -j2 PussiFight.nds"
```

Expected: ARM9 compilation, link, NitroFS/font generation, and `NDSTOOL PussiFight.nds` exit 0. Record ROM byte size and SHA-256.

- [ ] **Step 11: Emulator acceptance check**

Launch the new ROM in the user's emulator and verify: normal START never displays personality; L+START Debug battle does; AI waits or yowls during the first two seconds; player first action releases a waiting AI; action timing varies; repeated player patterns alter behavior without changing combat rule outcomes; pause freezes AI timers.

- [ ] **Step 12: Commit Task 3 without unrelated assets**

Stage only the files named in this task. Confirm `git diff --cached --name-only` contains no untracked cat action-sheet, `pixel64`, revision, converter, or converter-test paths, then commit:

```powershell
git commit -m "feat: add adaptive real-time battle AI"
```
