# 基米斗 Progression, Save, and Release Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将可玩战斗扩展为含 1–255 危机、51 个固定特化敌人、永久增益、可靠存档及完整菜单流程的首版游戏。

**Architecture:** 进度模块用纯函数生成挑战、敌方最终属性和奖励；存档模块序列化固定宽度磁盘结构并通过后端接口隔离文件系统。场景控制器只保存场景状态和一次生成的战前敌人，战斗、成长与 IO 之间通过值对象交接。

**Tech Stack:** C11、主机 GCC、BlocksDS FAT/NitroFS、CRC32、libnds/GL2D、melonDS

**Spec:** `docs/superpowers/specs/2026-08-23-jimidou-design.md`

## Global Constraints

- 普通危机公式、地位跳升、挑战范围和八种增益严格按规格；危机和地位为 `uint8_t` 但计算先提升到至少 32 位。
- 每 5 级是固定特化节点，含 255，共 51 个；战前完整显示最终值、强化、削弱、反制与预警变化。
- 增益效果不递减；抽取权重为 `基础权重/(层数+1)`；候选三种互不重复。
- 闪避 80%、CD 0.5 秒、危险预知 100% 达上限后退出池；其他层数使用 32 位并可继续叠加。
- 存档含 magic/version/CRC，临时写入、正式替换、备份恢复；写失败不阻止游玩。

---

## File Map

- `source/progression/enemy.c`, `boss_nodes.c`：普通成长和 51 个固定节点。
- `source/progression/rewards.c`：挑战范围、地位和加权三选一。
- `source/save/save.c`, `save_codec.c`：内存状态、版本化编码和原子文件策略。
- `source/ui/*_scene.c`, `scene_flow.c`：标题、菜单、选择、情报、奖励、规则、设置、制作人员。
- `tests/host/test_progression.c`, `test_save.c`, `test_content.c`：全部边界与内容覆盖。
- `docs/qa/nds-release-checklist.md`：模拟器和真机验收记录。

### Task 1: Crisis Growth and 51 Specialized Nodes

**Files:**
- Create: `include/progression.h`, `source/progression/enemy.c`, `source/progression/boss_nodes.c`
- Create: `tests/host/test_progression.c`, `tests/host/test_content.c`

**Interfaces:**
- Produces: `EnemySpec enemyGenerateNormal(CatId,uint8_t)`, `const BossNode *bossNodeFor(uint8_t)`, `EnemySpec enemyGenerateBoss(uint8_t)`, `bool crisisIsBoss(uint8_t)`.

- [ ] **Step 1: Write exact formula and node coverage tests**

```c
EnemySpec e = enemyGenerateNormal(CAT_ORANGE, 20);
assert(e.max_hp == 60 * 140 / 100);
assert(e.attack == 15 * 120 / 100);
assert(e.action_cd_frames == 120 - 4 * 2); /* 0.03s rounded to 2 frames */
assert(e.rage_per_tick == 6 && e.heal_per_tick == 17);
for (int level=5, count=0; level<=255; level+=5, ++count) {
    assert(crisisIsBoss((uint8_t)level));
    assert(bossNodeFor((uint8_t)level)->level == level);
    assert(count < 51);
}
```

- [ ] **Step 2: Run and verify missing progression API**

Run: `C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && make -C tests/host test_progression"`

Expected: FAIL because `progression.h` is missing.

- [ ] **Step 3: Implement growth and explicit boss table**

Represent 0.03 seconds as two frames consistently and clamp normal CD at 42 frames. Define 51 explicit `BossNode` entries with level, cat, one/two template IDs, signed modifiers, counter delta, warning delta, and localized title/description IDs. Cycle the eight approved archetypes through early nodes, use paired hybrids from level 50 onward, and make level 255 a named final hybrid. Each entry must contain at least one positive and one negative modifier; tests reject duplicates or missing localized IDs.

- [ ] **Step 4: Run progression/content tests**

Run: `C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && make host-test"`

Expected: formulas at levels 1, 5, 20, 254, 255 PASS; exactly 51 boss records pass invariants.

- [ ] **Step 5: Commit**

```bash
git add include/progression.h source/progression tests/host source/localization
git commit -m "feat: add crisis growth and boss nodes"
```

### Task 2: Challenge Range, Status, and Weighted Permanent Buffs

**Files:**
- Modify: `include/progression.h`
- Create: `source/progression/rewards.c`
- Modify: `tests/host/test_progression.c`

**Interfaces:**
- Produces: `BuffId` in the fixed order HP/attack/CD/rage/heal/dodge/warning/rage-cap; `CatProgress { uint8_t status; uint32_t buff[BUFF_COUNT]; }`; `ChallengeRange { uint8_t first, last; }`; RNG callback `uint32_t (*ProgressRandom)(void*,uint32_t upper_exclusive)`; `ChallengeRange challengeRange(uint8_t status)`, `uint16_t rewardChoiceCount(uint8_t status,uint8_t crisis)`, `void rewardDrawThree(const CatProgress*,ProgressRandom,void*,BuffId out[3])`, `void rewardApply(CatProgress*,BuffId)`.

- [ ] **Step 1: Add failing boundary tests**

```c
assertRange(challengeRange(0), 1, 5);
assertRange(challengeRange(11), 12, 15);
assertRange(challengeRange(254), 255, 255);
assertRange(challengeRange(255), 255, 255);
assert(rewardChoiceCount(11, 15) == 5);
assert(rewardChoiceCount(255, 255) == 1);
```

Add 10,000 seeded draws proving three IDs are distinct, a 10-layer buff appears less often than a 0-layer buff, capped dodge/CD/warning never appears, and applying buffs yields exact per-layer values.

- [ ] **Step 2: Confirm reward tests fail**

Run: `C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && make -C tests/host test_progression"`

Expected: FAIL at `challengeRange` or `rewardDrawThree`.

- [ ] **Step 3: Implement integer weighted sampling**

Use base weight 840 and effective weight `840/(layers+1)` so common low layers retain integer precision. Sample without replacement by setting the selected item's temporary weight to zero. Remove dodge at 40 layers, CD when computed frames reach 30, and warning at 14 layers from the 30% base. Store all eight layers as `uint32_t`; compute derived stats in saturating 64-bit arithmetic then clamp to battle field limits.

- [ ] **Step 4: Run deterministic reward tests and commit**

Run: `C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && make host-test"`

Expected: all range, weighting, distinctness, cap and overflow tests PASS.

```bash
git add include/progression.h source/progression/rewards.c tests/host/test_progression.c
git commit -m "feat: add status challenges and permanent buffs"
```

### Task 3: Versioned Save Codec and Recovery

**Files:**
- Create: `include/save.h`, `source/save/save.c`, `source/save/save_codec.c`, `source/save/crc32.c`
- Create: `tests/host/test_save.c`, `tests/host/fixtures/save_v1.bin`

**Interfaces:**
- Produces: `GameSave saveDefaults(void)`, `bool saveEncode(const GameSave*,uint8_t*,size_t,size_t*)`, `SaveDecodeResult saveDecode(...)`, `SaveLoadResult saveLoad(SaveBackend*,GameSave*)`, `bool saveStore(SaveBackend*,const GameSave*)`, `void saveResetCat(GameSave*,CatId)`.

- [ ] **Step 1: Write codec, isolation, and corruption tests**

```c
GameSave s = saveDefaults();
s.cat[CAT_TABBY].status = 77; s.cat[CAT_TABBY].buff[BUFF_HP] = 9;
assert(roundTrip(&s, &decoded));
assert(decoded.cat[CAT_TABBY].status == 77);
saveResetCat(&decoded, CAT_ORANGE);
assert(decoded.cat[CAT_TABBY].status == 77);
encoded[20] ^= 0x80;
assert(saveDecode(encoded, size, &decoded) == SAVE_BAD_CRC);
```

Use an in-memory fake backend to assert write order `tmp`, rotate main to backup, rename tmp to main; simulate main corruption and verify backup loads; simulate every write failure and verify gameplay state remains in memory with `SAVE_IO_ERROR`.

- [ ] **Step 2: Run and verify missing save API**

Run: `C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && make -C tests/host test_save"`

Expected: FAIL because `save.h` is missing.

- [ ] **Step 3: Implement a fixed little-endian disk format**

Write fields explicitly, never dump C structs: 8-byte magic `JIMIDOO\0`, `uint16 version=1`, payload length, five records of status plus eight `uint32` layers, language, music volume, SFX volume, then CRC32 over header+payload excluding CRC. Reject wrong sizes and unknown future versions; migrate the committed v1 fixture through a version switch. On NDS, mount FAT and use `jimidou.sav.tmp`, `jimidou.sav`, `jimidou.sav.bak`; flush and close before each rename.

- [ ] **Step 4: Run save tests under sanitizers**

Run: `C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && make -C tests/host SANITIZE=1 test_save"`

Expected: round-trip, CRC, v1 fixture, recovery, cat isolation and failure injection PASS with no sanitizer report.

- [ ] **Step 5: Commit**

```bash
git add include/save.h source/save tests/host
git commit -m "feat: add recoverable versioned save system"
```

### Task 4: Complete Scene Flow and Prebattle Disclosure

**Files:**
- Create: `include/scene_flow.h`
- Create: `source/ui/scene_flow.c`, `main_menu_scene.c`, `cat_select_scene.c`, `crisis_select_scene.c`, `prebattle_scene.c`, `reward_scene.c`, `rules_scene.c`, `settings_scene.c`, `credits_scene.c`
- Modify: `source/ui/title_scene.c`, `source/main.c`
- Create: `tests/host/test_scene_flow.c`

**Interfaces:**
- Produces: explicit `SceneId` transition table and `GeneratedChallenge { uint8_t crisis; CatId enemy_cat; bool specialized; EnemySpec enemy; uint32_t seed; }` retained from crisis selection through prebattle and battle.

- [ ] **Step 1: Write transition and enemy-stability tests**

Assert title→menu; menu branches; cat→crisis→prebattle→battle; win performs exactly N reward selections then save→crisis; death resets selected cat then save→cat; backing out of prebattle and returning preserves the generated enemy; changing crisis or completing battle changes the seed.

- [ ] **Step 2: Run and verify scene controller failure**

Run: `C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && make -C tests/host test_scene_flow"`

Expected: FAIL because `scene_flow.h` is missing.

- [ ] **Step 3: Implement all scenes using localized IDs**

Display five always-available cats with base+buffed stats; constrain crisis selector to `challengeRange`; retain a generated enemy object in the controller. Prebattle shows actual HP/attack/CD/rage/heal/rage cap/dodge/counter/warning and explicit positive/negative boss modifiers. Reward scene draws three cards for every opportunity and saves only after all choices. Settings switches language immediately and adjusts separate music/SFX volumes; rules reproduce all six combat rules; credits list project and source acknowledgements.

- [ ] **Step 4: Connect death and victory persistence**

On victory compute reward count from pre-win status, apply all selections, then set status to defeated crisis and save. On death call `saveResetCat` for only the active cat and save immediately. Keep a localized persistent banner after write failure while permitting scene transitions.

- [ ] **Step 5: Verify host flow and commit**

Run: `C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && make host-test && make -j2"`

Expected: all transition tests PASS and ROM completes title→battle→reward/death paths.

```bash
git add include source/ui source/main.c tests/host
git commit -m "feat: add complete game scene flow"
```

### Task 5: Content Audit, Emulator, and Hardware Release Gate

**Files:**
- Create: `tools/audit_release.py`, `docs/qa/nds-release-checklist.md`
- Modify: `Makefile`, `.gitignore`

**Interfaces:**
- Produces: `make release-check`, a clean `PussiFight.nds`, and a signed-off QA checklist.

- [ ] **Step 1: Write the release auditor with concrete failures**

The script must fail unless: 35 processed cat images exist; 10 SFX and 2 streamed BGM exist; both text arrays populate every ID; required glyphs cover both tables; 51 boss nodes cover all multiples of 5; no source/UI file contains a Chinese/English literal outside localization tables; ROM exists and is below 64 MiB.

- [ ] **Step 2: Run audit before wiring the target**

Run: `C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && python3 tools/audit_release.py"`

Expected: FAIL on any unconnected or missing asset; fix only the reported concrete gaps and rerun until PASS.

- [ ] **Step 3: Add reproducible release target**

`release-check` runs host tests, asset audits, a clean NDS build, then prints ROM SHA-256 and size. It must not download assets or mutate `assets_src`; fetching remains an explicit developer action.

- [ ] **Step 4: Execute emulator checklist**

Record date/result for: clean boot, both languages, five cats, normal and boss crisis, all four touch/key actions, 0.7s warning, pause freeze, music loops/fades, mute fallback, victory buffs, death reset, save reload, corrupt-main backup recovery, and 30-minute battle soak in melonDS.

- [ ] **Step 5: Execute real hardware checklist**

On one DS/DS Lite flashcart and one DSi/3DS SD environment where available, record model/loader: stable 60 FPS, touch corners, L/R/Y/A, readable 128×128 cats, audio without underrun, DLDI/SD save across reboot, and write-failure banner. Any unavailable hardware row is marked `NOT RUN` and prevents declaring hardware release complete.

- [ ] **Step 6: Final build and commit**

Run: `C:\msys64\usr\bin\bash.exe -lc "cd /c/Desktop/JiMiDoo && make release-check"`

Expected: host tests and audit PASS; clean `PussiFight.nds` is built; checklist has no failed row.

```bash
git add tools/audit_release.py docs/qa Makefile .gitignore
git commit -m "test: add NDS release verification gate"
```
