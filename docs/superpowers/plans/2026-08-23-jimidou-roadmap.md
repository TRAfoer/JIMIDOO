# 基米斗 / PussiFight Implementation Roadmap

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 按三个可独立验收的阶段，将空目录建设为可在 melonDS 与真实 NDS/DSi 上运行、保存进度的完整《基米斗》。

**Architecture:** 先建立可构建 ROM、资源流水线与双语界面，再接入与渲染解耦的确定性战斗内核和 AI，最后加入 1–255 危机成长、永久增益、可靠存档与完整场景流程。每一阶段结束时都生成可运行的 `PussiFight.nds`，后续阶段只依赖前一阶段已公开的接口。

**Tech Stack:** C11、BlocksDS v1.21.1、Wonderful Toolchain、libnds、GL2D、Maxmod、NitroFS、Python 3 资源脚本、FFmpeg、主机端 GCC 测试

**Spec:** `docs/superpowers/specs/2026-08-23-jimidou-design.md`

## Global Constraints

- 目标 ROM 名称固定为 `PussiFight.nds`，ARM9 使用 BlocksDS，ARM7 使用预编译 `arm7_maxmod.elf`。
- 战斗固定 60 FPS，规则使用整数帧和整数/定点数；战斗帧内不分配内存、不读文件、不用浮点数。
- 所有可平衡数值集中在 `include/game_config.h`；自然语言只能通过 `textGet(GameTextId)` 获取。
- 大型纹理、字库和流式音乐放入 NitroFS；进入战斗前载入双方 14 张纹理。
- 五只猫始终可用并各自保存地位与八类增益；死亡只清空当前猫。
- 简体中文与英文可即时切换，最终以 256×192 显示效果验收。
- 现有 `Audios/` 是用户素材，不重命名、不删除、不覆盖；外部 FFmpeg 不复制进仓库。

---

## Ordered Plans

1. [`2026-08-23-jimidou-foundation-assets.md`](2026-08-23-jimidou-foundation-assets.md) — 可构建 ROM、资源处理、双语文本、图形/音频服务和标题演示。
2. [`2026-08-23-jimidou-combat-ai.md`](2026-08-23-jimidou-combat-ai.md) — 完整四行动规则、实时 AI、上下屏战场、触摸/按键与暂停。
3. [`2026-08-23-jimidou-progression-save-release.md`](2026-08-23-jimidou-progression-save-release.md) — 危机 1–255、51 个特化节点、增益、存档、全场景流程和发布验收。

## Stage Gates

- [ ] 阶段一：`make host-test assets PussiFight.nds` 全部成功；ROM 可进入中英双语标题演示并播放两类音频。
- [ ] 阶段二：确定性规则测试全部成功；ROM 可从调试入口完成一场玩家对 AI 的实时战斗。
- [ ] 阶段三：从标题到胜/负结算全流程可完成；存档损坏恢复测试、melonDS 流程和真机检查表全部通过。

阶段必须按顺序执行。每份计划内部按任务编号执行，不跨任务预先实现接口。
