# Boards

每个板型目录记录精确型号/修订、官方资料链接、MCU/Flash/PSRAM、供电、逻辑电平、连接器、
保留引脚、冲突和验证状态。未知修订不沿用相似板卡的引脚结论。

## 命名约定

- 目录 ID 使用 `厂商-精确型号-必要修订`，例如 `m5stack-sticks3-k150`。
- 页面标题和索引名称都以厂商或品牌开头，避免只写 `StickS3`、`ESP32-S3` 这类容易混淆的名称。
- SoC 厂商不等于开发板厂商；例如 StickS3 的板卡厂商是 M5Stack，主控厂商是 Espressif。
- 只有确认过精确型号和硬件修订的板卡才能建立引脚档案。购买记录只能证明买过，不能证明仍有库存或已经实测。

## ESP32 板卡

| 板卡 | 主控 | 项目验证 | 状态 |
| --- | --- | --- | --- |
| [M5Stack StickS3 K150](m5stack-sticks3-k150/) | ESP32-S3-PICO-1-N8R8 | EV Engine Sound | hardware-verified |
| [LCKFB 立创·实战派 ESP32-S3 VA](lckfb-szpi-esp32-s3-va/) | ESP32-S3-WROOM-1-N16R8 | EV Engine Sound | hardware-verified（部分交互仍待复核） |

这里列的是仓库项目已经实际使用的板卡，不是个人购买清单。型号不明确的历史 ESP32-S3
开发板暂不收录，等确认厂商、型号和修订后再补。
