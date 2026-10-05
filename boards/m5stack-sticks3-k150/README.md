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
| 显示 | ST7789P3，原生 135×240；横屏逻辑画布为 240×135 |
| 音频 | ES8311、MEMS 麦克风、AW8737 功放、板载扬声器 |
| 输入 | A/B 可编程按键、侧面电源键 |
| 供电 | USB Type-C 5 V 或板载电池 |
| 逻辑电平 | 3.3 V；外设 GPIO 不耐 5 V |
| 外壳标称尺寸 | 长 × 宽 × 厚：48 × 24 × 15 mm；不是 PCB 板框 |
| 顶部扩展 | Hat2-Bus，2.54 mm、16P，官方针号分为奇/偶两列 |

USB 烧录前需确认目标为 8 MB Flash，并按实际固件核对 PSRAM 配置。

## 板载连接

| 功能 | GPIO / 地址 |
| --- | --- |
| 内部 I²C SDA / SCL | 47 / 48 |
| ES8311 / M5PM1 | `0x18` / `0x6e` |
| I²S MCLK / BCLK / WS | 18 / 17 / 15 |
| I²S DOUT / DIN | 14 / 16 |
| LCD MOSI / SCLK | 39 / 40 |
| LCD DC / CS / RESET / BL | 45 / 41 / 21 / 38 |
| A / B 按键 | 11 / 12，低电平有效 |
| HY2.0-4P Grove | 黑 GND、红 5V、黄 GPIO9、白 GPIO10 |

LCD 电源与扬声器使能由 M5PM1 的 GPIO2/GPIO3 控制，不是 ESP32 的 GPIO2/GPIO3。
GPIO38 用于屏幕背光 PWM；不要再把上述引脚分配给外接模块。

Grove 5V 默认处于输入/关闭状态。使用板载电池或 USB 给 Grove 外设供电时，固件必须先通过
M5PM1 `POWER_CONFIG.BOOST_EN` 开启 5V 输出；开启后不得再从 Grove 红线反向输入 5V。
UART 外设通常把主机 GPIO9 作为 TX、GPIO10 作为 RX，但仍须按外设连接器丝印确认交叉方向。

## 顶部 Hat2 接口与供电方向

2026-09-27 按官方中英文文档核对，以下为**文档核验**，尚未实测该接口的扩展板。
针号按官方 Hat2-Bus 表转录；表格左右列不能代替实物的观察方向、Pin 1 定位和配对连接器图纸。

| 奇数针 | 信号 | 偶数针 | 信号 |
| --- | --- | --- | --- |
| 1 | GND | 2 | G5 |
| 3 | EXT_5V | 4 | G4 |
| 5 | Boot | 6 | G6 |
| 7 | G1 | 8 | G7 |
| 9 | G8 | 10 | G43 |
| 11 | BAT | 12 | G44 |
| 13 | 3V3_L2 | 14 | G2 |
| 15 | 5V_IN | 16 | G3 |

- 官方说明：外部 5 V 接口默认输入模式，可从 Grove、Hat2 EXT_5V 或 5V_IN 输入 DC 5 V。
- 配置成输出模式后，只允许从 USB 或 Hat2 **5V_IN** 输入供电；不得再向 Grove 或 EXT_5V 反向输入。
- M5Unified 默认初始化关闭 `EXT_5V_EN`；`M5.Power.setExtOutput(true)` 切换为外部输出。
  这与直接操作 M5PM1 的实现属于不同软件层级，固件需按实际库版本核对。
- 官方 Grove 带载能力为最大 **4.88 V @ 0.38 A**；这不是顶部 Hat2 的额定电流，也不能证明足以供给蜂窝负载。
- 板载电池标称容量为 250 mAh；官方未给出 Hat2 BAT 可承受的蜂窝发射峰值，不能据此向模组供电。
- `EXT_5V`、`5V_IN`、`BAT`、`3V3_L2` 是不同电源网络，不能并接或用同一个“5V”符号代替。
  GPIO 分配前还需核对启动配置、串口调试和其他板载功能；此表没有分配任何项目 UART。

顶部 Hat2 与旧 StickC 接口不能按针数或名称直接互换。官方明确说明 U156、U157、U080
三款 Hat 存在结构不兼容。设计适配器时需核对官方结构文件与实际外壳、插入深度及受力固定。

## 验证范围

本页记录官方型号、连接和供电资料核验。具体固件、外设与负载的实机结论需要独立的
验收记录，注明精确硬件版本、固件版本、供电、时长和范围。

## 来源

- [M5Stack StickS3 官方文档](https://docs.m5stack.com/en/core/StickS3)
- [M5Stack StickS3 中文文档](https://docs.m5stack.com/zh_CN/core/StickS3)（Hat2、尺寸与供电，2026-09-27 核验）
- [M5Stack StickS3 官方结构文件](https://github.com/m5stack/M5_Hardware/tree/master/Products/K150_StickS3/Structures)

实际接线前仍应核对手中板卡丝印、SKU 和官方原理图修订。
