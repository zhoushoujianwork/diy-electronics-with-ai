# Demos

这里放单一模块、接口或硬件组合的最小可复现实验。新 Demo 从
[`templates/demo/`](../templates/demo/) 开始，目录名使用小写 `kebab-case`。

建议示例：

- `at6668-gps-uart`：只验证 UART、NMEA、定位质量和时间；
- `n9301-audio-player`：只验证 SD 文件播放与 UART 控制；
- `dual-button-input`：只验证去抖、长按和组合键。

建议名称不代表已经实现或拥有现货，真正创建时必须填写验证状态。

## 已有 Demo

| Demo | 目的 | 状态与验证 |
| --- | --- | --- |
| [ESP32 A2DP Engine Sound](esp32-a2dp-engine-sound/) | 原版 ESP32 用 BOOT 控制声浪，经 A2DP 发给蓝牙耳机/音箱；本轮用 Beats Flex 试配 | 开发中，`build-verified`；已构建/烧录，120 秒 AP 与蓝牙扫描共存检查通过；页面选配、耳机出声待验证。[证据](esp32-a2dp-engine-sound/docs/validation.md) |
| [StickS3 ML307R-DL Tiny AT/TCP](sticks3-ml307r-at/) | StickS3 UART1 检测 Tiny AT 板、等待蜂窝注册并打开 TCP socket | 开发中，`build-verified`；GPIO7/TX、GPIO4/RX 与 Tiny 完成两分钟 AT、蜂窝注册和 TCP 短窗测试，任务栈余量达标；30 分钟运行和电平复测待完成。[证据](sticks3-ml307r-at/docs/validation.md) |

[StickS3 GPS → MotoBox](../projects/m5stack-sticks3-gps-motobox/) 按完整定位终端的功能范围
归入 `projects/`，验证状态仍为 `prototype`。
