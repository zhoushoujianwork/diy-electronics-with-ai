# Demos

这里放单一模块、接口或硬件组合的最小可复现实验。新 Demo 从
[`templates/demo/`](../templates/demo/) 开始，目录名使用小写 `kebab-case`。

建议示例：

- `at6668-gps-uart`：只验证 UART、NMEA、定位质量和时间；
- `n9301-audio-player`：只验证 SD 文件播放与 UART 控制；
- `dual-button-input`：只验证去抖、长按和组合键。

建议名称不代表已经实现或拥有现货，真正创建时必须填写验证状态。

目前暂无独立 Demo。[StickS3 GPS → MotoBox](../projects/m5stack-sticks3-gps-motobox/)
已按完整定位终端的功能范围归入 `projects/`，验证状态仍为 `prototype`。
