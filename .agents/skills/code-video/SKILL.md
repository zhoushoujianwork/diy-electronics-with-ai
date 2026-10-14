---
name: code-video
description: "用代码直接做动画讲解视频：画面是 Canvas2D 绘制的时间纯函数，音效和配乐由代码合成，Playwright 逐帧截图加 ffmpeg 合成 mp4。用于给项目、功能、使用方式做介绍视频。触发：「做个动画视频」「代码动画 + 音效」「项目介绍视频」。不依赖 HyperFrames 或任何视频类技能。"
metadata:
  version: "0.1.0"
  source: "@global/code-video"
---

# code-video

画面是时间的纯函数，声音由代码合成，渲染是确定性的。不用素材库，不用视频类技能。

## Repository integration

Before creating media, read [repository integration](references/repository-integration.md)
for destination routing, prerequisites, still-image scope, and source provenance.

## 管线

1. 场景模块：每个场景导出 `dur`、`energy`、`cues`、`draw(ctx, t)`。
2. `timeline.js`：按顺序拼接场景，相邻场景交叉淡入 0.5s，统一给出全局时钟。
3. `tools/score.mjs`：汇总所有场景的 `cues` 并合成配乐，输出 48kHz 立体声 `score.wav`。
4. `tools/render.mjs`：Playwright 驱动系统 Chrome，批量并发截图，按帧序写入 ffmpeg stdin，输出 `picture.mp4`。
5. `tools/mux.mjs`：合成音画，用 ffprobe 校验流和时长。

目录结构（自行搭建）：
- `src/index.html`：画布入口，支持 `?scene=ID&t=秒` 单场景预览。
- `src/timeline.js`：场景顺序、交叉淡入和全局时钟，同时给渲染器和音频合成使用。
- `src/lib/`：缓动、绘制、带种子随机、音符等纯函数工具。
- `src/scenes/`：每个场景一个文件。
- `tools/`：`score.mjs`（音频）、`render.mjs`（画面）、`mux.mjs`（合成校验）、`stills.mjs`（单帧验收），以及 `lib/` 下的本地静态服务和合成器。

## 硬约束

1. `draw` 必须是纯函数：不用 `Date.now`、`Math.random`，不改模块级状态。随机用带种子的哈希，保证任意顺序渲染同一帧结果相同。
2. 画面与声音同源：事件时间写成常量，同时驱动绘制和 `cues`。打字用同一份 `typeSchedule`，每个可见字符对应一个 `click`。
3. 声音全部合成：振荡器加滤波噪声，不用采样。配乐用固定和弦循环，层次随 `energy` 增减。音效点要留空，不要每秒都响。
4. 每个场景铺满不透明背景，最终态停留不少于 1.5s，用于承接下一场景的淡入。
5. 事实只来自仓库文档。规划中的功能标「规划中」，虚构的示例标「示意」。不能把计划写成已实现。
6. 版式：中文用 PingFang SC，英文标识和命令用等宽字体。正文不小于 28px，两侧留 110px，不溢出、不遮挡。
7. 错误即停：页面报错就让渲染失败，不输出空白帧。不要用 `file://` 打开页面，Chrome 不允许 ES module，用本地回环静态服务。
8. 浏览器用系统 Chrome，通过 `playwright-core` 的 `executablePath` 启动，不下载浏览器。

## 验收（缺一不可）

- 每个场景至少截 6 个时间点（含 t≤0.8 和 dur-0.05），逐张打开看，再继续。
- 成片后抽 9 帧做联络图，再抽各场景收尾定格帧，逐张看。
- `ffprobe` 校验：有视频流和音频流，两者时长差不超过 0.25s。
- `ffmpeg -af ebur128=peak=true` 看响度和真峰值，真峰值应低于 -1 dBFS。
- 无法验证的要明说：不能听音频，所以只能证明数值正常，不能证明好听。

## 局限

- 中文字体依赖 macOS 自带字体，Linux 需另配。
- 80 秒、1080p、4 个并发约 10 分钟。
- 配乐是通用的循环和弦，不会因内容变化而换风格。
