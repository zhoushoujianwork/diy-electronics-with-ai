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
| 显示 | 2.0 英寸 ST7789，320×240，SPI |
| 触摸 | FT6336 电容触摸，I²C |
| 音频 | ES8311 DAC、ES7210 ADC、板载功放和扬声器 |
| 姿态传感器 | QMI8658 六轴 IMU |
| 供电 | 项目验证使用 USB 5 V |
| 逻辑电平 | 3.3 V；外设 GPIO 不耐 5 V |

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

## 验证状态

- 状态：`hardware-verified`，但保留明确的未完成项。
- 已烧录 ESP-IDF 固件并回读 UI、ES8311、PCA9557 和 FT6336。
- V12 负载测试达到 15999 RPM，连续心跳中未出现音频写错误、软件故障、panic、
  Guru Meditation、栈溢出、任务启动失败或重启循环。
- 横滑车型、红线滑块、启动/停止和按住给油已有触摸事件回读。
- 扬声器主观听感、最终触摸手感，以及未来 SD/BLE/4G 并发负载尚未验证。

## 来源与实现

- [立创·实战派 ESP32-S3 官方文档](https://wiki.lckfb.com/zh-hans/szpi-esp32s3/)
- [官方项目页](https://lckfb.com/project/detail/lckfb-szpi-esp32s3)
- [项目实机验证记录](../../projects/ev-engine-sound/docs/validation.md)
- [固件板级配置](../../projects/ev-engine-sound/firmware/config/boards/lichuang_szp/board_config.h)

实际接线前应再次核对板卡丝印 `LCKFB-SZPI-ESP32-S3-VA` 和官方资料修订。
