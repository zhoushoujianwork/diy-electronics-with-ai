# LCKFB / 立创开发板 supplier knowledge

本页记录查询立创开发板资料时可复用的厂商级入口、产品族命名和去标识化工程经验。具体型号、规格、
来源与验证等级以 [`products/`](products/) 下的记录为准；原理图中的精确引脚和供电方向只有经过
精确修订核对或实物验证后，才进入仓库根目录的 `boards/`。

## 官方入口与语言

| 用途 | 入口 | 说明 |
| --- | --- | --- |
| 产品与购买 | [lckfb.com](https://lckfb.com/) | 立创开发板官网 |
| 技术文档 | [wiki.lckfb.com](https://wiki.lckfb.com/) | 开发板文档、教程和开源硬件入口 |
| 庐山派 K230 产品族 | [简体中文](https://wiki.lckfb.com/zh-hans/lushan-pi-k230/) | 当前有效的完整产品族文档 |
| 庐山派选型表 | [K230 与 Lite K230D](https://wiki.lckfb.com/zh-hans/lushan-pi-k230/k230vsk230d.html) | 两款主板的官方差异表 |
| 原理图与 PCB | [开源硬件](https://wiki.lckfb.com/zh-hans/lushan-pi-k230/open-source-hardware/profile.html) | 从侧栏进入原理图、PCB、固件和扩展板资料 |
| CanMV Web IDE | [在线工具](https://wiki.lckfb.com/zh-hans/web-tool/canmv-web-ide.html) | 立创提供的庐山派在线开发入口 |

文档站部分页面声明了 `/en/` alternate URL，但截至 2026-09-21，庐山派 K230 的英文首页和
选型页返回 404；不要仅替换语言段后记录为有效英文来源。以 `sitemap.xml` 和实际 HTTP 响应核对。

## 庐山派 K230 产品族不要混用

| 产品 | SoC | 内存 | 关键差异 | 产品记录 |
| --- | --- | --- | --- | --- |
| 立创·庐山派 K230-CanMV | Canaan K230 | 1 GB 外置 LPDDR4 | 有 USB-A Host，3.5 mm 耳机输出 | [`lushan-pi-k230-canmv.yaml`](products/lushan-pi-k230-canmv.yaml) |
| 立创·庐山派 Lite-K230D-CanMV | Canaan K230D | 128 MB SiP LPDDR4 | 无 USB-A Host，带 HT6872 功放和 PWM 风扇接口 | [`lushan-pi-lite-k230d-canmv.yaml`](products/lushan-pi-lite-k230d-canmv.yaml) |

两者采用相同的双核 RISC-V 架构和官方标称 6 TOPS INT8 KPU，但内存、连接器、音频输出和可用
IO 不同。`K230` 与 `K230D` 不是可互换的板名；模型可运行不代表内存预算、FPIOA 或外设代码
也能原样复用。

## 去标识化工程经验

- 摄像头、显示/OSD、触摸和 KPU 共用媒体资源；KPU 留在主循环，网络线程负责收发和排队，
  不跨线程调用 KPU，也不在 VO 已绑定的通道重复抓帧。
- 使用板型能力表隔离 UART、RGB LED、蜂鸣器和背光实现。标准版配置不能复制给 Lite K230D，
  也不能复制给其他 Canaan K230 开发板。
- 大块 HTTP、SD/FAT、Wi-Fi 扫描与模型推理都可能同步阻塞；可采用小块发送、单并发网络请求、
  限频日志和事件驱动 UART，保持 UI 与关键状态可观察。
- CanMV 的 FPIOA 调试必须保存并恢复原复用；板载外设已占用的 PAD 不能只按 SoC 功能表重新分配。
- “API 可调用”“外设有数据”“完整功能通过”“长稳通过”应分别记录，不能从板名识别或编译成功
  推断外设支持。

## 建档边界

- 产品目录可以记录官方公开的处理器、内存、无线、连接器、供电入口和尺寸；不复制第三方 PDF。
- 40Pin/排针顺序、连接器顶视/底视、GPIO、PWM、UART、背光和电源反灌方向进入 `boards/` 前，
  必须针对精确 PCB 修订核对原理图或完成实物测量。
- 官方选型页列出 Type-C 5 V 与独立 8–24 V DC 两种供电入口。产品 YAML 只把 Type-C 5 V 写入
  单值 `power` 字段，避免用连续 5–24 V 范围错误暗示 USB 入口可接高压。
