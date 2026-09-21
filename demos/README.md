# Demos

这里放单一模块、接口或硬件组合的最小可复现实验。新 Demo 从
[`templates/demo/`](../templates/demo/) 开始，目录名使用小写 `kebab-case`。

建议示例：

- `at6668-gps-uart`：只验证 UART、NMEA、定位质量和时间；
- `n9301-audio-player`：只验证 SD 文件播放与 UART 控制；
- `dual-button-input`：只验证去抖、长按和组合键。

建议名称不代表已经实现或拥有现货，真正创建时必须填写验证状态。

## Available demos

- [`m5stack-sticks3-gps-motobox`](m5stack-sticks3-gps-motobox/)：StickS3 读取 AT6668 GNSS，
  通过 Wi-Fi 和 MQTT TLS 上报到 MotoBox；当前为 prototype，等待实机验收。
