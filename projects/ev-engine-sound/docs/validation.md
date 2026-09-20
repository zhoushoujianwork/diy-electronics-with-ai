# 验证记录与硬件验收

## 2026-09-17 StickS3 实机验证

M5Stack StickS3 K150 原 8 MB Flash 已备份，新固件写入并逐段 Hash 校验。
67 秒串口负载测试通过：66 连续心跳、V12 实测 15999 RPM、约 28.6 FPS，
零音频写入错误/故障；最低剩余栈 audio/console/heartbeat/LVGL 为
3808/8064/1600/5964 B。用户确认屏幕和声音正常，A 给油/释放和 B 换车型
有实体操作回读。设置页与 A+B 仍待实体按键验收。
本版无 SD/BLE/4G，无这些负载测试或 SD 启动日志；完整证据和备份摘要见
[`sticks3.md`](sticks3.md)。

## 实现范围

第一版采用轻量程序合成，先验证 ESP32 输出链路和转速控制。未来可以混合桌面 Engine Simulator 导出的采样层。
无堆分配音频内环；块大小 256 帧 / 32 kHz = 8 ms。DMA 为 6 个块；最多约 48 ms 的排队缓冲，实际输出延迟需真机测量。
I²S timeout 使用毫秒；部分写入按实际字节偏移推进；失败锁定静音，避免重复发送同一缓冲形成速度错误。

## 栈预算

ESP-IDF task stack 参数与 `uxTaskGetStackHighWaterMark` 都以字节计。

| 任务 | 分配 | 最大路径及局部数据 | 真机要求 |
| --- | --- | --- | --- |
| voice_audio | 6144 B | render + I²S write + 异常日志/TinyUSB tee；控制快照/状态小于 256 B，2560 B PCM 缓冲为静态存储 | 实测最低剩余 4152 B，最大 render 920 µs |
| voice_heartbeat | 4096 B | 状态快照 + 三任务栈查询 + 浮点/整数日志格式化 + 256 B TinyUSB tee 格式缓冲 | 实测最低剩余 1732 B，1 s 心跳持续 |
| app_main / console | 10240 B | TinyUSB CDC 接收、96 B 行、32 B 输入块、命令解析 sscanf/strtof、面板/LVGL/触摸初始化和日志 | 新界面实测最低剩余 7968 B |
| taskLVGL | 10240 B | 18 卡片横滑列表、三类机械自绘、触摸回调、状态查询和 33 ms 动画刷新 | 机械界面实测最低剩余 5052 B |

以上是按调用路径给出的保守初始分配，不能代替板上测量。状态查询不遍历全系统任务；大缓冲不放任务栈。
心跳不依赖音频写完成；音频任务一秒无进展会锁定故障并关闭可控功放。
本版没有 SD/BLE/4G；接入这些模块后必须在并发压力下重新测栈、音频吞吐和心跳，不能沿用本次未测的余量结论。

## 电脑验证

- CTest：初始静音、非法命令无状态修改、固定 RPM 的点火计数、270°/450°及315°/405°间隔、转速收敛、停止后静音、4种模型各30秒持续运行。
- AddressSanitizer / UndefinedBehaviorSanitizer：同一测试集。
- WAV：4种模型各10秒；检查采样率、PCM编码、长度、峰值、非静音和尾段停声。
- ESP-IDF：分别编译默认无输出模式和启用参考引脚的 I²S 模式。

## 2026-09-14 实际结果

- CTest 通过；UndefinedBehaviorSanitizer 通过。
- 8 项串口日志检查器测试通过（合成故障样本，不是真机日志）。
- 四份 WAV 均为 PCM16、32 kHz、单声道、10秒；峰值 8344–10061，末250毫秒全静音。
- ESP-IDF 5.5.2 默认模式、参考 I²S 模式和 8 MB Flash/TinyUSB CDC 真机模式构建通过。
- AddressSanitizer 未完成：本机 AppleClang 17 / macOS 26.5.2 在进入 main 前发生运行时初始化死锁。
  `sample` 显示 `AsanInitInternal → InitializeShadowMemory → get_dyld_hdr → malloc → AsanInitFromRtl → StaticSpinMutex`。
  已终止测试进程，诊断保存在 `build/sanitizer-sample.txt`；此项不算通过。
- 原 8 MB Flash 已完整备份（8,388,608 B，SHA-256 `ba5ce0ebc8f124baae072eca43a61d0265a53abd2ad95fa3b23015ab6a2582b7`），随后整片擦除并烧录。
- 真机串口检查通过：88 个连续心跳、四模型切换、`write_errors=0`、`fault=0`、最大 render 920 µs、三任务栈余量均 ≥1024 B，无 panic/Guru Meditation/stack overflow/重复启动。
- 断开保活测试通过：运行态在约 2 秒后记录 `reason=control_timeout` 并进入 STOP，随后状态查询确认 `running=0`。
- 本版没有 SD/BLE/4G，未验证这些负载并发；实际扬声器响度和音质仍需人耳确认。

## 后续真机验收

1. 人耳确认单缸、270°双缸、V2 和直列四缸的差异、音量与失真；软件日志通过不等于已经验声。
2. 按实际车辆测试怠速到最高转速、快速收油和音量变化，再调参数。
3. 验证非法/超长命令不会改变运行状态；必要时用逻辑分析仪复核 I²S 时钟。
4. 接入 SD/BLE/4G 或车辆输入后，在真实并发负载下重新测栈、音频吞吐和心跳。

## 2026-09-15 挡位与设置页版本

- CTest 已覆盖默认 60% 音量、`gear N|0..6`、120 ms 换挡点火切断、挡位转速映射、
  `redline default|N` 和 15000 RPM 实际收敛；全部通过。
- 立创实战派 N16R8 与历史开发板 8 MB 两套 ESP-IDF 固件均构建通过。
- 新立创固件已写入并校验；串口回读确认 UI、音频编解码器和触控设备在线。
- 1 挡、直列四缸、100% 油门、自定义 16000 RPM 条件下已到达 15994 RPM；期间
  `write_errors=0`、`fault=0`、最大 render 1044 us，LVGL 最低栈余量 3548 B。
- 该轮持续测试约 8 秒时原生 USB CDC 消失且未自动重新枚举；断开前没有 panic、
  Guru Meditation、stack overflow 或软件故障日志。UART 桥仍在但目标无回包，需在
  重新上电后继续抓 reset reason，并完成设置页的实体触摸验收。此项尚不能归因为硬件。
- 本固件没有 SD、BLE 或 4G 任务，因此不存在可检查的 SD 启动日志，也不能把当前栈
  余量外推到未来的 BLE/SD/4G 并发版本。

## 2026-09-15 级联车型选择版本

- 合成表扩展到 1/2/3/4/6 缸共 11 种配置。CTest 新增三缸 `triple270` 的
  270/270/180 度点火间隔断言，并继续覆盖全部模型固定转速点火计数及 30 秒运行。
- 测试发现并修复高转速向较低红线收敛时，单精度步进小于 ULP 导致残留约 8 RPM
  的问题；现在检测到无法推进的浮点步长时会收敛到目标值。
- 真机 `UI_SYNC` 已逐项确认：1 缸 1 个选项、2 缸 5 个、3 缸 2 个、4 缸 2 个、
  6 缸 1 个；`triple270` 显示为 `270 T-PLANE`。
- 六缸 `flat6` 在 1 挡、80% 油门时稳定到 6980 RPM；切回 `triple270` 后红线从
  六缸默认 8500 正确切换到三缸默认 11500 RPM。
- 33 个连续心跳和后续回读中：`write_errors=0`、`fault=0`、最大 render 1554 us /
  16000 us，最低栈余量 audio 3712 B、console 7744 B、heartbeat 1680 B、LVGL
  3564 B，free heap 约 232644 B；多次 ES8311/PCA9557/UI readback 均成功。
- 空闲监控 60 秒、全模型与六缸发声扫描 40 秒均未出现 USB 掉线或时间戳回退。
  级联选择的状态同步和数据面已验证；下拉框实体触摸仍需用户在屏幕上完成最终手感验收。

## 2026-09-16 横滑机械动画版本

- 合成表扩展到 1–12 缸共 18 种配置；每种配置增加实际缸号点火顺序，CTest 检查
  点火顺序为合法排列，并覆盖全部 18 个模型。核心、立创板和历史板构建均通过。
- 下拉选择改为 18 张横向滑动、中心吸附卡片；红线设置改为主页面滑块；删除圆弧
  仪表和挡位控件，只保留一个按住给油按钮。实体触摸已回读 `profile_swipe`、
  `redline_slide`、`engine_toggle` 和 `rev`，其后的 `UI_ACTION` 均为 `result=OK`。
- LVGL 自绘机械视图按直列、V 型和水平对置布局绘制气缸、活塞、连杆、曲轴、
  气门和点火。活塞相位由点火顺序反推，直四为 1/4、2/3 对称运动；机械运动按
  RPM 比例慢放以避免 30 FPS 高转混叠，点火仍使用合成器真实 firing counter。
- V12 在 8500 RPM 连续运行，期间 `write_errors=0`、`fault=0`，最大 render
  1709 us / 16000 us；最低栈余量 audio 3808 B、console 7840 B、heartbeat
  1664 B、LVGL 5052 B，free heap 约 230 KB。未见 panic、Guru Meditation、
  stack overflow、任务启动失败、USB 重枚举或心跳中断。
- 验证结束时已停机、0% 油门、60% 音量，转速自然回落到 0 RPM。

## 2026-09-16 精细机械与柔和音频过渡

- 机械模型采用固定长度连杆（3.5 倍曲柄半径）、720 度四冲程相位、双活塞环和
  气门动作。测试覆盖 18 种模型每缸 720 个相位的连杆长度、活塞行程边界和
  直四内外成对运动；V 型改为并排缸对，按模型使用 45/60/72/90 度夹角。
- 新增启动、负载、增益与收油余韵包络；16 kHz 和 32 kHz 各跑全模型
  启动/给油/收油/停机/重启测试，原有音频测试也通过。PCM 无削顶，转换测试的
  最大相邻采样差分别为 8598 / 8116（PCM16）。这些是数据检查，不代替实际听感。
- 测试发现旧 sub-ULP 修正会追随缓慢变化的目标转速而绕过飞轮惯性；改为保留
  浮点小步长余量，收油前 100 ms 不再骤降到目标值。
- 立创板和历史板固件均构建通过。立创板原始逐图元版 V12 测试段的实际刷新为
  (840-639)/(52.107-39.415) = 15.8 FPS；保留画布版同一负载为
  (1116-745)/(78.103-65.124) = 28.6 FPS。这里按真实渲染次数计算，不把 16 ms
  定时配置误报为 60 FPS。启动全屏刷新也包含在 render_max_us 中。
- 画布版 67 秒串口测试收集 66 个连续心跳；V12 自定义 16000 RPM 稳定到
  15999 RPM，`write_errors=0`、`fault=0`，音频最大 render 2837 / 16000 us。
  最低栈余量 audio 3832 B、console 7832 B、heartbeat 1720 B、LVGL 5172 B。
  新增 74240 B 静态画布和双 40 行 DMA 缓冲后，剩余堆约 130 KB；帧缓冲不在栈上。
- 串口确认 AUDIO OFF → STARTING → IDLE → ACCEL → HOLD → COAST → IDLE
  和 STOPPING → OFF；未见 panic、Guru Meditation、栈溢出、错误锁定或重启循环。
- 复现脚本：`python3 tools/verify_device.py PORT --log build-core-final/device.log`。
  它持续捕获日志、发送定时命令和保活，并断言关键转换、转速、栈与错误计数。
  本轮原始日志为 `build-core-final/refined-device.log` 和
  `build-core-final/refined-canvas-device.log`（本地忽略文件）。
- 16 ms 触摸采样、6 px 滑动识别与缓出吸附已实现；实体手指拖动手感及扬声器
  听感仍需用户确认。没有 SD/BLE/4G 功能或 SD 启动日志，不代表这些并发负载已验证。
- 最终补入停机卡片吸附后的画布重绘修正，再烧录并完成 67 秒测试：66 个连续
  心跳，V12 15999 RPM；最大音频 render 3128 / 16000 us，最低栈余量
  audio 3864 B、console 7832 B、heartbeat 1720 B、LVGL 5092 B。实际刷新
  (1116-746)/(61.303-48.353) = 28.6 FPS。日志见
  `build-core-final/refined-final-device.log`；结束为直四、停机、0 RPM、0% 油门、60% 音量。
