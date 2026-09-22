# LCKFB 立创·实战派 ESP32-S3 VA

立创开发板的实战派 ESP32-S3 开发板，仓库使用完整 ID
`lckfb-szpi-esp32-s3-va`。已确认物料型号为 `LCKFB-SZPI-ESP32-S3-VA`，不能把本页引脚
直接套用到其他 ESP32-S3 N16R8 核心板。

## 已确认硬件

| 项目 | 值 |
| --- | --- |
| 厂商 / 品牌 | LCKFB（立创开发板） |
| 型号 | LCKFB-SZPI-ESP32-S3-VA |
| 主控模组 | Espressif ESP32-S3-WROOM-1-N16R8，双核 240 MHz |
| 存储 | 16 MB Flash、8 MB PSRAM |
| 显示 | 2.0 英寸 IPS、ST7789、SPI；240×320 像素，官方横屏示例及本项目逻辑画布为 320×240 |
| 触摸 | FT6336 电容触摸，I²C |
| 音频 | ES8311 DAC、ES7210 ADC、板载功放和扬声器 |
| 音频外围 | NS4150B 单声道 D 类功放、DB1811AB50 1 W 喇叭、两路 ZTS6216 麦克风；ES7210 第三路连接 DAC 回采用于 AEC 应用 |
| 姿态传感器 | QMI8658 六轴 IMU |
| 摄像头 | GC0308，标称 30 万像素，内部 BTB 接口；本项目未验证 |
| USB | 单 Type-C → CH334F HUB → CH340K 串口桥及 ESP32-S3 USB-OTG 两条路径 |
| 扩展 / 存储 | 两个 GH1.25-5P（共享 I²C、多功能各一个）、microSD/TF（1-bit SD） |
| 按键 | 复位键、用户/BOOT 键 |
| 整机尺寸 | 产品结构图 69×41×15 mm；Wiki 文字 69×41×14 mm，厚度存在来源冲突 |
| 供电 | 项目验证使用 USB 5 V |
| 逻辑电平 | 3.3 V；外设 GPIO 不耐 5 V |

新增外设规格于 2026-09-22 对照官方产品页、硬件表及第 1 章整理，属于资料核验，
不表示这些外设已经在本仓库运行。ES7210 回采连接不等于已经实现或验收回声消除。

## 屏幕尺寸与 UI 约束

- **320×240 是当前横屏画布，并非把面板分辨率填错。**官方第 9、11 章同样使用此配置。
- 2.0 英寸指显示区对角线；69×41 mm 指整机，不能拿来换算字体或按钮大小。
- 按标称 2.0 英寸、4:3 和方形像素估算，横屏显示区约 **40.64×30.48 mm**、200 PPI；
  这是设计估算，精确可视区、玻璃和开窗尺寸仍须查屏幕机械图或测量。
- 显示、触摸方向、物理尺度估算、内存预算和待确认项见
  [显示与触摸档案](display.md)。产品规格的机器可读记录见
  [catalog YAML](../../catalog/vendors/lckfb/products/szpi-esp32-s3-va.yaml)。

## EV Engine Sound 使用的连接

| 功能 | GPIO / 地址 |
| --- | --- |
| I²C SDA / SCL | 1 / 2 |
| ES8311 / PCA9557 / FT6336 | `0x18` / `0x19` / `0x38` |
| I²S MCLK / BCLK / WS | 38 / 14 / 13 |
| I²S DOUT / DIN | 45 / 12 |
| LCD SCLK / MOSI / DC / BL | 41 / 40 / 39 / 42 |
| PTT / 状态 LED | 0，低有效 / 48 |

LCD 片选和功放使能由 PCA9557 控制，不是直接 GPIO。屏幕要求 SPI mode 2，背光 GPIO42
要求 5 kHz PWM；静态高低电平不能可靠点亮其升压电路。I²C0 同时连接音频、GPIO 扩展和
触摸，添加外设前必须检查地址、总线负载和初始化顺序。

官方产品资源图将 IO 扩展器标为 **TCA9557**，本项目驱动沿用 **PCA9557** 命名。
现有寄存器读回证明的是当前驱动路径可用，不等于通过器件 ID 确认了实际芯片型号；
更换器件或核对电气参数时仍须查看对应 PCB 修订和芯片丝印。

## 验证状态

- 状态：`hardware-verified`，但保留明确的未完成项。
- 已烧录 ESP-IDF 固件并回读 UI、ES8311、PCA9557 和 FT6336。
- V12 负载测试达到 15999 RPM，连续心跳中未出现音频写错误、软件故障、panic、
  Guru Meditation、栈溢出、任务启动失败或重启循环。
- 横滑车型、红线滑块、启动/停止和按住给油已有触摸事件回读。
- 扬声器主观听感、最终触摸手感，以及未来 SD/BLE/4G 并发负载尚未验证。

## 来源与实现（2026-09-22 复核）

- [立创·实战派 ESP32-S3 官方文档](https://wiki.lckfb.com/zh-hans/szpi-esp32s3/)
- [官方项目页](https://lckfb.com/project/detail/lckfb-esp32-s3-va)
- [开发板介绍与共享总线](https://wiki.lckfb.com/zh-hans/szpi-esp32s3/beginner/introduction.html)
- [LCD 显示、横屏与 RGB565](https://wiki.lckfb.com/zh-hans/szpi-esp32s3/beginner/lcd-display.html)
- [LVGL 与触摸配置](https://wiki.lckfb.com/zh-hans/szpi-esp32s3/beginner/lvgl.html)
- [官方原理图与外壳设计入口](https://wiki.lckfb.com/zh-hans/szpi-esp32s3/open-source-hardware/)
- [项目实机验证记录](../../projects/ev-engine-sound/docs/validation.md)
- [固件板级配置](../../projects/ev-engine-sound/firmware/config/boards/lichuang_szp/board_config.h)

实际接线前应再次核对板卡丝印 `LCKFB-SZPI-ESP32-S3-VA` 和官方资料修订。
