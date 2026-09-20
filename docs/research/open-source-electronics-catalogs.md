# 相似开源项目调研

调研日期：2026-09-20

本页关注三类可借鉴项目：开发板元数据、硬件支持包/示例，以及元件图形库。它们可以帮助本仓库设计目录结构和验证流程，但不是可直接合并的数据源。许可证只描述所链接仓库；厂商网页、数据手册、图片和商标可能另有条款。

## 值得借鉴的项目

| 项目 | 类型 | 仓库许可证 | 可借鉴点 | 引入风险 / 不应照搬的内容 |
| --- | --- | --- | --- | --- |
| [PlatformIO Espressif 32](https://github.com/platformio/platform-espressif32) | 开发平台与 `boards/*.json` 清单 | Apache-2.0 | 每块板一个机器可读清单；把调试器、上传参数、框架兼容性与板级事实分开 | 清单服务于 PlatformIO 构建语义，不等于通用硬件规格；不要把上传参数推断成电气能力 |
| [Zephyr](https://github.com/zephyrproject-rtos/zephyr) | RTOS、板级定义和 samples | Apache-2.0 | `boards/`、Devicetree、Kconfig、示例与测试分层成熟；板 revision 和构建 target 命名值得参考 | 数据与 Zephyr 的 binding/驱动模型强耦合；“可构建”不能转换成“实机验证” |
| [MicroPython](https://github.com/micropython/micropython) | 多 MCU ports、board 定义和 examples | MIT（仓库根 `LICENSE`） | 以 port/board 分层；小型、可运行示例便于爱好者复现 | board 配置主要描述 MicroPython 固件构建，并非完整产品规格；部分第三方子目录可能有独立许可 |
| [CircuitPython.org](https://github.com/adafruit/circuitpython-org) | 开发板目录网站及下载入口 | 仓库 API 未识别许可证，使用前需逐文件核验 | 面向人的板卡发现体验、厂商/芯片筛选、固件下载入口 | 不能假设网站元数据、图片或描述可再分发；本仓库只记录自主整理事实并链接官方来源 |
| [Espressif ESP-BSP](https://github.com/espressif/esp-bsp) | 官方板级支持组件与示例 | 仓库 API 未识别许可证，组件可能各自声明 | BSP 组件、板级初始化和示例的边界清楚；适合参考“支持声明必须落到可运行示例” | 必须按组件核对许可证；只适用于 Espressif 生态，不能直接成为跨厂商 schema |
| [Arduino examples](https://github.com/arduino/arduino-examples) | IDE 内置教学示例 | CC0-1.0 | 从最小语言特性到通信、传感器的渐进示例结构 | 示例通常不表达供电、逻辑电平和特定板限制；不能把教学代码视为接线证据 |
| [RIOT](https://github.com/RIOT-OS/RIOT) | IoT OS、boards、drivers、examples/tests | LGPL-2.1 | 板、CPU、驱动、应用、自动化测试的分离方式；多架构命名实践 | LGPL 义务与本仓库内容类型需分别评估；其 board support 状态不能替代本仓库硬件验证 |
| [Fritzing parts](https://github.com/fritzing/fritzing-parts) | 元件图形/连接器元数据 | 图形和文档声明为 CC BY-SA 3.0 | 元件视图、连接点和可视化接线资料的组织方法 | ShareAlike 和署名要求会传播到衍生图形；不要复制图片、SVG 或元件描述到本目录 |
| [Pinout.xyz](https://github.com/pinout-xyz/Pinout.xyz) | Raspberry Pi 引脚资料网站 | CC BY-SA 4.0 | 引脚功能的可浏览呈现、来源清晰的扩展板说明 | 引脚资料有型号/revision 语境；ShareAlike 内容不要混入未标明来源的板卡事实 |
| [Meshtastic firmware](https://github.com/meshtastic/firmware) | 多厂商无线板固件与 variants | GPL-3.0 | 大量真实市售板 variant、硬件抽象和持续构建矩阵 | variant 往往是固件配置而非权威规格；GPL 代码不可未经评估复制到宽松许可工程 |

## 对本仓库的设计结论

1. **事实目录与可执行 Demo 分离。** `catalog/` 记录有来源的产品族事实；`demos/` 证明一个可复现能力；`projects/` 组合多个能力。
2. **来源和验证状态是一级字段。** 每条资料记录官方 URL、访问日期；编译、实机和长期运行分别标记，绝不互相代替。
3. **使用本仓库自己的归一化描述。** 不批量镜像别人的 JSON/YAML、图片、数据手册或营销文案。许可证兼容也不等于数据正确或适用于当前 revision。
4. **保留型号与 revision 语境。** 家族条目用于发现，具体接线和电气参数最终必须落到精确 SKU/revision。
5. **链接优先于转载。** 官方产品页、文档站、原理图和源代码只保存链接与少量可核验事实；第三方文件需要明确再分发许可才进入 Git。

## 来源核验说明

- 仓库 URL、描述和 GitHub 可识别的 SPDX 标识通过 GitHub Repository API 于 2026-09-20 查询。
- MicroPython 的 MIT 许可另外由仓库根 `LICENSE` 核验。
- CircuitPython.org 与 ESP-BSP 的 GitHub API 未返回明确 SPDX 标识，因此这里刻意标为“需逐文件核验”，不根据项目归属猜测许可证。
- Fritzing parts 的许可说明来自其仓库根 `LICENSE.txt`。

## 后续扩展队列

首批目录优先建立了 12 家厂商和 24 个可验证条目。后续适合按真实 Demo 需求逐批补充，而不是一次性抓取营销目录：

- M5Stack：CoreS3、Cardputer、Dial、NanoC6、LoRa/蜂窝通信 Module 和更多 GNSS Unit；
- Espressif：ESP32-C、H、P 系列及对应 DevKit、模组和芯片级条目；
- Seeed Studio：SenseCAP、Wio、Grove 传感器与 reComputer/reTerminal；
- Adafruit 与 SparkFun：FeatherWing、QT Py、Thing Plus 和 Qwiic 传感器；
- Raspberry Pi、Arduino：精确到当前在售型号、内存配置与 board revision 的条目；
- Waveshare、LILYGO、Heltec：显示、LoRa、蜂窝、GNSS 和电子纸系列；
- DFRobot、Sipeed：FireBeetle、Gravity、Maix、Tang、Lichee 等系列。

每次扩展都应优先补齐精确 SKU/revision、电源与逻辑电平、连接器方向、官方原理图/数据手册链接；若官方页面只给营销描述，则保留为发现条目，不补猜测参数。
