# 嵌入式 UI 开发模式与 SquareLine 自动化调研

调研日期：2026-09-22。范围为官方文档、发布记录、公开源码和本项目源码阅读。
本轮没有安装或升级编辑器/MCP，没有执行导出、模拟器、测试、构建、烧录或声音播放。
下述方案是选型建议，不代表已经集成或通过验收。

## 决策

本项目推荐 **LVGL 同源实现 + 可视化布局工具 + 独立状态接口 + 静音桌面预览**。
可视化工具优先评估 SquareLine Studio 1.6.2：它已明确支持项目所用的 LVGL 9.5，
并提供命令行导出。发动机、排气等程序化绘图继续由共享 C 模块实现。

真正的交付物是可编译的 UI、字体/素材和状态契约；设计图用来表达视觉目标。
电脑预览与 ESP32 编译同一套页面和绘图代码，才能持续检查目标是否实现。
MCP 是让 AI 调用工具的接口，本身不保证设计还原度，也不能替代版本和显示配置对齐。

如果 SquareLine 的自动化适配成本高，直接编写组件化 LVGL C 是最稳妥的基础路线。
两条路线都落到相同的共享运行结构，无需为换编辑器重写音频核心或板级驱动。

## 为什么当前预览与实机差别大

当前整屏设计由 Pillow/HTML 表达，固件另用 LVGL 控件和简化几何实现；
两端的字体、抗锯齿、默认主题和部分素材不同。电脑工具目前只共用中间 320×108
绘图区，尚未共用完整 320×240 页面。具体差异见
[设计与实现约定](ui-implementation-contract.md)。

240×320 是竖屏像素排列，320×240 是横屏逻辑画布，不能仅凭顺序不同判定屏幕配置错误。
2 英寸、4:3 显示区估算约 40.6×30.5 mm，电脑放大的好看效果不能证明实物可读。
详见[显示档案](../../../boards/lckfb-szpi-esp32-s3-va/display.md)。

因此优先修正工作流和信息密度；目前没有证据表明更换 LVGL 就能解决问题。
版本不一致是需要消除的另一项风险，不能把未经对比的差异都归因于库缺陷。

## 主流模式比较

以下推荐程度针对本项目：ESP32-S3、2 英寸 320×240、LVGL 9.5.0、ESP-IDF、
较多自定义动态绘图、AI 参与开发，以及可复用的公开代码。

| 模式 | 优势 | 关键代价/限制 | 本项目建议 |
| --- | --- | --- | --- |
| 组件化 LVGL C + SDL 桌面端 | AI 可直接改源码；自定义绘图自由；电脑/固件能编译完全相同的 UI | 布局需要程序表达，缺少直接拖拽；应先拆开驱动与页面 | 作为基础架构与保底路线 |
| SquareLine → 生成 LVGL C → 共享桌面/板端 | 所见即所得布局，设计者易微调；官方 CLI 可自动导出；1.6.2 支持 LVGL 9.5 | 编辑器项目与手写扩展必须分清归属；无已核实的官方通用控件编辑 API | 优先评估的可视化入口 |
| EEZ Studio → LVGL C，可选 Flow | 编辑器开源，支持布局、主题、状态/事件流程；有命令行构建入口 | 要学习其工程和绑定体系；引入 Flow 会增加运行时与逻辑归属的选择 | 希望全开源可视化工具时的备选 |
| LVGL Pro XML → C/模拟器，可选 Figma Flow | 声明式文本适合 AI；官方提供校验、截图、对比等 CLI；Figma 可转实际组件/页面 | 免费 Community 不提供 CLI；版本、授权、生成文件归属需确认 | 有专业工具预算或专职设计流程时评估 |
| Figma/Pillow/HTML 出图，再独立手写 MCU UI | 概念探索自由 | 两套实现长期漂移；网页浏览器不是 LVGL 渲染器 | 仅作为概念与交互说明，不作为最终预览 |

LVGL 官方提供桌面 SDL 工程，支持 Windows/Linux/macOS；其价值是复用页面代码，
不是在电脑模拟完整 ESP32 硬件。[1] 桌面端应使用与板端一致的软件绘制路径、字体、
主题和颜色配置。桌面显示速度不代表 SPI LCD 的刷新性能。

EEZ Studio v0.29.0 的版本映射源码包含 LVGL 9.5.0 及对应 WASM 运行时；
入口源码包含 `--build-project`。这比笼统的“支持 9.x”更具体，但本项目尚未导入。[6][7]
编辑器 GPLv3 不等于生成固件必须 GPL；官方 README 明确用户拥有项目与生成代码，
Flow 相关代码采用 MIT，最终仍需逐项保留所用代码/素材的许可。[6]

LVGL Pro 发布记录已到 v2.0.4（2026-09-10），部分官网弹窗仍显示 1.2.1，
应以具体发布记录和对应文档为准。[8] 新版 Figma Flow 支持屏幕、组件、颜色、字体、
间距和部分真实 LVGL 控件，不应再概括为“只能同步图片和样式”。但它不是任意 Figma
效果的无损转换；控件支持表仍有限制，重新导出会重建它拥有的文件。[9]

Pro CLI 当前是 `@lvgl/lvglpro`，包括生成、编译、校验、截图和对比。
Community/Evaluation 均不含 CLI。定价页把 Growth 列为可用 CLI，而 CLI 认证文档
只写 Product/Platform token，具体可用套餐需厂商确认，不能把免费编辑器等同免费自动化。[10]

## SquareLine 能否由 MCP/API 操作

### 官方稳定入口：命令行导出

SquareLine Studio 1.5.4 和 1.6.2 均有正式 CLI 文档：[2]

- `-projectfile`：打开指定 `.spj`。
- `-exportfolder`：导出项目，已有目标输出可能被覆盖。
- `-ui_exportfolder`：更新指定 UI 目录；按文档要求先有项目导出目录。
- `-batchmode`、`-logFile`：批处理和日志相关选项；文档表述为尝试关闭 GUI，
  不等于已证明所有平台均能在无桌面环境的 CI 中运行。

需要先在 GUI 完成许可证设置，未设置时 CLI 同样会停在启动阶段。
本轮不读取账号凭据，不推断本机许可证类型，也未验证本机导出是否可用。
这些接口可以封装成 MCP，但它们主要管理打开/导出，不直接提供新增控件、改属性等设计 API。

在本轮查阅的官方文档中，没有找到公开、稳定的通用控件编辑 API 或官方 SquareLine MCP。
这是检索结论，不是对所有未公开接口存在性的断言。

### 已找到的第三方 MCP：修改工程文件

公开仓库：[samir-pro/SquareLine_mcp](https://github.com/samir-pro/SquareLine_mcp)。
静态阅读固定提交
[`6d81c7fba8e6ab3fe8c33b8f3e47fcc7aa8c56c1`](https://github.com/samir-pro/SquareLine_mcp/tree/6d81c7fba8e6ab3fe8c33b8f3e47fcc7aa8c56c1)，
包版本 0.1.0，MIT。未安装、运行或完成全面代码审计。

源码提供 `create_project`、`load_project`、`add_screen`、`add_widget`、
`set_property`、`set_style`、`add_event`、`export_project` 等 MCP 工具。
其 `export_project` 实际写 JSON `.spj` 并复制素材；**不是导出 LVGL C，也不直接控制
正在运行的 SquareLine App**。项目作者称支持部分工程往返编辑，本轮没有复现该声明。

默认是 CrowPanel、800×480、Arduino/TFT_eSPI、LVGL 8.3.11、SquareLine 1.4.2
格式。宽高可以改，但公开的创建接口未提供任意 LVGL/板级配置参数。
读入工程可保留已有信息，不代表新建控件/事件序列化已经兼容 LVGL 9.5 和编辑器 1.6.2。
因此它是可借鉴的适配起点，不是本项目即装即用的工具。只改版本字符串或宽高不够。

可靠的接入顺序应是：

1. 用目标版本的 SquareLine 创建最小 320×240、LVGL 9.5 工程，保留真实格式样本。
2. 在工程副本上适配 MCP 的板型信息、控件属性、事件、字体/图片引用与拆分屏幕文件。
3. 让 SquareLine 重新打开/保存，再由官方 CLI 导出，检查控件与生成 C 的变化。
4. 以导出 C 的完整 LVGL 桌面画面检查结果；不要用 MCP 自己生成的 HTML 代替。

这四步是后续实施建议，当前“停止测试”要求仍有效，本轮均未执行。

### 内部渲染服务与界面自动化

发行包内可见 Python `server.py`，实现编辑器与 LVGL 预览进程之间的 localhost
socket 通信，包含脚本、鼠标和像素数据传递。这是内部预览实现，未发现其被官方承诺为
公共编辑 API；改变预览对象不等于更新可保存的编辑器工程。不建议把它作为首选集成点。

桌面自动化可以作为打开工程、查看布局和人工式操作的补充，但控件可访问性和稳定性
需要实际确认。本轮没有操作 App，因此不宣称已经具备可靠的逐控件 GUI 自动化。

### 版本要求

官方 1.5.4 发布于 2025-10-09，1.6.0 增加 LVGL 9.3，1.6.2 于 2026-08-10
增加 LVGL 9.5、Apple Silicon 完整支持、代理支持和离线激活；1.6.1 记录了 CLI 导出
修复与屏幕截图导出。[3] 对本项目应优先评估 1.6.2 与 LVGL 9.5.0 组合，
不要为适应旧工具默认值而自动降级现有固件。此处没有升级任何本机应用。

## 推荐的工程组织与工作方式

以下路径为建议，尚未创建：

```text
ui/
  design/          SquareLine 工程与源素材（若采用）
  generated/       编辑器生成的页面/字体/图片，不手工改布局
  widgets/         自定义发动机/排气组件
  ui_state.h       不依赖 ESP-IDF 的状态与动作接口
  ui_presenter.c   状态到页面的更新，触摸到命令的转换
ports/desktop/     SDL 输入/显示、确定性状态、无音频输出
firmware/.../      LCD/触摸初始化、任务调度、板级适配
```

没有采用编辑器时，`generated/` 换为手写 `screens/` 即可，其他边界不变。
同一个页面或控件只选一个布局来源；不能同时在 `.spj` 和生成 C 中维护两套布局。
手写扩展放在生成目录之外，通过事件回调和初始化后的绑定接入。

现有 `board_ui.h` 已有状态结构和动作回调，但同时包含 ESP-IDF I²C/错误类型；
可以先提取平台无关接口，将 `board_ui.c` 中的页面/样式/更新与 LCD/触摸初始化分离。
这是让整个 UI 可在电脑运行的第一步。无需先引入一套庞大的 MVC/MVVM 框架。

动态发动机以共享的 RPM/点火状态驱动；编辑器适合容器、标签、设置页和控件样式，
不必承担逐帧机械绘制或高频音频调度。UI 操作发出命令，音频核心返回状态快照；
UI 事件内不执行阻塞音频、存储或通信操作，LVGL 对象在约定的 UI 上下文中更新。

行业中可复用的交付流程可以落实为：

1. **硬件与显示契约**：320×240 横屏、RGB565、物理尺寸、触控方式、字体与资源预算。
2. **页面与组件设计**：主屏、设置和状态层级，使用最终字体/图标；同时定义按下、禁用、
   故障、长文本等状态。先看实际尺寸可读性，再看放大后的美观程度。
3. **组件实现**：选定布局来源，生成代码与手写扩展分开；所有页面均可接固定状态运行。
4. **同源电脑预览**：锁定 LVGL、字体、主题、颜色与动画相位，输出完整原始像素画面；
   再提供整数倍放大与校准过的物理尺寸视图。
5. **后续静音硬件验收**：只在用户恢复硬件验证后检查触摸、可读性、旋转、色序与刷新，
   遵循仓库串口/栈水位要求。声音验收单独授权与安排。

电脑端与板端的锁定项还包括字体字形范围、抗锯齿配置、默认控件主题、
图片转换格式、透明度、布局边距和 UI 状态时间点。使用 GPU 绘制或不同颜色格式的
编辑器预览不能自动视为板端软件绘制的像素基准。

本板一帧 RGB565 原始像素为 320×240×2 = 153,600 字节（150 KiB），双缓冲为
300 KiB，不含对象、字库、画布和音频内存。全屏每秒 30 帧仅有效像素就需约
4.61 MB/s 的传输，尚未计命令与总线开销；这是预算计算，不是实测速度。
因此发动机局部刷新、字体/素材复用和按状态更新比大图堆叠更适合该项目。

## 后续完成条件与边界

- 一套页面实现覆盖首屏、给油/收油、设置展开、故障和全部排气选择，电脑预览有明确版本和状态。
- 编辑器重新导出不会覆盖音频逻辑、驱动或自定义绘图；项目能从保存的源文件重建 UI。
- 选择 SquareLine 时，先完成目标版本工程的读写/导出兼容确认，再决定接入第三方 MCP 或自建薄封装。
- UI 桌面端默认不链接声音输出，不触碰串口；静音预览与声音调试独立。
- 像素一致不能证明 LCD 背光、盖板、视角、触摸手感、时序及资源占用一致；这些仍需后续实物验收。
- 本轮只形成研究结论，不升级项目状态，不声称 UI 迁移完成，不修改现有固件工作区。

## 资料

访问日期均为 2026-09-22。产品定价/版本页面可能更新；固定源码引用用于保留判断依据。

1. [LVGL 官方桌面工程](https://github.com/lvgl/lv_port_pc_vscode)。
2. SquareLine CLI：[1.5.4 文档](https://docs.squareline.io/docs/1.5.4/commandline/)、
   [当前文档](https://docs.squareline.io/docs/commandline/)。
3. [SquareLine 发布记录](https://docs.squareline.io/docs/miscellanos/changelog/)。
4. [SquareLine 项目设置](https://docs.squareline.io/docs/dev_env/project_settings/)，
   包含分屏保存、LVGL 版本与导出设置。
5. [SquareLine 许可证说明](https://docs.squareline.io/docs/introduction/licences/)。
6. [EEZ Studio README](https://github.com/eez-open/studio/blob/v0.29.0/README.md)、
   [v0.29.0 发布记录](https://github.com/eez-open/studio/releases/tag/v0.29.0)。
7. EEZ v0.29.0：[LVGL 版本映射](https://github.com/eez-open/studio/blob/v0.29.0/packages/project-editor/lvgl/lvgl-versions.ts)、
   [命令行入口](https://github.com/eez-open/studio/blob/v0.29.0/packages/main/main.ts)。
8. [LVGL Pro v2.0.4](https://github.com/lvgl/lvgl_pro/releases/tag/v2.0.4)。
9. LVGL Pro：[Figma Flow](https://lvgl.io/docs/pro/figma)、
   [控件支持](https://lvgl.io/docs/pro/figma/widget-support)、
   [重新导出与文件归属](https://lvgl.io/docs/pro/figma/updating)。
10. LVGL Pro：[CLI](https://lvgl.io/docs/pro/cli)、[价格与权限](https://lvgl.io/pro/pricing)。

本地代码依据：[`board_ui.h`](../firmware/main/board_ui.h)、
[`board_ui.c`](../firmware/config/boards/lichuang_szp/board_ui.c)、
[`CMakeLists.txt`](../CMakeLists.txt)。
