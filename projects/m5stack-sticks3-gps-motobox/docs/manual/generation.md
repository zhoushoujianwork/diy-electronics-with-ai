# 生图与校对记录

## 交付清单：4 / 4

采用 **Codex 内置 image_gen**：四张首次生成，第 2 张进行一次定向修正，共五次调用。未使用 CLI/API Key；工具没有披露具体后台模型版本。
最终是三页操作手册（01/03、02/03、03/03）与独立“原理篇”，没有把补充页错误编号为 04/03。
提示词曾请求更高分辨率；下表是实际产物，**不宣称 4K、300 DPI 或印刷验收通过**。

| 文件 | 实际 PNG 像素 | 字节数 | 状态 |
| --- | --- | ---: | --- |
| [01-hardware.png](01-hardware.png) | 1672 × 941 | 1,610,997 | 已生成并目视检查 |
| [02-screen-and-controls.png](02-screen-and-controls.png) | 1672 × 941 | 1,711,361 | 已生成并目视检查 |
| [03-setup-and-troubleshooting.png](03-setup-and-troubleshooting.png) | 1672 × 941 | 1,647,865 | 已生成并目视检查 |
| [04-solution-principles.png](04-solution-principles.png) | 1672 × 941 | 1,680,445 | 已生成并目视检查 |

所有最终图片均保存在本目录。说明书入口为 [README.md](README.md)，功能和来源依据见 [facts.md](facts.md)。

## 输入图角色与素材边界

- 第 1 张：用户提供的 StickS3 硬件介绍图 = 设备身份；M5Stack 官方 Unit GPS v1.1 产品图 = 配件身份；用户提供的 T-LITE 说明书 = 版式参考。
- 第 2 张：本次第 1 张 = 配色和版式参考；StickS3 硬件图 = 按键外观依据。界面内容来自源码，不是设备屏幕照片。
- 第 2 张修正：第一次实际输出 = 编辑目标；只澄清左右屏幕是两个不同时刻的状态示例，并移除误继承的多余底栏。
- 第 3、4 张：第 1 张 = 系列版式；StickS3 与 GPS Unit 图 = 身份依据。平台/手机使用流程图标，不绘制不存在的注册网站、菜单或生产地图。
- 官方素材链接和归属见 facts.md；参考原图没有复制入 Git。模型重绘的外壳背面标签、屏幕、网络图标不是丝印、端口针序或 UI 像素精度的证明。

## 逐页目视检查

| 页面 | 结果 | 检查范围 |
| --- | --- | --- |
| 01 硬件与接线 | PASS | 黑色 StickS3、蓝色 A 键、白色 GPS Unit 与参考相符；引线对应屏幕、A、USB-C 与后部 Grove。表格逐项核对黑 GND、红 5 V、黄 GPIO9/TX→RX、白 GPIO10/RX←TX；供电警示、B 未使用与续航未测保留。背面仅为结构示意。 |
| 02 按键与状态 | PASS（修正后） | 逐项核对英文状态、中文解释、A 切页/B 无功能及三个状态区别。左右屏幕明确标为独立示例；SSID/IP/设备名是占位，绑定页没有真实六位码；保留 LCD 尚未实机目视验收的限定。 |
| 03 上手与排查 | PASS | 核对 UART→Wi-Fi→互联网→平台链路、凭据前提、六步顺序、仅未绑定发码、10 分钟一次性码和 120 帧 RAM 限制；保留小程序资格、无蜂窝与户外待验收。 |
| 原理篇 | PASS | GNSS 负责位置、Wi-Fi 负责上传；用户账号与设备凭据分离；微信邀请流程和只读角色符合公开客户端/协议；默认一天指邀请链接，未声称授权自动到期；已测/待验收分列。 |
| 实际文件 | PASS | 四张 PNG 均存在且已打开查看；实际像素从文件头读取。 |
| 密集文字机器 OCR | NOT CHECKED | 使用目视核对，没有把 OCR 结果当作文字正确的证据。 |
| 实机 LCD、供电、户外定位、真实注册与分享 | NOT CHECKED | 本次未刷机、未做新硬件测试、未注册账号、未分享任何实际位置。既有验证仅引用原记录。 |
| 印刷与可扫描二维码 | NOT CHECKED | 没有印刷验收；图中不包含二维码。 |

这次校对证明说明图与所采用的资料一致，不将插图作为功能实测证据。实际使用以对应版本的精确文字、接线丝印与已验证步骤为准。
材料制作期间其他固件工作继续进行；本版锁定已提交的源版本，不把工作区新功能写成已发布说明。

## 来源快照

交付前复核到 `0748e80`：后续固件新增卫星诊断与户外静止定位证据。四图保留原生成版本，没有重绘成新固件；README 的醒目版本补记及 facts.md 的 V04 说明最新范围。图中旧验收状态不得作为当前项目结论单独转发。

- DIY Electronics with AI：`ae261becbd392fbaf46844fbb999c13eb4ca317c`，主要固件提交 `a743599`。
- MotoBox 公开协议/小程序：`9b706e3e9fc347ea68d010ae15f0620e5a1d23c7`。
- 官方硬件页面访问日期：2026-09-24。
- 下列 SHA-256 通过 `git show <源提交>:<相对路径>` 的内容计算；不读取私人配置、生产令牌或实际验证码。项目 README 的哈希指本次增加手册入口之前的源快照。

| 仓库 | 源文件（仓库相对路径） | SHA-256 |
| --- | --- | --- |
| DIY Electronics with AI | `projects/m5stack-sticks3-gps-motobox/README.md` | `0e3bb77988dcb21822c8f159e77f3986a0810ce9aa96cb08725e0e2d2c86e2bf` |
| DIY Electronics with AI | `projects/m5stack-sticks3-gps-motobox/project.yaml` | `7fd8102f02e7ce74a7ae20fa2e827b57c8db6b3b404985849dfe8ea027debd76` |
| DIY Electronics with AI | `projects/m5stack-sticks3-gps-motobox/docs/validation.md` | `86a17e42aa75fdf14ff5086559a859f470077fbff970bcf2951cbbaddc8bcfad` |
| DIY Electronics with AI | `projects/m5stack-sticks3-gps-motobox/firmware/main/ui.c` | `b63c0035b03138abaa5fb181d987e4ee8b04acd730c6742066c229197b15a1ac` |
| DIY Electronics with AI | `projects/m5stack-sticks3-gps-motobox/firmware/main/main.c` | `9ba3ed786b4dc7ecd212bc4f03aac71010a9b6f659d4645e532be7dfbdafd527` |
| DIY Electronics with AI | `projects/m5stack-sticks3-gps-motobox/firmware/main/board.h` | `bbc652b2ec2a404d301afe03373145f66a564ac47715ff63eef0842dc3e58886` |
| DIY Electronics with AI | `projects/m5stack-sticks3-gps-motobox/firmware/main/board.c` | `1892f117378c958ebaceea1c50bcdce9540b2e3ff324df5cadbe68c2568065d8` |
| DIY Electronics with AI | `projects/m5stack-sticks3-gps-motobox/firmware/main/Kconfig.projbuild` | `2a4133bc359a04b83e76063fad28419d77600797321fc24125fc8e2c9e8b0020` |
| DIY Electronics with AI | `projects/m5stack-sticks3-gps-motobox/firmware/main/gnss_parser.c` | `106b0ffdc5f9a16e281f18d6ab0165ad71c25e650247e7d1f2bc38f8afc5c67c` |
| DIY Electronics with AI | `projects/m5stack-sticks3-gps-motobox/firmware/main/telemetry_queue.h` | `e6b996f5ae48cea0139d2b706017cef0a5e4e929609d8af26f461520bcf9550f` |
| DIY Electronics with AI | `projects/m5stack-sticks3-gps-motobox/firmware/main/telemetry_queue.c` | `606b6ae1b9135c3d3c47c1fbc31a42471f4f40cf9d56e29d225fd8c0c8eaf579` |
| DIY Electronics with AI | `projects/m5stack-sticks3-gps-motobox/firmware/main/binding_announcement.c` | `4f9c43117714b14705b65be12db5a7e33aa206208b45e46e6fc0f64d0fd0b8fd` |
| DIY Electronics with AI | `projects/m5stack-sticks3-gps-motobox/firmware/sdkconfig.local.defaults.example` | `ef843509f089e4d181d0b92b198645e34e5cc42c9b970b8297812486fc0ef4c1` |
| DIY Electronics with AI | `catalog/vendors/m5stack/products/sticks3-k150.yaml` | `704a7e1565dade06df0e7fe418c6e381a0a89d8057bd226f56469fff6500b63f` |
| DIY Electronics with AI | `catalog/vendors/m5stack/products/gps-unit-v1-1.yaml` | `2caf11a8ef91dcf161b2878350838867ee70872479882767349c21329b7da642` |
| MotoBox | `docs/06-backend/BACKEND_INTEGRATION.md` | `2ee34e5977f29e2add38f76df33124e12f87cb3081dd0937f5a72e86e6653764` |
| MotoBox | `mobile-integration/wechat-miniprogram-app/README.md` | `ad1cf0e9412360603fe268306522dc9b6e0d1f55562fc7720df83f78772f9ca3` |
| MotoBox | `mobile-integration/wechat-miniprogram-app/pages/login/index.js` | `63bc072ca946c641af50931a3468caad53d75a140f2de56c139b209a77abc38e` |
| MotoBox | `mobile-integration/wechat-miniprogram-app/pages/device-detail/index.wxml` | `25ee113931ca00f0d9f06800add16099bfbcc91d29f61f36b4056faba4ab6393` |
| MotoBox | `mobile-integration/wechat-miniprogram-app/pages/device-share/index.js` | `0ebb42773221dfd03ae60670baf2a2a964376ee4d1896d8633ee26a09faf78ac` |
| MotoBox | `mobile-integration/wechat-miniprogram-app/pages/device-share/index.wxml` | `f9e6a65f77a5b69bcaccff31ccc9fbbf9d1ce8e8fc06f0c1a91db075076ee222` |
| MotoBox | `mobile-integration/wechat-miniprogram-app/utils/config.js` | `5deff44ffc4685bd30de0355707a535409b6147b053fd8ed06c361fd57b6eed0` |
| MotoBox | `mobile-integration/wechat-miniprogram-app/utils/coord.js` | `2d58eb27609301868bd65bfb4a6e283a0e0f9444d469c57cfdce1d3f58201e2d` |
| MotoBox | `mobile-integration/wechat-miniprogram-app/utils/api.js` | `cebb1e2dca77cc6a614b4ae2c144c8b7ae2153b88a68d9317d3020289f1c9d3a` |

## 完整提示词

下列提示词中的“当前”均指以上源快照；工具调用时按前述角色提供实际图片。
风格参考截图未在仓库重新分发，因此未来复测需提供自己的布局参考或使用本次生成页，并不能保证像素级复现。

### 第 1 张

```text
Use case: infographic-diagram.
Create page 1 of a 3-page detailed Chinese user manual for our real MotoBox GPS project, not a generic invented product.
Format: landscape 16:9, high resolution, aim for 3072x1728 or the highest tool-supported readable output. White paper, editorial technical manual design, dark charcoal typography, M5 blue section numbers and connector lines, restrained orange safety block. Clear and generous typography. This is technical educational illustration, NOT an advertisement, no huge title that wastes space.
INPUT ROLES:
Image 1 = verified StickS3 K150 hardware identity. Preserve the slim BLACK rectangular body, portrait screen window, single BLUE horizontal pill-shaped front KEY1/A button below screen, actual side buttons, bottom-edge USB-C, white Grove connector shown on lower BACK. No orange M5StickC case and no extra buttons. The front and back can be separate accurate views; do not move the rear Grove connector onto the front.
Image 2 = verified Unit GPS v1.1 identity, WHITE slim rectangular module, turquoise Grove connector, two mounting holes, GPS V1.1 face. Preserve this module; GPS is an external separate accessory.
Image 3 = LAYOUT ONLY (T-LITE sheet). Use its high-information annotated-manual concept only. Do not copy its product, text, logo, thermal imagery, menu, QR code, Wi-Fi setup method or LAN/CLOUD modes.
Hardware invariants: annotations precisely locate front screen, blue front A button, USB-C lower edge, rear Grove port. If the speaker or B/power button is not visible in a view, use a separately titled text card; never point an arrow to a guessed location.
Page layout: header 10%; left 48% main accurate product-front illustration and separate small rear/Grove inset plus white GPS Unit; right 52% clear cable mapping table and control/power cards; full width bottom slim distinction strip. Fine dividers, color-coded line dots in table black/red/yellow/white with gray outline for white. Do not draw an uncertain physical pin-order diagram; the TABLE describes signal mapping, not connector left-to-right orientation.
Exact text, preserve all digits and directions:
HEADER:
"MotoBox GPS"
"01 / 03  认识硬件与接线"
"StickS3 K150 + Unit GPS v1.1"
LEFT labels:
"彩色屏幕"
"1.14 英寸 · 本项目横屏 240 × 135"
"A / KEY1 · GPIO11"
"轻按切换状态页与绑定页"
"USB-C"
"5 V 供电 / 烧录"
"背面 Grove 接口"
"HY2.0-4P · 连接 GPS"
"Unit GPS v1.1"
"外接定位模块 · 115200 / 8N1"
Main screen ONLY show "MotoBox GPS" in mint on dark screen; no fictitious status, values or menus. Label under illustration "结构示意 · 非实机照片".
RIGHT top heading: "Grove 接线对应"
Table columns: "线色" | "StickS3" | "方向" | "GPS Unit"
Rows EXACT:
"黑" | "GND" | "共地" | "GND"
"红" | "5 V 输出" | "→" | "5 V"
"黄" | "GPIO9 / TX" | "→" | "RX"
"白" | "GPIO10 / RX" | "←" | "TX"
Under table: "按防呆与丝印接线；本表不是插座左右针序。"
RIGHT middle three concise control cards:
"B / KEY2 · GPIO12" / "当前固件未定义操作"
"扬声器" / "播报服务器下发的绑定验证码"
"电源 / 复位键" / "单击开机或复位 · 双击关机" / "连接 USB 后长按进入下载模式" / "以上为厂商按键说明"
RIGHT lower orange power caution:
"供电注意"
"首测使用 USB-C 5 V"
"固件已开启 Grove 5 V 输出，禁止红线反向供电"
"GPIO 为 3.3 V；电池标称 250 mAh，续航未测"
BOTTOM full-width neutral capability strip:
"板载但本固件未用：IMU / 麦克风 / 红外 / HAT2 / 磁吸"
FOOTER:
"prototype · 固件 a743599 · 资料 2026-09-24"
"AI 图解；操作与验证边界见配套手册"
Do not add real passwords, codes, MAC addresses, IPs, QR codes, performance claims or unknown pinouts. Avoid replicating tiny backside schematic text: use understated non-text markings in rear inset, showing physical geometry only. Every callout endpoint must be accurate. Never present battery runtime or outdoor positioning as validated. Render exactly one complete sheet with readable text and no clipping.
```

### 第 2 张

```text
Use case: infographic-diagram. Create page 2 of our 3-page Chinese MotoBox GPS / StickS3 manual.
Landscape 16:9, high-resolution crisp technical typography. Image 1 is the approved PAGE STYLE reference: same white background, dark headline, M5 blue headers/cards, restrained orange caution, compact typography and footer. Image 2 is the real StickS3 hardware identity only; never invent product controls.
Goal: explain the EXACT CURRENT FIRMWARE screen states and A button behavior. This is an instructional screen reconstruction, not an actual device screenshot. No real binding code, SSID, IP address, MAC address or account data.
Page layout: header 10%; upper two-thirds split 54% left into a large dark landscape status-screen reconstruction + line-by-line meanings below/next to it, and 46% right into a white binding-screen reconstruction + three-state explanation. Bottom 20% a three-column crucial distinction strip. Strong visual hierarchy, adequate white space, no overly small text.
Header exact:
"MotoBox GPS"
"02 / 03  按键与屏幕状态"
"StickS3 K150 · 当前固件界面说明"
LEFT blue title: "状态页：先看模块，再看定位"
Draw standalone enlarged LANDSCAPE rectangle ratio 240:135 with dark navy screen and mint title, NOT a touch phone frame. All screen text exactly:
"MotoBox GPS"
"GPS MODULE ONLINE"
"FIX WAITING"
"Wi-Fi UP  DEMO"
"IP x.x.x.x  -- dBm"
"MQTT UP  BIND BOUND"
"A: CODE"
All fit legibly; A: CODE as small but readable footer right. These are explicitly anonymized teaching placeholders.
Under screen a clear label:
"界面示意 · DEMO 与 IP 均为占位"
Below/alongside the screen a 2-column explanation table. Exact rows:
"GPS MODULE ONLINE" | "最近 5 秒内收到有效模块数据"
"GPS MODULE NO DATA" | "检查 GPS 供电、接线与串口"
"FIX WAITING" | "模块在线，正在等待有效定位"
"FIX READY / SAT / km/h" | "有效定位 / 卫星数 / 速度"
"Wi-Fi / IP / dBm" | "网络连接 / 本机地址 / 信号强度"
"MQTT UP / DOWN" | "遥测服务已连接 / 未连接"
RIGHT blue title: "绑定页：只有未绑定才需要新码"
Draw a standalone landscape WHITE screen rectangle with exactly:
"MOTOBOX BINDING CODE"
"------"
"WAITING FOR MQTT"
"DEVICE DEMO"
"A: BACK"
Large six hyphens are placeholders, never substitute six digits or a working-looking code.
Caption:
"占位界面示意；实际会显示服务器六位码或 BOUND"
Below, three colored compact text cards exact:
"CHECK" / "服务器状态尚未确认；先等待"
"UNBOUND" / "确认未绑定后请求新码，并播报六位数字"
"BOUND" / "已经绑定；重连或重启无需重新绑定"
Separate small horizontal control card with precise icon of the single BLUE front pill A button (same as hardware reference):
"A / KEY1" / "轻按：状态页 ↔ 绑定页"
"B / KEY2" / "本固件未定义操作"
Annotation just below: "A 只切页，不是刷新验证码"
BOTTOM full width three reminder blocks, exact text:
"模块在线 ≠ 定位成功" / "有数据后，还要看 FIX"
"Wi-Fi UP ≠ MQTT UP" / "联网不代表服务已连接"
"CHECK ≠ UNBOUND" / "不把查询中当成未绑定"
Footer:
"prototype · 固件 a743599 · 资料 2026-09-24"
"源码界面示意；新 LCD 排版仍待实机目视验收"
Do not add any settings menus, AP mode, Bluetooth provisioning, QR code, long-press behavior, battery gauge, current coordinates, countdown, microphone usage or navigation controls. Current app is not a touch UI. Preserve exact case in the English states. Correct Chinese and clean line wraps. Render one complete standalone page.
```

#### 第 2 张的定向修正

```text
Edit this exact manual page, preserving every section, table entry, main title, hardware button icon, font style, blue/orange colors and the two screen drawings.
Only make these three clarity corrections:
1. Replace the caption below the dark LEFT screen with the exact Chinese text: "示例 A：联网且已绑定；尚未定位"
2. Replace the caption below the white RIGHT screen with the exact Chinese text: "示例 B：未联网等待绑定；六横线为占位"
3. In the bottom GRAY strip remove the circle "5" and the unrelated repeated list about IMU / 麦克风 / 红外 / HAT2 / 磁吸. Replace that LEFT part with two clean lines:
"两个屏幕展示不同状态，并非同一时刻"
"界面、网络名与 IP 均为示意，非实机截图"
Keep the existing RIGHT footer "prototype · 固件 a743599 · 资料 2026-09-24" and "源码界面示意；新 LCD 排版仍待实机目视验收".
Do not change, add or remove any other text or functional claims. Do not introduce a real binding code. Preserve all English display strings exactly. Render one standalone corrected page at the same aspect ratio and readable quality.
```

### 第 3 张

```text
Use case: infographic-diagram. Create the MISSING third and final operational page of a detailed Chinese MotoBox GPS manual.
Landscape 16:9, aim high-resolution 3072x1728 or highest readable supported output.
Input 1 is approved page STYLE reference only: white paper, M5 blue headings, charcoal text, orange caution. Keep a compact header and clear dense-but-readable manual layout.
Input 2 is StickS3 hardware identity: black slender device with blue single pill front A button. Input 3 is white Unit GPS v1.1 hardware identity. Do not import arbitrary reference text or functions.
This page must explain the ACTUAL current demo onboarding, not invent AP/QR/Bluetooth provisioning or imply service credentials are free automatically.
Exact header:
"MotoBox GPS"
"03 / 03  联网绑定与排查"
"StickS3 K150 + Unit GPS v1.1"
TOP section heading: "设备怎样连到微信"
Draw a clear left-to-right icon pathway, each node label and connecting label must fit:
"GPS Unit" -- "UART" --> "StickS3" -- "2.4 GHz Wi-Fi" --> "路由器 / 手机热点" -- "互联网" --> "MotoBox" --> "微信小程序"
Small note below: "车辆移动时需保持热点联网；当前组合没有蜂窝模块。"
MAIN section heading: "首次使用：按顺序完成"
Six generously sized numbered cards in 3 columns x 2 rows connected in reading order. Text EXACT:
01 title "配置并烧录"
body "预先填写 Wi-Fi 与设备 MQTT 凭据"
small "凭据由平台单独开通；本机没有配网菜单"
02 title "接线并供电"
body "GPS 接 Grove，USB-C 接 5 V"
small "按防呆方向接线，检查模块供电"
03 title "等待网络上线"
body "先 Wi-Fi UP，再 MQTT UP"
small "联网后自动向服务器查询绑定状态"
04 title "看绑定状态"
body "CHECK：等待查询；BOUND：直接使用"
small "只有 UNBOUND 才请求并播报新码"
05 title "未绑定才添加设备"
body "微信登录 → 添加设备 → 输入六位码"
small "有效 10 分钟，仅使用一次；等待 BOUND"
06 title "户外检查定位"
body "FIX READY 后查看实时位置与历史轨迹"
small "当前户外定位、移动轨迹与分享仍待验收"
Do not draw fake app screens. Use simple icons and clearly caption "流程示意".
BOTTOM left 66% section heading "遇到问题先看状态"
Five-row compact table exact:
"NO DATA" | "查 GPS 供电、接线与 115200 / 8N1"
"FIX WAITING" | "移到户外开阔处，等待有效定位"
"Wi-Fi DOWN" | "检查热点与烧录前填写的配置"
"MQTT DOWN" | "检查互联网、设备凭据和服务"
"BIND CHECK" | "等待查询；持续不变时核对后端协议"
BOTTOM right 34% orange caution card:
"断网不等于永久保存"
"最多 120 帧 RAM 缓存"
"默认 5 秒一帧，约 10 分钟容量"
"恢复联网按序补传；掉电或重启丢缓存"
"队列满会丢旧帧"
Footer exactly:
"prototype · 固件 a743599 · 资料 2026-09-24"
"小程序访问需有对应版本资格；图解不替代实机验收"
Constraints: one complete standalone sheet. Every block must fit and be readable. No real SSID, IP, password, binding code, QR code or location. No 4G radio, SD card, Bluetooth provisioning, AP setup, touchscreen controls, registration website mockup, or claim outdoor test passed. Do not copy the previous page's footer capability list or stray section number 5.
```

### 第 4 张（技术原理篇）

```text
Use case: infographic-diagram.
Create one additional TECHNICAL PRINCIPLES sheet for a real end-to-end MotoBox GPS demo. This complements the three operational pages, so header says 原理篇, NOT 04/03 or 04/04.
Landscape 16:9, high resolution, white page, M5 blue technical manual style matching input 1, black text, a few mint normal-state accents, orange limits. Clear Chinese. Input 1 is visual style only. Input 2 real black StickS3 with blue front pill A key. Input 3 real white Unit GPS v1.1 with turquoise Grove connector. Preserve real hardware identity in small illustrations.
Exact header:
"MotoBox GPS"
"原理篇  从卫星定位到微信分享"
"硬件 + 定位固件 + 后台平台 + 微信端"
TOP 38%: four connected numbered modules with clear simple icons and blue arrows:
1 "GNSS 定位"
"卫星 → GPS Unit 解算经纬度"
"UART 输出 NMEA 数据"
2 "StickS3 处理"
"校验数据，筛选有效定位"
"经纬度 / 速度 / 时间 / 序号"
3 "联网与上报"
"2.4 GHz Wi-Fi / 手机热点"
"MQTT over TLS · 默认每 5 秒上报"
4 "MotoBox 与微信"
"后台保存最新位置与历史点"
"小程序通过 HTTPS 获取并显示"
Diagram: satellite signals go ONLY to GPS Unit; GPS Unit connected by UART to StickS3; StickS3 connects over Wi-Fi to internet/cloud; cloud connects to phone. Never draw GPS being obtained from Wi-Fi or from the phone. Use tiny icon-backed text blocks, all text large enough to read.
Caption under this row: "车辆位置来自外接 GPS；Wi-Fi 负责传数据。地图显示按需将 WGS-84 转为 GCJ-02。"
MIDDLE left 49% title:
"平台注册与设备接入"
Four concise rows with icons:
"① 微信登录：首次创建或关联平台账号"
"② 开通设备凭据：配置 Wi-Fi 与 MQTT"
"③ 六位验证码：把设备绑定到自己账号"
"④ 查看位置：先有有效定位，再看地图与轨迹"
A thin boundary note:
"用户账号 ≠ 设备接入凭据；当前 MQTT 凭据需单独开通。"
MIDDLE right 49% title:
"微信分享：设备主人授权，好友只读"
Numbered flow with a simple owner phone, share card and viewer phone; NO mock real app screenshot, no map of an actual place, no QR code:
"① 设备详情 → 分享位置 → 生成链接"
"② 点击分享，发送给好友或微信群"
"③ 对方登录并接受，以只读身份加入"
Two concise notes:
"邀请链接默认 1 天有效，可供多人接受"
"主人可移除授权；接收人可看位置与轨迹"
BOTTOM full-width section title:
"一个可落地的车辆定位 Demo"
Three rectangular components with plus signs:
"StickS3 + GPS Unit + USB 供电"
+
"可联网热点 + 已开通的平台服务"
+
"微信账号 + 可访问的小程序版本"
Below these a two-color validation strip with exact text:
"已有证据：室内联网、状态上报、断网补传、验证码语音与绑定"
"待验收：户外有效定位、移动轨迹、微信位置分享完整链路"
Small final note: "准实时展示，不保证固定端到端延迟；当前设备无蜂窝模块。"
Footer:
"prototype · 固件 a743599 · 平台资料 9b706e3 · 2026-09-24"
"系统流程示意 · 非实机地图或生产注册页面"
Keep the audience able to understand what each component does. A generic backend registration web form MUST NOT be invented: actual end-user registration here is via WeChat login; network device credentials are separate. Do not imply unrestricted public access, free automatic device credential creation, all backend code open-sourced, completed field testing, SD persistence, 4G hardware, indoor GNSS guarantee, or autonomous stolen-vehicle tracking. Only depict described facts. One polished complete sheet.
```
