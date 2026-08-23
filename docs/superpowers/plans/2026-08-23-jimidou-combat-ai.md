# 基米斗 Combat and AI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在阶段一 ROM 上实现全部战斗规则、实时电脑 AI、双屏战场以及触摸与实体键控制。

**Architecture:** `battle_core.c` 是不依赖 libnds 的确定性状态机，所有随机数由注入的 RNG 提供；AI 只输出行动意图。NDS 场景把按键/触摸转换为同一种命令，再根据战斗事件驱动动画和音效。

**Tech Stack:** C11、主机 GCC、BlocksDS/libnds、GL2D、Maxmod

**Spec:** `docs/superpowers/specs/2026-08-23-jimidou-design.md`

## Global Constraints

- 60 FPS 固定更新；0.5 秒=30 帧、0.7 秒=42 帧、1 秒=60 帧、2 秒=120 帧。
- 抓挠是唯一直接伤害；成功哈气不启动自身 CD；CD 期间输入不缓存。
- 闪避只针对抓挠，闪避后不打断持续行动；抓挠命中打断持续行动但不清怒。
- AI 普通成长不改变玩家 30% 基础预警，也不绕过既定公式。
- 任一方生命归零立即停止该帧剩余结算；START 暂停所有战斗计时。

---

## File Map

- `include/battle.h`, `source/battle/battle_core.c`：状态、命令、事件和逐帧结算。
- `include/battle_rng.h`, `source/battle/battle_rng.c`：可播种 RNG。
- `include/ai.h`, `source/ai/ai.c`：评分与危机分段决策。
- `source/ui/battle_scene.c`, `source/ui/battle_hud.c`：输入、上下屏和暂停。
- `source/graphics/battle_animation.c`：单图程序动画。
- `tests/host/test_battle.c`, `tests/host/test_ai.c`：规则回归测试。

### Task 1: Battle State, Timing, Channels, and Damage

**Files:**
- Create: `include/battle.h`, `include/battle_rng.h`
- Create: `source/battle/battle_core.c`, `source/battle/battle_rng.c`
- Create: `tests/host/test_battle.c`

**Interfaces:**
- Produces: `FighterSpec { max_hp, attack, rage_per_tick, heal_per_tick, rage_cap, action_cd_frames, dodge_percent, warning_percent, counter_percent }`; `FighterState` containing mutable HP/rage/timers/channel; `BattleState`, `BattleCommand { CMD_NONE, CMD_HISS, CMD_SCRATCH, CMD_YOWL, CMD_HEAL }`, `BattleEvent`; `battleInit(BattleState*, const FighterSpec*, const FighterSpec*, uint32_t seed)`, `bool battleSubmit(BattleState*, Side, BattleCommand)`, `size_t battleTick(BattleState*, BattleEvent*, size_t)`.

- [ ] **Step 1: Write failing timing and damage tests**

```c
battleInit(&b, &orange, &tabby, 1);
assert(battleSubmit(&b, SIDE_PLAYER, CMD_YOWL));
tick(&b, 30); assert(b.fighter[SIDE_PLAYER].rage == 5);
tick(&b, 30); assert(b.fighter[SIDE_PLAYER].rage == 10);
b.fighter[SIDE_PLAYER].cooldown = 0;
assert(battleSubmit(&b, SIDE_PLAYER, CMD_SCRATCH));
assert(b.fighter[SIDE_AI].hp == 65 - (15 + 2));
```

Also assert gum heals at frames 60/120, caps at max HP, CD counts during stun, command during CD returns false, and pause prevents every counter from changing.

- [ ] **Step 2: Run and verify missing battle API**

Run: `C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && make -C tests/host test_battle"`

Expected: FAIL because `battle.h` is missing.

- [ ] **Step 3: Implement the minimal deterministic state machine**

Use frame counters and `Channel { NONE, YOWL, HEAL }`. At accepted non-hiss actions, end the actor's previous channel and assign the actor's configured global CD. `battleTick` decrements CD/stun, advances channel accumulators, caps values, and returns ordered events in the caller buffer without heap allocation. Damage is `attack + (rage / 10) * 2`. Stop processing immediately after emitting `EVENT_BATTLE_END`.

- [ ] **Step 4: Run battle timing tests**

Run: `C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && make -C tests/host test_battle"`

Expected: all timing, pause, channel and damage assertions PASS.

- [ ] **Step 5: Commit**

```bash
git add include/battle* source/battle tests/host
git commit -m "feat: add deterministic battle state machine"
```

### Task 2: Hiss, Counter, Warning, Dodge, and Interrupt Rules

**Files:**
- Modify: `include/battle.h`, `source/battle/battle_core.c`, `tests/host/test_battle.c`

**Interfaces:**
- Extends: `BattleEventType` with `WARNING`, `HISS_SUCCESS`, `HISS_FAIL`, `DODGE`, `HIT`, `CHANNEL_STOP`, `STUN`.
- Produces: injectable `uint16_t (*BattleRandom)(void *ctx, uint16_t upper_exclusive)` in `BattleState` for exact probability tests.

- [ ] **Step 1: Add table-driven failing tests**

```c
assert(counterPercent(0) == 40);
assert(counterPercent(10) == 10);
assert(counterPercent(14) == 0);
force_roll(79); assert(hiss_channel_succeeds(&b));
force_roll(80); assert(!hiss_channel_succeeds(&b));
```

Add scenarios proving: warning lasts exactly 42 frames; hiss in that window is 100%; successful hiss clears target rage/stuns 120 frames and leaves actor CD zero; failed/irrelevant hiss starts CD; dodge prevents damage and preserves the target channel; hit stops channel but preserves target rage; an immediate player scratch counter cancels damage.

- [ ] **Step 2: Confirm tests fail on missing rule branches**

Run: `C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && make -C tests/host test_battle"`

Expected: FAIL first on `counterPercent` or warning state.

- [ ] **Step 3: Implement rules in one resolution order**

For player scratch: immediate-counter roll, then dodge, then hit. For AI scratch: warning roll; warned attack enters `pending_scratch_frames=42`, otherwise resolves immediately. For hiss: resolve pending scratch first at 100%, else active channel at 80%, otherwise fail. Use integer roll `0..99`; clamp counter probability at zero and dodge/warning at their configured caps.

- [ ] **Step 4: Run all battle regression tests**

Run: `C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && make -C tests/host test_battle"`

Expected: every exact boundary and event-order assertion PASS.

- [ ] **Step 5: Commit**

```bash
git add include/battle.h source/battle/battle_core.c tests/host/test_battle.c
git commit -m "feat: implement battle reactions and interrupts"
```

### Task 3: Crisis-Aware Real-Time AI

**Files:**
- Create: `include/ai.h`, `source/ai/ai.c`, `tests/host/test_ai.c`

**Interfaces:**
- Consumes: read-only `BattleState` and `Side`.
- Produces: `AiScores aiScoreActions(const BattleState*, Side)`, `BattleCommand aiChoose(const BattleState*, Side, uint8_t crisis, BattleRandom, void*)`.

- [ ] **Step 1: Write failing score and randomness tests**

```c
AiScores s = aiScoreActions(&b, SIDE_AI);
assert(s.score[CMD_HEAL] == AI_SCORE_FORBIDDEN); /* full HP */
b.fighter[SIDE_PLAYER].channel = CHANNEL_YOWL;
s = aiScoreActions(&b, SIDE_AI);
assert(s.score[CMD_HISS] > s.score[CMD_YOWL]);
b.fighter[SIDE_PLAYER].hp = 5;
s = aiScoreActions(&b, SIDE_AI);
assert(s.score[CMD_SCRATCH] == maxAllowedScore(s));
```

Run 1000 seeded selections at crisis 255 and assert non-best selections are between 15% and 22%; at crisis 1 assert all four legal actions appear.

- [ ] **Step 2: Verify missing AI API failure**

Run: `C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && make -C tests/host test_ai"`

Expected: FAIL because `ai.h` is missing.

- [ ] **Step 3: Implement scoring and tier selection**

Use named integer weights in `game_config.h`: lethal scratch, interrupt active channel, low-rage yowl, missing-HP heal, and threat suppression. Choose the highest score with probabilities 35%, 55%, 70%, 80%, 85% for crisis bands 1–24, 25–74, 75–149, 150–224, 225–255; otherwise uniformly choose a legal non-best command. Never return heal at full HP or any command while the fighter is unavailable.

- [ ] **Step 4: Run deterministic AI tests**

Run: `C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && make host-test"`

Expected: battle and AI suites PASS with reproducible seed output.

- [ ] **Step 5: Commit**

```bash
git add include/ai.h include/game_config.h source/ai tests/host/test_ai.c tests/host/Makefile
git commit -m "feat: add crisis-scaled combat AI"
```

### Task 4: NDS Battle Scene, Controls, HUD, and Program Animation

**Files:**
- Create: `include/battle_scene.h`
- Create: `source/ui/battle_scene.c`, `source/ui/battle_hud.c`
- Create: `source/graphics/battle_animation.c`
- Modify: `source/main.c`
- Create: `tests/host/test_touch_layout.c`

**Interfaces:**
- Consumes: `battleSubmit`, `battleTick`, `aiChoose`, audio and cat texture services.
- Produces: `BattleSetup { FighterSpec player, FighterSpec enemy, CatId player_cat, CatId enemy_cat, uint8_t crisis, uint32_t seed }`, `BattleResult { BATTLE_PLAYER_WIN, BATTLE_PLAYER_DEAD, BATTLE_ABORTED }`, `BattleResult battleSceneRun(const BattleSetup*)` and pure `BattleCommand touchCommandAt(int x, int y)`.

- [ ] **Step 1: Write exact touch-boundary tests**

Define lower screen rows: status `y=0..63`, buttons `y=64..191`; columns split at `x=128`, rows at `y=128`. Assert centers map `L/Hiss`, `R/Scratch`, `Y/Laowu Yowl`, `A/Chew Gum`, and status/out-of-range coordinates return `CMD_NONE`.

- [ ] **Step 2: Run and verify layout API failure**

Run: `C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && make -C tests/host test_touch_layout"`

Expected: FAIL because `touchCommandAt` is undefined.

- [ ] **Step 3: Implement unified input and rendering**

Map `KEY_L/R/Y/A` and touch rectangles into one command path. Draw player/enemy HP and rage in the upper third of the lower screen; draw four equal buttons below, darken during CD, display frame-derived tenths of seconds, and lock during stun. On top, draw mirrored opponents and consume events for 6–12 frame translations, shake, flash, afterimage, scale, particles, damage numbers, dodge text and warning icon. Load exactly the selected two cats' 14 textures before battle.

- [ ] **Step 4: Add pause and full debug match**

START toggles a localized pause overlay and prevents both `battleTick` and AI decisions. In the temporary title demo, START launches orange vs seeded tabby crisis 1; victory/defeat returns to title. Route every action event to the audio service.

- [ ] **Step 5: Verify on host and melonDS, then commit**

Run: `C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && make host-test && make -j2"`

Expected: all tests PASS; on melonDS, touch and physical keys trigger identical actions, successful hiss immediately re-enables action, pause freezes warning/CD/channel timers, and a match reaches a correct result.

```bash
git add include source tests
git commit -m "feat: add real-time NDS battle scene"
```
