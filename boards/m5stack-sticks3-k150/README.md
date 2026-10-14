# M5Stack StickS3 K150

M5Stack 的紧凑型 ESP32-S3 开发套件，SKU 为 K150。仓库使用完整 ID
`m5stack-sticks3-k150`，不要与 M5StickC、M5StickC Plus 或 Plus2 共用板级配置。

## 官方规格

| 项目 | 值 |
| --- | --- |
| 厂商 | M5Stack |
| 型号 / SKU | StickS3 / K150 |
| 主控 | Espressif ESP32-S3-PICO-1-N8R8，双核 240 MHz |
| 存储 | 8 MB Flash、8 MB Octal PSRAM |
| 显示 | ST7789P3，原生 135×240；软件旋转不改变面板像素矩阵 |
| 音频 | ES8311、MEMS 麦克风、AW8737 功放、板载扬声器 |
| 输入 | A/B 可编程按键、侧面电源键 |
| 供电 | USB Type-C 5 V 或板载电池 |
| 逻辑电平 | 3.3 V；外设 GPIO 不耐 5 V |
| 外壳标称尺寸 | 长 × 宽 × 厚：48 × 24 × 15 mm；不是 PCB 板框 |
| 顶部扩展 | Hat2-Bus，2.54 mm、16P，官方针号分为奇/偶两列 |

Flash 与 PSRAM 是不同存储；驱动的 DMA 和任务栈分配仍须按 ESP-IDF 与具体软件版本核对。
烧录配置应匹配 8 MB Flash，不根据外形相近的 ESP32-S3 开发板沿用配置。

## 官方 PinMap 中的板载连接

下表于 2026-10-14 对照 M5Stack 官方页面复核；不是某个项目的运行时配置。

| 功能 | GPIO / 地址 |
| --- | --- |
| 内部 I²C SDA / SCL | 47 / 48 |
| ES8311 / M5PM1 | `0x18` / `0x6e` |
| I²S MCLK / BCLK / WS | 18 / 17 / 15 |
| I²S DOUT / DIN | 14 / 16 |
| LCD MOSI / SCLK | 39 / 40 |
| LCD DC / CS / RESET / BL | 45 / 41 / 21 / 38 |
| A / B 按键 | 11 / 12 |
| HY2.0-4P Grove | 黑 GND、红 5V、黄 GPIO9、白 GPIO10 |

LCD 电源与扬声器使能分别由 M5PM1 的 GPIO2/GPIO3 控制，不是 ESP32 的 GPIO2/GPIO3。
GPIO38 是屏幕背光连接；添加外设前核对上述板载资源占用。
UART 方向由应用配置决定，接外设时仍须按其连接器定义交叉 TX/RX，不能只按线色判断。

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

## 资料核验范围与未验证项

本档案仅整理官方规格、PinMap、供电警告与结构入口；上述连接于 2026-10-14 在线复核。
没有据此声明任何固件、Hat2 扩展、蜂窝负载、外部供电切换或外壳装配已通过实机验收。
GPIO 电平、扩展口余量、启动配置和信号方向需在目标软硬件组合中检查。

## 官方来源

- [M5Stack StickS3 官方文档](https://docs.m5stack.com/en/core/StickS3)
- [M5Stack StickS3 中文文档](https://docs.m5stack.com/zh_CN/core/StickS3)
- [M5Stack StickS3 官方结构文件](https://github.com/m5stack/M5_Hardware/tree/master/Products/K150_StickS3/Structures)
- [Espressif ESP32-S3 数据手册](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)

实际接线前仍应核对手中板卡丝印、SKU 和官方原理图修订。本仓库保存自主整理的事实与链接，
不转载厂商结构文件、原理图、照片或数据手册；这些资料与商标的权利归原权利人。
