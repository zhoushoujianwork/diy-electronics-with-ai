# LCKFB 立创·实战派 ESP32-S3 VA

立创开发板的实战派 ESP32-S3 开发板，仓库使用完整 ID
`lckfb-szpi-esp32-s3-va`。官方产品型号为 `LCKFB-SZPI-ESP32-S3-VA`，不能把本页引脚
直接套用到其他 ESP32-S3 N16R8 核心板。

## 官方规格

| 项目 | 值 |
| --- | --- |
| 厂商 / 品牌 | LCKFB（立创开发板） |
| 型号 | LCKFB-SZPI-ESP32-S3-VA |
| 主控模组 | Espressif ESP32-S3-WROOM-1-N16R8，双核 240 MHz |
| 存储 | 16 MB Flash、8 MB PSRAM |
| 显示 | 2.0 英寸 IPS、ST7789、SPI；240×320 像素，官方横屏示例为 320×240 |
| 触摸 | FT6336 电容触摸，I²C |
| 音频 | ES8311 DAC、ES7210 ADC、板载功放和扬声器 |
| 音频外围 | NS4150B 单声道 D 类功放、DB1811AB50 1 W 喇叭、两路 ZTS6216 麦克风；ES7210 第三路连接 DAC 回采用于 AEC 应用 |
| 姿态传感器 | QMI8658 六轴 IMU |
| 摄像头 | GC0308，标称 30 万像素，内部 BTB 接口；本档案未声明实机验证 |
| USB | 单 Type-C → CH334F HUB → CH340K 串口桥及 ESP32-S3 USB-OTG 两条路径 |
| 扩展 / 存储 | 两个 GH1.25-5P（共享 I²C、多功能各一个）、microSD/TF（1-bit SD） |
| 按键 | 复位键、用户/BOOT 键 |
| 整机尺寸 | 产品结构图 69×41×15 mm；Wiki 文字 69×41×14 mm，厚度存在来源冲突 |
| 供电 | Type-C 5 V；扩展口电源与输出余量另行核对 |
| 逻辑电平 | 3.3 V；外设 GPIO 不耐 5 V |

新增外设规格于 2026-09-22 对照官方产品页、硬件表及第 1 章整理，属于资料核验，
不表示这些外设已通过实机验收。ES7210 回采连接不等于已经实现或验收回声消除。

## 屏幕尺寸与 UI 约束

- **320×240 是官方示例的横屏画布。**官方第 9、11 章同样使用此配置。
- 2.0 英寸指显示区对角线；69×41 mm 指整机，不能拿来换算字体或按钮大小。
- 按标称 2.0 英寸、4:3 和方形像素估算，横屏显示区约 **40.64×30.48 mm**、200 PPI；
  这是设计估算，精确可视区、玻璃和开窗尺寸仍须查屏幕机械图或测量。
- 显示、触摸方向、物理尺度估算、内存预算和待确认项见
  [显示与触摸档案](display.md)。产品规格的机器可读记录见
  [catalog YAML](../../catalog/vendors/lckfb/products/szpi-esp32-s3-va.yaml)。

## 板载连接与资料边界

官方第 9 章显示示例使用 ST7789 SPI mode 2；屏幕背光接 GPIO42，以 5 kHz PWM 调光。
这是示例配置，不能将该频率表述为板卡唯一允许的值。教程说明 GPIO42 高电平关闭背光、
低电平打开背光。屏幕片选由 I²C IO 扩展器控制，先初始化共享 I²C 总线与扩展器，再初始化显示。

官方第 1 章说明 I²C 扩展口与姿态、触摸、音频芯片共享总线，多功能接口引出 GPIO10/11；
接线前核对精确 PCB 的连接器针序、电平和地址冲突。不得将共享 I²C 口任意改作其他用途。
模组 GPIO35/36/37 已被八线 PSRAM 使用，不能当作可用扩展引脚。

官方产品资源图将 IO 扩展器标为 **TCA9557**，第 9、11 章代码采用 **PCA9557** 命名。
这属于公开资料差异，不能据函数名确认实际装配器件。第 1 章音频 DAC 电路段另有 ES8310
文字，而概览和专门音频输出章节列 ES8311；实际器件和电气参数以对应修订原理图及丝印为准。

## 资料核验范围与未验证项

官方第 1、9、11 章于 2026-10-14 在线复核。本档案整理公开规格、示例配置与计算方法，
没有声明某个固件、显示/触摸/音频链路、AEC 或 SD/BLE/蜂窝组合负载已通过实机验收。
扬声器音质、触摸四角映射、供电峰值、帧率及最终外壳操作效果均需另做目标组合测试。

## 官方来源

- [立创·实战派 ESP32-S3 官方文档](https://wiki.lckfb.com/zh-hans/szpi-esp32s3/)
- [官方项目页](https://lckfb.com/project/detail/lckfb-esp32-s3-va)
- [开发板介绍与共享总线](https://wiki.lckfb.com/zh-hans/szpi-esp32s3/beginner/introduction.html)
- [LCD 显示、横屏与 RGB565](https://wiki.lckfb.com/zh-hans/szpi-esp32s3/beginner/lcd-display.html)
- [LVGL 与触摸配置](https://wiki.lckfb.com/zh-hans/szpi-esp32s3/beginner/lvgl.html)
- [官方原理图与外壳设计入口](https://wiki.lckfb.com/zh-hans/szpi-esp32s3/open-source-hardware/)

实际接线前应再次核对板卡丝印 `LCKFB-SZPI-ESP32-S3-VA` 和官方资料修订。

本仓库保存自主整理的事实与链接，不转载厂商原理图、照片、代码包或机械文件。厂商资料与商标归原权利人。
