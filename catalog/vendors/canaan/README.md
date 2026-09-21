# Canaan / Kendryte / CanMV supplier knowledge

本页记录查询 Canaan K230 与 CanMV 资料时可复用的供应商级知识，以及去标识化的工程经验。
具体产品规格、来源和验证等级仍以 [`products/`](products/) 下的记录为准；
引脚、供电和结构尺寸只有在精确板型证据完整后才进入仓库根目录的 `boards/`。

## 名称与产品层次

- **Canaan / 嘉楠**是厂商；官方英文资料也使用 `Canaan Creative`。
- **Kendryte / 勘智**是 AI SoC 品牌，`K230` 是芯片型号。
- **CanMV** 是面向视觉 AI 的 MicroPython 软件平台，也是官方固件和部分开发板命名的一部分。
- `CanMV K230 V3.0`、庐山派和其他 K230 板不能只因采用相同 SoC
  就合并为一个板型。板名、修订、内存配置、FPIOA 映射和连接器必须分别核对。

## 官方资料入口

| 用途 | 入口 | 说明 |
| --- | --- | --- |
| 开发者与资源 | [Kendryte](https://www.kendryte.com/) | 固件镜像和开发资源的厂商入口 |
| K230 硬件/SDK 文档 | [kendryte/k230_docs](https://github.com/kendryte/k230_docs) | `zh/` 与 `en/` 分别保存中英文文档 |
| K230 技术参考手册 | [kendryte/k230_trm_docs](https://github.com/kendryte/k230_trm_docs) | 查外设、寄存器、PAD/FPIOA 等 SoC 级事实 |
| CanMV K230 固件 | [kendryte/canmv_k230](https://github.com/kendryte/canmv_k230) | 板型目标、版本、构建方式和 MicroPython 实现 |
| CanMV API 与使用文档 | [kendryte/k230_canmv_docs](https://github.com/kendryte/k230_canmv_docs) | API、示例和使用说明；通用 CanMV-K230 页面不能代替 V3.0 工程 |
| RTOS SDK 板级配置 | [kendryte/k230_rtos_sdk](https://github.com/kendryte/k230_rtos_sdk) | `boards/k230_canmv_v3p0/` 与对应 defconfig，属于固件配置依据 |
| CanMV v1.8 | [release](https://github.com/kendryte/canmv_k230/releases/tag/v1.8) | 本页现场经验对应的固件大版本 |
| 模型工具链 | [kendryte/nncase](https://github.com/kendryte/nncase) | 模型导入、量化和 KModel 编译 |

优先引用固定提交或 release tag。`main`、在线文档站和镜像下载页会随版本变化；记录 API 行为时
必须同时写明 CanMV 版本和精确板型，不能把某次固件的行为外推到所有 K230 镜像。

## CanMV K230 V3.0 硬件工程入口

**V3.0 的原理图和 PCB 工程有官方公开来源。**查到固件仓库中的 `mpconfigboard.h` 后，
还应继续查 [K230 Docs README 的 CanMV-K230 工程设计文件区][hardware-index]，不能据一个
简短的板型配置文件或一个未补齐的 catalog 记录断言硬件资料缺失。

其中 [V3.0 EPRO 工程][v3-project] 的文件名为
`ProDocument_CanMV-K230-LP4-V3.0-20240509.epro`。截至 2026-09-21，下载并只读解析确认：

- `project.json` 的原理图与 PCB 名称为 `CanMV-K230-LP4-V3.0_20240509`。
- 工程包含 7 页原理图：P1、K230、LPDDR4、Video、Power-K230、Peripherial、HDMI+ETH，
  以及 1 个 PCB；`.epro` 是 ZIP 格式，内含 `.esch`、`.epcb` 与元器件库定义。
- 官方 README 对这个 EPRO 的链接文字误标为“CanMV-K230D-Zero V1.0”，但下载 URL 和工程
  内部名称都指向 K230 LP4 V3.0。核验应结合工程内部身份，不照抄错误的链接标签。
- 同一区域的 `CanMV-K230_2023-10-11.html` 是嵌入数据的离线 SMT/PCB 查看页面；文件名日期
  与 EPRO 不同，不能自动当成同一次设计导出，查看源码时也应避免整份输出其内嵌数据。

下载文件为 998617 字节，SHA-256 为
`14d25ec39c6fa5e6cb9624edbdaffb0b659b82daddfd18a91ecab601d5258555`。
哈希用于识别本次核验的公开文件，不是对厂家未来同名下载的完整性保证。工程原件不复制
进入本仓库；已核实的器件与接口设计归入 [产品记录](products/canmv-k230-v3p0.yaml)。

固件仓库、官方文档仓库和 HDK 下载站分开维护。用户 fork 可以帮助发现来源和软件版本，
产品事实仍应回溯官方工程；fork 中的应用代码或硬编码 GPIO 不能替代板级连线证据。

[hardware-index]: https://github.com/kendryte/k230_docs/blob/f8e30213963e0ed5cf995c3cd4701218a45e4793/README.md
[v3-project]: https://kendryte-download.canaan-creative.com/developer/k230/HDK/CanMV-K230%E5%B7%A5%E7%A8%8B%E8%AE%BE%E8%AE%A1%E6%96%87%E4%BB%B6/ProDocument_CanMV-K230-LP4-V3.0-20240509.epro

## 能力地图

K230 SoC 的官方数据手册描述双 64 位 RISC-V C908、KPU、AI2D/2D、视频输入输出、音视频编解码、
音频和常用控制外设。板级开发时应把能力拆成三层核对：

1. **SoC 能力**：数据手册或 TRM 说明芯片可能支持什么。
2. **板卡暴露**：原理图和精确修订决定传感器、显示、音频 codec、存储及 PAD 是否实际连接。
3. **固件绑定**：CanMV 的板型目标、镜像版本和运行时模块决定 Python 层当前能调用什么。

`K230 支持某外设` 不等于 `某块 K230 板已接出该外设`，也不等于 `当前 CanMV 镜像已绑定该接口`。

立创·庐山派属于采用 Canaan K230/K230D 的 LCKFB 板级产品，具体硬件差异见
[LCKFB 供应商知识页](../lckfb/README.md)。Canaan SoC/CanMV 平台事实与 LCKFB 板级事实应
分别维护，不能把庐山派原理图或引脚写成通用 K230 能力。

## PDM 音频：官方软件路径与板型边界

CanMV v1.8 的公开 `media/pyaudio.py` 实现已有 PDM 输入和 3A 配置分支；同时提供
`audio_pdm.py` 示例，但示例明确按庐山派配置 GPIO。不能把这组引脚写入 CanMV K230 V3.0
板卡记录。单颗 PDM 麦克风还需核对该版本按双声道组采集的行为、有效左右槽与时钟范围。

详见 [PDM MEMS 与 MAX9814 音频前端比较](../../../docs/research/pdm-vs-max9814-audio-frontends.md)，
其中引用固定提交的官方源码，并区分 SoC 支持、Python 绑定、V3.0 接线和真实音频效果。

## 去标识化工程经验

以下内容是多次 CanMV/K230 开发中可复用的去标识化经验，不包含私有项目、设备或部署信息。
它们是工程模式和风险提示，不是 Canaan 官方规格，也不能据此提升产品的验证等级。

### 已用到的能力

- `os.uname()[-1]` 可用于选择精确板型能力表；未知板应保持输出外设禁用，而不是逐个 GPIO
  “试初始化”。
- `/data` 可承载带 `__init__.py` 和相对导入的多文件 MicroPython 包，适合稳定 bootstrap、
  隔离 smoke 目录和事务式部署；更新工具应保留明确的生产入口保护。
- 摄像头、显示/OSD、触摸、KPU、WLAN、UART、SD、音频和 PWM 可以组合使用，但每项能力仍取决于
  精确板型和固件绑定，不能从一个成功组合外推到所有 K230 板。
- 某些 CanMV 构建提供 `FPIOA`、`I2C`、`PWM`、`Pin`、`TOUCH`、`UART`、`chipid` 和
  `temperature` 等运行时接口；应以目标 release 的公开 API 文档和运行时探针分别确认。
- 音频接口返回非零 PCM 只说明数据通路工作，不等于声学质量、AEC 或识别效果已经验收。

### 调度与稳定性约束

- KPU、摄像头和显示属于共享媒体管线。可把 KPU 推理留在主循环，让网络线程只收发和排队，
  避免跨线程调用 KPU。不要在已经绑定 VO 的摄像头通道上额外 `snapshot()`，否则可能争抢
  视频缓冲并在原生层阻塞。
- `sta.scan()`、大块 socket 发送、SD/FAT 操作和模型推理都可能长时间同步占用运行时。
  实践中采用小块传输、单并发 HTTP、超时、限频日志以及“主线程执行设备驱动、工作线程处理网络”
  的边界。
- UART 的 `any()` 在部分 CanMV 构建中更适合作为“有数据可读”标志，而不是精确字节数；按行协议
  应维护接收缓冲、限制最大长度，并丢弃过期状态，只保留最新心跳/事件。
- FPIOA 是板级事实的一部分。临时探针必须保存并恢复原复用和输入/输出配置；同为 K230 的板子
  也不能照搬 UART、按键、LED、蜂鸣器或背光引脚。
- 低功耗策略首先可做暗屏、降低检测频率和限速后台工作；“摄像头未停流的暗屏降载”不能表述为
  真正休眠。功耗和温升结论需要同工况仪表测试与持续运行，而不是只看软件状态。

### 复用时的验证清单

1. 记录精确板名、PCB 修订、CanMV release、`os.uname()` 和公开固件版本。
2. 先运行隔离目录下的单外设 smoke，再组合摄像头、KPU、显示、网络、SD、UART 和音频负载。
3. 串口持续观察首个 fatal、panic、复位原因、USB 重新枚举、心跳连续性和内存趋势。
4. 只有官方原理图或实物测量确认后，才把具体 PAD、排针、电源方向和机械尺寸写进 `boards/`。
5. 将“API 可调用”“返回了数据”“功能可用”“质量达标”“长稳通过”分别记录，不合并结论。

## 当前核验范围与归档边界

- [`products/canmv-k230-v3p0.yaml`](products/canmv-k230-v3p0.yaml) 已补入公开 V3.0 工程来源、
  LPDDR4 器件型号、麦克风、音频插座和 40Pin 排针设计。LPDDR 数值容量、完整引脚映射、
  电压域与复用、供电限制和机械参数尚未完成核验；这是核验进度，不是官方资料缺失的结论。
- 私有项目、设备照片、内部日志或未公开测量不能补齐上述字段；只有公开可复核来源才能进入 catalog。
