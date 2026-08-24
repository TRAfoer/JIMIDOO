# 基米斗 / PussiFight

《基米斗》是一款面向 Nintendo DS 的即时猫咪对战游戏。玩家需要观察对手状态，在全局行动冷却中选择哈气、抓挠、老吴叫或嚼口香糖回血，与实时 AI 控制的愤怒猫咪战斗。

PussiFight is a real-time cat battle game for Nintendo DS. Read your opponent, manage the global action cooldown, and choose between hissing, scratching, yowling, and chewing gum to heal while fighting an AI-controlled angry cat.

## 操作 / Controls

- `L`：哈气 / Hiss
- `R`：抓挠 / Scratch
- `Y`：老吴叫、持续积攒怒气 / Yowl and build rage
- `A`：嚼口香糖回血 / Chew gum to heal
- 触摸下屏对应按钮也可执行行动 / Actions can also be selected on the touch screen
- 标题画面按 `START`：以危机等级 1 快速开战 / Press `START` on the title screen for a crisis-level 1 quick battle
- 标题画面按住 `L` 再按 `START`：进入危机等级 Debug 界面 / Hold `L` and press `START` to open the crisis-level debug menu

## 构建 / Build

项目使用 Wonderful Toolchain 与 BlocksDS 构建。安装并配置工具链后，可在 Wonderful shell 中运行：

```sh
make PussiFight.nds
```

The project is built with Wonderful Toolchain and BlocksDS. After configuring the toolchain, run the command above in the Wonderful shell.

## 生成与素材鸣谢 / Generation and Credits

本项目由 OpenAI Codex 生成，并在开发者的设计与指导下持续迭代。

部分猫咪素材及其表现形式化用自 bilibili UP 主 **@洪山桥小老板** 的素材。相关素材权利归原作者或相应权利人所有。

This project was generated with OpenAI Codex and iterated under the developer's design and direction.

Some cat assets and visual ideas are adapted from materials by bilibili creator **@洪山桥小老板**. The rights to the referenced materials remain with their original creator or respective rights holders.

## 项目状态 / Project Status

项目仍在开发中，游戏数值、AI、界面和素材可能继续调整。

This project is under active development. Balance, AI, interface, and assets may continue to change.
