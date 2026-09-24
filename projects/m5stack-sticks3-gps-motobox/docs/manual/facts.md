# MotoBox GPS 说明书事实依据

资料快照：2026-09-24，仓库提交 `ae261becbd392fbaf46844fbb999c13eb4ca317c`；主要固件变更提交 `a743599`。制作期间工作区存在其他进行中的固件变更，本手册锁定上述已提交版本，未将这些改动计为验证结果。
平台公开协议与小程序资料快照：MotoBox 仓库 `9b706e3e9fc347ea68d010ae15f0620e5a1d23c7`。
以下只描述 StickS3 K150 + Unit GPS v1.1 / U032-V11。项目状态仍为 `prototype`；制作说明图没有增加硬件验证。
逐文件内容哈希记录在 [生成记录](generation.md)，以便检查后续固件变化是否需要更新说明。

**同日后续核验：** 图稿与 F01–F14 仍对应原快照；`0748e80` 的验证记录新增 `fec6e12` 户外静止定位证据。新的无定位界面在已有 GGA 时显示 `NO FIX  SAT …`。以下 V03 保留为原快照状态，V04 是后续补记，不能将旧图视为新固件截图。

## 硬件与当前用途

| ID | 结论 | 层级与来源 | 限制 |
| --- | --- | --- | --- |
| H01 | StickS3 K150，ESP32-S3-PICO-1-N8R8；8 MB Flash、8 MB PSRAM；2.4 GHz Wi-Fi | [官方 StickS3 文档](https://docs.m5stack.com/en/core/StickS3)、[产品记录](../../../../catalog/vendors/m5stack/products/sticks3-k150.yaml) | 不与 M5StickC / Plus / Plus2 混用配置。 |
| H02 | 1.14 英寸 LCD，原生 135×240；当前项目使用 240×135 横屏 | 官方文档、[ui.c](../../firmware/main/ui.c) | 说明图中的屏幕为源码信息示意，非实机截图。 |
| H03 | A = KEY1 / GPIO11；B = KEY2 / GPIO12 | 官方文档、[板卡档案](../../../../boards/m5stack-sticks3-k150/README.md)、[board.h](../../firmware/main/board.h) | 当前项目仅读取 A；板卡档案中“给油/换车型”等动作属于另一项目，不适用。 |
| H04 | USB-C DC 5 V 供电与烧录；标称内置电池 250 mAh | 官方文档 | 当前 GPS 组合电池续航、峰值电流未测。首测使用 USB-C。 |
| H05 | 板载 ES8311 + AW8737 + 扬声器；当前用于播报绑定验证码 | 官方文档、[语音实现](../../firmware/main/binding_announcement.c)、[验证记录](../validation.md) | 不推导为语音助手、录音或实时导航播报。 |
| H06 | IMU、麦克风、红外、磁吸、HAT2 扩展属于板载能力 | 官方文档及用户提供硬件介绍图 | 当前 GPS 固件未用这些能力；不得画成防盗、姿态检测、语音输入、红外控制等已实现功能。 |
| H07 | 侧面电源/复位键：单击开机/复位、双击关机；连接 USB 后长按进入下载模式，绿灯闪烁为提示 | 官方文档的 Button Operation Instructions / Download Mode，2026-09-24 读取 | 厂商操作说明；本轮没有按键实测，不指定未提供的长按秒数。 |
| H08 | Unit GPS v1.1 使用 ATGM336H-6N / AT6668，UART 默认 115200 8N1 | [官方 Unit 文档](https://docs.m5stack.com/en/unit/Unit-GPS%20v1.1)、[产品记录](../../../../catalog/vendors/m5stack/products/gps-unit-v1-1.yaml)、[main.c](../../firmware/main/main.c) | GPS 来自外接 Unit，不是 StickS3 内置。 |
| H09 | 黑 GND↔GND，红 5V→5V，黄主机 GPIO9/TX→Unit RX，白 Unit TX→主机 GPIO10/RX | [项目接线表](../../README.md#wiring-and-power)、官方端口定义、board.h | 表格说明信号方向，不是从任意视角看插座的左右针序。按防呆和丝印确认。 |
| H10 | 固件开启 M5PM1 的 Grove 5 V 输出；开启后不得再给 Grove 红线外灌 5 V；GPIO 为 3.3 V | [board.c](../../firmware/main/board.c)、官方 EXT_5V_EN 说明、项目 README | 不把 5 V 信号直接接 MCU GPIO。 |

## 按键、显示和联网

| ID | 结论 | 层级与来源 | 限制 |
| --- | --- | --- | --- |
| F01 | A 按下边沿切换状态页/绑定页；B 没有应用按键动作 | [ui.c](../../firmware/main/ui.c) refresh_timer / ui_init | 没有 A 长按刷码、B 翻菜单、AP 配网、音量设置等 UI。 |
| F02 | `GPS MODULE ONLINE` 表示最近 5 秒内收到有效 NMEA；超过 5 秒显示 `NO DATA` | main.c snapshot_status、ui.c | 模块在线不等于定位成功。 |
| F03 | 有有效定位显示 `FIX READY`、卫星数 `SAT`、速度 `km/h`；模块在线但无定位显示 `FIX WAITING` | ui.c | 无模块数据时显示 `CHECK GPS POWER / CABLE`。 |
| F04 | Wi-Fi 行显示 UP/DOWN 与 SSID；下一行是本机 IP 与 RSSI dBm，未连接则 `IP -- CONNECTING` | ui.c、main.c | 公共图片中不显示真实 SSID/IP。RSSI 更接近 0 通常表示接收信号更强，但不等同网络服务正常。 |
| F05 | MQTT UP/DOWN 与 Wi-Fi 状态分开；绑定有 CHECK/UNBOUND/BOUND | ui.c、[绑定协议](../../firmware/main/binding_protocol.c) | CHECK 表示尚未得到可信状态，不等于未绑定。 |
| F06 | MQTT 连接后先查是否已绑定；只有明确未绑定才请求六位服务器验证码，自动切到绑定页并播报 | main.c mqtt_event、ui.c、binding_announcement.c | 已绑定不请求/播报新码；A 只切页，不强制刷新码。 |
| F07 | 绑定页在未拿到码时显示六个横线；有真实码时显示码；已绑定时显示 BOUND | ui.c | 本说明书用 `------` 作占位，不展示可用验证码；页内“示意”标记优先。 |
| F08 | README 说明验证码有效 10 分钟、使用一次；固件以服务器 expires_ms 判定到期 | README、main.c、binding_protocol.c | 10 分钟是当前服务协议约定，不是按钮计时功能；没有证据时不声称屏幕有倒计时。 |
| F09 | 约每 10 秒查询绑定状态；服务确认后显示 BOUND，重连/重启无需重复绑定 | main.c、验证记录 | 当前后端须支持 binding-status 协议；旧后端可能停留 CHECK。 |
| F10 | Wi-Fi/热点与设备 MQTT 凭据写入本地配置并在烧录时配置 | [配置示例](../../firmware/sdkconfig.local.defaults.example)、[Kconfig](../../firmware/main/Kconfig.projbuild)、README | 只读示例，未读取实际凭据；没有 AP 网页或手机蓝牙配网。设备服务凭据须单独开通。 |
| F11 | 数据路径：Unit GPS → UART → StickS3 → 2.4 GHz Wi-Fi/热点 → 互联网 → MotoBox → 微信小程序 | README、main.c | 没有蜂窝模块；热点需能访问互联网。小程序登录和版本资格须由服务方提供。 |
| F12 | 默认每 5 秒生成一次遥测，MQTT QoS 1；启动后须先有可信时间 | main.c、Kconfig、[payload](../../firmware/main/telemetry_payload.c) | 间隔可配置；无有效定位时仅报状态、不报 location。 |
| F13 | 120 帧 RAM 队列；恢复后按顺序补传，收到 PUBACK 才移除；掉电/重启丢失未发缓存 | [queue.h](../../firmware/main/telemetry_queue.h)、[queue.c](../../firmware/main/telemetry_queue.c) | 不是 SD 历史轨迹。默认 5 秒间隔约 10 分钟容量；队列满会丢最旧未在途帧，不保证长期离线无损。 |
| F14 | 有效定位要求 RMC/GGA 时间及坐标匹配、卫星≥4、0<HDOP<20，且位置数据不超过 5 秒 | [gnss_parser.c](../../firmware/main/gnss_parser.c)、main.c | 属于固件过滤条件，不代表实测定位精度或固定首次定位时间。 |

## 验证与公开边界

| ID | 结论 | 来源 | 范围 |
| --- | --- | --- | --- |
| V01 | 构建、室内联网、状态遥测、断网恢复已有记录；语音验证码和小程序实机绑定已验证 | [validation.md](../validation.md) | StickS3 K150 + Unit GPS v1.1，USB-C 供电并开启 Grove 5 V；具体时长与固件见原记录。 |
| V02 | 状态版固件及后端更新后，已有 bound=1 与重启后仍绑定且不播新码的串口证据 | validation.md，固件 a743599，后端 38ca40e | 新 LCD 排版未目视验收；故意断开 MQTT 后的 LCD 变化、解绑后重绑仍待检查。 |
| V03 | 原 `ae261be` 快照时，户外有效定位、移动轨迹、小程序位置/分享页面验收尚未完成 | validation.md @ae261be、[project.yaml](../../project.yaml) | 这是图稿制作时状态，后续变化见 V04。 |
| V04 | `fec6e12` 后续固件取得户外静止定位；后台连续接收 126 帧有效 GNSS 位置，约 10 分钟 26 秒 | validation.md @0748e80，2026-09-24 | 同型号、USB-C / Grove 5 V；移动轨迹、定位精度与微信分享完整链路未验收。户外无串口记录，较早序号重置原因未知。项目仍为 prototype。 |

## 平台注册、原理与分享

本节只摘录公开协议和终端用户流程，不复制后台内部实现、部署拓扑或账号配置。

| ID | 结论 | 公开依据（MotoBox 9b706e3） | 边界 |
| --- | --- | --- | --- |
| P01 | 小程序通过微信登录请求平台，平台查找或创建用户并返回登录会话；手机号不是当前入口必填项 | [小程序 README](https://github.com/zhoushoujianwork/motobox/blob/9b706e3/mobile-integration/wechat-miniprogram-app/README.md)、[登录页面](https://github.com/zhoushoujianwork/motobox/blob/9b706e3/mobile-integration/wechat-miniprogram-app/pages/login/index.js)、[公开协议](https://github.com/zhoushoujianwork/motobox/blob/9b706e3/docs/06-backend/BACKEND_INTEGRATION.md) | 未新建账号、未测试生产注册；小程序正式/体验发布资格仍需确认。 |
| P02 | 公开协议包含账号密码注册/登录 API；Demo 用户可直接走微信登录建号/关联 | 公开协议的账号章节 | 不声称存在已开放的自助注册网页；不把注册用户等同设备 MQTT 凭据开通。 |
| P03 | 设备主人在设备详情进入“分享位置”，先“生成链接”，成功后“分享”给好友/群 | [设备详情](https://github.com/zhoushoujianwork/motobox/blob/9b706e3/mobile-integration/wechat-miniprogram-app/pages/device-detail/index.wxml)、[分享页面](https://github.com/zhoushoujianwork/motobox/blob/9b706e3/mobile-integration/wechat-miniprogram-app/pages/device-share/index.wxml) | 当前 UI 与接口实现，不代表 StickS3 户外位置分享已验收。 |
| P04 | 邀请链接默认 24 小时有效，有效期内可多人接受；接收人需登录，加入后以 viewer 只读看位置/轨迹 | 小程序 README、[分享逻辑](https://github.com/zhoushoujianwork/motobox/blob/9b706e3/mobile-integration/wechat-miniprogram-app/pages/device-share/index.js)、公开协议 | 邀请有效期不代表已经授予的访问权自动在同一时间失效。 |
| P05 | 设备主人可列出并移除共享成员；接收人没有设备所有权或二次授权管理入口 | 分享页面、分享逻辑 | 本次未对真实成员执行增加/移除操作。 |
| P06 | 小程序实时位置采用约 5 秒轮询；设备默认约 5 秒遥测采样；两个周期独立 | [客户端配置](https://github.com/zhoushoujianwork/motobox/blob/9b706e3/mobile-integration/wechat-miniprogram-app/utils/config.js)、本项目 Kconfig | “准实时”不是固定 5 秒端到端延迟保证。 |
| P07 | 模块输出 WGS-84；客户端地图按需转换为 GCJ-02 | [坐标处理](https://github.com/zhoushoujianwork/motobox/blob/9b706e3/mobile-integration/wechat-miniprogram-app/utils/coord.js)、本项目 README | 不复用其他车机的基站/Wi-Fi 定位、惯导融合或 4G 能力。 |
| P08 | 平台保留最新有效定位与历史点供已授权用户查看；设备“在线”与“有有效位置”分别表达 | 小程序 README、公开协议 | 最新有效点可能是历史点，不能将它说成车辆此刻位置；结合时间和在线状态查看。 |

## 素材与来源

- 用户提供的 T-LITE 手册只参考信息组织，不使用其产品、按键功能、菜单、无线模式、网址和二维码。
- 用户提供的 StickS3 硬件图用于外观身份核对。官方同图：[StickS3 主图](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150-stickS3_main-products_01.webp)。
- Unit GPS 外观依据：[官方产品图](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/products/unit/Unit-GPS%20v1.1/4.webp)。
- 参考原图未纳入 Git。M5Stack 商标及参考素材归原权利人所有；输出是 AI 图解示意，不是厂商官方手册、实机照片或精确工程图。
