# 验证记录

## 2026-09-25 构建里程碑

- 目标：ESP32-DevKitC / ESP32-WROOM-32E；实际 ROM 为 ESP32-D0WD-V3 v3.1，4 MB Flash，
  CP2102N USB-UART。PCB 丝印版本待人工核对。
- 工具链：ESP-IDF v5.5.2，目标 `esp32`。固件 v0.1.0 构建通过；应用约 940 KiB，
  1500 KiB 应用分区余约 37%。运行时版本为 `v5.5.2-dirty`：本机 IDF 的未使用 OpenThread
  子模块指向不同提交，`git diff` 未显示 IDF 主仓库源码改动；本固件不使用 OpenThread。
- 主机测试 2/2：BOOT 去抖/长按/释放/64 位计时，44.1 kHz 全配置音频过渡。
- 此时实际音箱尚未到场，原计划使用 JBL Go 3。配对、SBC 传输、实际出声、重连、端到端延迟未验证。

## 2026-09-25 初版 USB 烧录与无音箱实机检查

- 固件源码提交：`825cf3638a2ed41f74354b14a6da9b225814b7a8`，固件版本 `0.1.0`。
  应用二进制 962864 字节，SHA-256：
  `e284445d5df576e97766e08b31a3da2f604746681d93b5af6f48d99b23181868`。
- 板卡为上文 ROM 识别的 ESP32-D0WD-V3 v3.1 / 4 MB，由电脑 USB 供电，CP2102N 提供串口通信；
  无外接 GPIO、DAC、功放、车辆电源或音箱。PCB 丝印修订仍待人工核对。
- 原 Flash 4 MB 已完整保存至仓库外私有目录。460800 波特率读备份出现 packet corruption，
  降为 115200 后读取完成；这不是固件运行期间观察到的复位证据。烧录使用 115200，
  bootloader、分区表和应用三段均 Hash 校验通过。
- 按验收标准完成 **120 秒持续串口捕获**；开始时工具主动脉冲 EN，启动日志为
  `POWERON_RESET` / `reset_reason=1`，之后没有再次启动或心跳时间回退。
- 116 个心跳连续，合成计数递增；双缸升至约 9000 RPM，V12 自定义红线 16000 RPM 时
  达到 **15998 RPM**。给油、收油、3 秒自动熄火和三种排气选择的日志/非零 PCM 峰值可见。
  结束为双缸、原厂排气、20% 音量、0% 油门、0 RPM、OFF。
- `errors=0`、`dropped=0`；没有 stack overflow、Guru Meditation、panic、Brownout、
  任务启动失败、复位循环、串口断开或心跳中断。最低可用堆 **122472 B**。
- 最大合成 **3303 µs / 256 帧**，44.1 kHz 对应每块预算约 5805 µs。最低任务剩余栈：
  synth **4316 B**、manager **4212 B**、console **6312 B**、heartbeat **2060 B**。
  全任务快照另含 BTC/BTU/控制器，但 **SBC 编码任务和音频数据回调尚无带载测量**。
- 持续扫描按期开始/停止；`A2DP_READY` 已出现。但始终 `connected=0 streaming=0`、
  PCM `callbacks=0 bytes=0`，因此 **不能认定 A2DP 连接或声音外放通过**。
- 串口非法 profile、101% 油门和 NaN 音量均拒绝；验证器能拒绝无流数据冒充蓝牙播放、
  注入的 panic 和任务启动失败记录。原始日志为本地忽略文件 `build-evidence/offline.log`。
- 120 秒记录内未有人按下实体 BOOT，机械按键验收仍待完成。
- 复查完整协议栈日志发现 `btc_a2dp_call_handler : unhandled event: 10`。API 返回 ESP_OK
  只代表事件入队，不能证明已应用：ESP-IDF 5.5.2 内置编码模式不处理自定义 SEP 注册。
  已在 `52bd4d2` 移除该调用，保留原本就是 44.1 kHz 的默认 SBC Source 端点；同时完善
  验证器，把 unhandled event 判为失败。上述原日志重判会失败，不能作为最终固件全项通过。
- 初版另做 40 秒 GPIO0 电气输入测试：通过 CP2102N DTR/自动下载电路触发两次 3 秒低电平，
  均收到 PRESS/RELEASE、约 8999 RPM、收油、3 秒自动熄火与 OFF；38 次连续心跳。
  这是电气输入路径证据，不是人手按键证据；同样须用修订固件复测。

## 2026-09-25 修订版复测（当前板上固件）

- 固件提交：`52bd4d24f6a74c133ce0ace374efe56fd430b438`，版本 `0.1.0`；板卡、USB 供电、
  工具链和无音箱条件同上。最终应用 **962880 B**，SHA-256：
  `d2315e71f636d03f012946beab4674dcb773d8fc6a22e4e10089ae00f64bcc78`。
  115200 波特率重新烧录应用并通过 Hash 校验，bootloader/分区表未变。
- **120 秒控制/合成复测通过**：116 个连续心跳、V12 达 15998 RPM，API errors 和 dropped
  均为零；非法命令均拒绝；最终 0 RPM/OFF。无 fatal、panic、栈溢出、额外复位、USB 中断、
  ESP-IDF 错误日志或 unhandled event。测试开始主动复位一次，后续时间戳连续。
- 该段最小堆 **122332 B**；栈余量 synth **4220 B**、manager **4228 B**、console
  **6296 B**、heartbeat **2060 B**。最大合成墙钟时间 **8169 µs**，在第一条心跳即已出现，
  后续不再增长；它高于单块 5805 µs 时间预算，墙钟计时包含可能的抢占，峰值原因未定位，不能据此宣称
  蓝牙带载稳态无欠载。真正编码/发射时仍需检查 PCM 缓冲和欠载计数。
- **40 秒 GPIO0 电气路径复测通过**：DTR 两次拉低/释放，每次持续 3 秒，记录到两组
  `BOOT_BUTTON PRESS/RELEASE`；转速到 8999 RPM，松开回怠速，3 秒后自动熄火，最终 OFF。
  38 个连续心跳；该段最大合成 **3350 µs**，最小堆 **122348 B**，最低剩余栈 synth
  **4312 B**、manager **4224 B**、console **6296 B**、heartbeat **1960 B**。
- 两段均只有捕获起始时的一次启动，GPIO0 脉冲期间没有复位。驱动打开串口可能触发
  EN 瞬态，因此起始复位与运行中的异常重启分别判断。
- 全任务快照中的最低观测值：BTC **4544 B**、BTU **4624 B**、控制器 **2116 B**、
  HCI **1556 B**。这些只覆盖扫描负载；SBC 编码/PCM 回调未运行，回调栈仍为未测量。
- 唯一预期的 BT 启动提示为 `A2DP Enable without AVRC`：本 Demo 有意不启用 AVRCP
  遥控/音量功能，乐鑫 A2DP API 说明支持独立运行；此提示不等于已验证音箱兼容性。
- 最终原始日志位于本地忽略目录的 `offline-final.log`、`boot-gpio-final.log` 及对应摘要。
  **GPIO0 电气触发不代替机械按键验收；蓝牙接收设备的配对、实际出声、持续播放、重连和延迟仍待测。**
  Manifest 保持 `build-verified`、`validation.hardware: partial`，不升级为完整蓝牙硬件验证。

## 2026-09-26 Beats Flex 首轮扫描

- 沿用当前板上固件提交 `52bd4d24f6a74c133ce0ace374efe56fd430b438` 与上文应用 SHA；
  原版 ESP32-DevKitC / ESP32-WROOM-32E，ROM ESP32-D0WD-V3 v3.1，CP2102N，电脑 USB 供电。
  PCB 丝印版本仍待人工核对。接收设备改为截图标示的 `Beats Flex` 蓝牙耳机。
- 通过 USB 串口发送 `peer Beats Flex`，收到 `CMD peer saved; exact name matching enabled`；
  名称写入板上 NVS。捕获约 90 秒，87 个连续心跳、8 次扫描开始、7 次扫描结束；
  没有 `TARGET_FOUND`、A2DP 连接或音频状态事件，PCM 回调为 0。
- 该段无 ESP-IDF 错误日志、panic、Guru Meditation、栈溢出或任务启动失败，心跳未回退。
  这只证明目标已保存且扫描运行，**不证明耳机进入配对模式、蓝牙连接或声音外放**。
  测试时耳机的可发现状态尚未确认，待其指示灯闪烁后复测。
- 原始串口记录保存在本地忽略目录 `build-evidence/beats-flex-probe.log`；没有提交设备日志。

## 2026-09-26 配对窗口复扫与 AP 原型

- 用户确认 Beats Flex 指示灯闪烁后，沿用前版固件再次捕获 **120 秒**：
  `TARGET_FOUND=0`、A2DP 连接事件为 0、PCM 回调为 0；无 fatal。用户观察到耳机的
  配对闪烁会自行结束，因此不能假定整段捕获都在可发现窗口内。原始记录为本地
  `build-evidence/beats-flex-pairing.log`。电脑的独立 Classic Bluetooth inquiry 也未发现
  新设备；电脑已保存的“未连接”耳机记录不能替代实时可发现性证据。
- 改造固件：扫描轮次间隔由 3 秒缩为 250 毫秒，每轮汇总收到的 Classic 设备数/有名称数/
  Beats 名称候选数；优先使用完整广播名。新增临时 WPA2 SoftAP 配网页面，展示最近 30 秒
  扫描结果，点选后通过 manager 队列请求连接；页面不显示蓝牙地址。AP 密码每次启动随机
  生成，只在私有串口日志输出，不进仓库。
- AP 固件源码提交：`39a57f1c43f1d2f25b830c7a4051f85700e07fb5`；应用 SHA-256：
  `1b788d8993102978d9b8d0e77caedcece9557b67e8be499b0e8b09f648401657`；
  4 MB Flash、电脑 USB 供电、同一 DevKitC 板；以 115200 波特率烧录，应用 Hash 验证通过。
  应用大小 **1495056 B**，1.5 MB 应用分区余约 **40944 B（3%）**。
- 烧录后 AP 在串口报告 `AP_READY`，本次临时 SSID 为 `EV-Engine-Setup`，页面地址
  `http://192.168.4.1/`；密码只在本地日志中保留。**120 秒 AP + 持续扫描**检查通过：
  116 次连续心跳，无 reset loop、fatal、ESP-IDF 错误、事件队列丢失或 API 错误。
  11 轮 `INQUIRY` 均为 `results=0 named=0 beats_named=0`，所以尚不能验证页面列表和点选路径。
- 无音频负载期间最低堆 **37248 B**，最低剩余栈：synth **5364 B**、manager **4236 B**、
  console **5496 B**、heartbeat **2004 B**。最大合成墙钟 **126146 µs**，发生于 AP 启动
  附近且随后最大值未增长；它超过 256 帧约 5805 µs 预算，故 AP + A2DP 同时播放的连续性
  与 Wi-Fi/HTTP/SBC 任务栈仍需带载验证。没有连接、音频状态或 PCM 回调，不能认定出声。
- 页面实际从手机打开、列表显示、手动点选、Beats Flex 配对与人耳听感均待用户联测。
  AP 联测日志为本地忽略文件 `build-evidence/ap-soak.log`，不提交临时密码或设备地址。

## 2026-09-26 本地数字 AP 密码

- 固件源码提交：`8a05655a3386b151da9c4362ff9f4f12e3527b15`；应用 SHA-256：
  `cd769a7de5488bed780a672daf12a714080035e496178df833e7e486eb5082a7`。
  同一 ESP32-DevKitC、电脑 USB 供电，115200 波特率只更新应用分区，写入 Hash 验证通过；
  NVS、bootloader 和分区表未覆盖。应用大小 **1495744 B**，分区余 **40256 B（约 3%）**。
- 用户要求的八位数字密码经串口 `ap_pin` 设置到板上 NVS，日志收到 `CMD ap_pin saved`；
  设置路径中 console 最低剩余栈 **5496 B**。固件按设计软件重启一次，随后日志出现
  `AP_READY ... password_source=NVS`，确认 AP 已加载本地密码。实际密码值不记入仓库或
  验证记录，原始串口日志只留在本地忽略文件 `build-evidence/ap-pin-config.log`。
- 重启后持续观察 **111 个心跳**，没有额外重启、fatal、ESP-IDF 错误；最低堆 **37260 B**。
  最低剩余栈：synth **5360 B**、manager **4280 B**、console **5496 B**、heartbeat
  **2144 B**。10 轮扫描的设备数仍为 0，尚未验证手机使用该密码成功加入 AP、页面交互、
  Beats Flex 配对或音频播放。

## 实机验收标准

1. 保存固件提交、应用 SHA、芯片/PCB 修订、USB 供电方式及捕获时长。
2. 无音箱启动和持续扫描时，120 秒以上串口连续：每秒心跳、合成帧递增、API 错误与
   丢事件为零、应用任务最低剩余栈大于 1024 B；无 panic、Guru Meditation、栈溢出、
   任务启动失败、Brownout、复位循环或 USB 反复重枚举。
3. 实体 BOOT 按下/松开分别有日志；按住转速升高、松开回落、3 秒后停机，开机默认关闭。
4. 音箱到场：名称匹配、鉴权、A2DP 连接、媒体开始、PCM 回调持续增长；稳定负载至少
   10 分钟，观察 SBC/BTU/BTC/合成/心跳所有任务低水位、欠载、内存和音频连续性。
5. 人耳确认启动、怠速、给油和收油；测试音箱关机/重启、距离断连和重连后的静音安全状态。
6. 录像/音频采集或仪器测 BOOT 至声音变化的实际延迟，不能以接收器 Delay Report 代替。

原始串口日志和完整 Flash 备份只留本地，提交脱敏摘要。本 Demo 不使用 SD，没有可检查的
SD 启动日志；BLE/SD/4G 同时负载仍不在验证范围内。
