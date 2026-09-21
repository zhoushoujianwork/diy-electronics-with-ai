# M5Stack StickS3 K150

M5Stack 的紧凑型 ESP32-S3 开发套件，SKU 为 K150。仓库使用完整 ID
`m5stack-sticks3-k150`，不要与 M5StickC、M5StickC Plus 或 Plus2 共用板级配置。

## 已确认硬件

| 项目 | 值 |
| --- | --- |
| 厂商 | M5Stack |
| 型号 / SKU | StickS3 / K150 |
| 主控 | Espressif ESP32-S3-PICO-1-N8R8，双核 240 MHz |
| 存储 | 8 MB Flash、8 MB Octal PSRAM |
| 显示 | ST7789P3，原生 135×240，项目使用 240×135 横屏 |
| 音频 | ES8311、MEMS 麦克风、AW8737 功放、板载扬声器 |
| 输入 | A/B 可编程按键、侧面电源键 |
| 供电 | USB Type-C 5 V 或板载电池 |
| 逻辑电平 | 3.3 V；外设 GPIO 不耐 5 V |

项目固件使用内部 RAM，不依赖 PSRAM。USB 烧录前需确认目标为 8 MB Flash，不能加载立创板的
16 MB 配置。

## 项目使用的板级连接

| 功能 | GPIO / 地址 |
| --- | --- |
| 内部 I²C SDA / SCL | 47 / 48 |
| ES8311 / M5PM1 | `0x18` / `0x6e` |
| I²S MCLK / BCLK / WS | 18 / 17 / 15 |
| I²S DOUT / DIN | 14 / 16；DIN 当前未使用 |
| LCD MOSI / SCLK | 39 / 40 |
| LCD DC / CS / RESET / BL | 45 / 41 / 21 / 38 |
| A / B 按键 | 11 / 12，低电平有效 |
| HY2.0-4P Grove | 黑 GND、红 5V、黄 GPIO9、白 GPIO10 |

LCD 电源与扬声器使能由 M5PM1 的 GPIO2/GPIO3 控制，不是 ESP32 的 GPIO2/GPIO3。
GPIO38 用于屏幕背光 PWM；不要再把上述引脚分配给外接模块。

Grove 5V 默认处于输入/关闭状态。使用板载电池或 USB 给 Grove 外设供电时，固件必须先通过
M5PM1 `POWER_CONFIG.BOOST_EN` 开启 5V 输出；开启后不得再从 Grove 红线反向输入 5V。
UART 外设通常把主机 GPIO9 作为 TX、GPIO10 作为 RX，但仍须按外设连接器丝印确认交叉方向。

## 验证状态

- 状态：`hardware-verified`。
- 2026-09-17 已确认芯片和 Flash 身份，并完成固件烧录及逐段 Hash 校验。
- 67 秒负载测试获得 66 个连续心跳；V12 达到 15999 RPM，未出现音频写错误、panic、
  Guru Meditation、栈溢出、任务启动失败或重启循环。
- 屏幕、声音、A 键给油/释放和 B 键换车型已有实体操作回读。
- 设置页和 A+B 同时停机仍需要补充实体按键验收；未测量校色、声压或音色还原精度。

## 来源与实现

- [M5Stack StickS3 官方文档](https://docs.m5stack.com/en/core/StickS3)
- [项目适配与完整引脚依据](../../projects/ev-engine-sound/docs/sticks3.md)
- [项目实机验证记录](../../projects/ev-engine-sound/docs/validation.md)
- [固件板级配置](../../projects/ev-engine-sound/firmware/config/boards/m5_sticks3/board_config.h)

实际接线前仍应核对手中板卡丝印、SKU 和官方原理图修订。
