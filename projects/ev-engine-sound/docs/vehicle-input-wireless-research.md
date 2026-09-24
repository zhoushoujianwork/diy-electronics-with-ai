# 2026 款 X9、特斯拉与无线车辆数据接入调研

调研日期：2026-09-25。关联：[共用声浪核心板方案](shared-core-hardware-plan.md)。
验证等级：公开资料核验与方案比较；没有购买、安装、连接车辆或新增硬件验证。

## 结论与选型方向

1. 首测车型明确为 **2026 款小鹏 X9**，动力版本和车机版本仍待确认；目前没有获得
   可核验的该车踏板 PID/CAN 映射、接口接线依据或现成适配器兼容性证据。
2. **特斯拉纳入第二条适配路线**。Scan My Tesla、CANserver、S3XY Commander 等
   生态有现成 CAN 接入方案，资料比当前掌握的 X9 资料充分；本项目尚未验证任何
   一款特斯拉，不能把第三方工具的车型支持写成本产品已支持。
3. “无线 OBD”通常是**车端有线接诊断口/专用转接线，适配器再通过蓝牙或 Wi‑Fi
   发数据**。车辆数据与车机音频是两条链路，无线传数据也不等于无线供电。
4. 共用核心保留有线车型接口、Wi‑Fi、BLE 输入；经典蓝牙适配器单列为需桥接的
   路线。蓝牙音频发射仍由独立 A2DP Source 承担，云端接口列为后续扩展。

## 可选路线比较

| 路线 | 公开依据 | 本项目实现与取舍 |
| --- | --- | --- |
| 自研有线车型接口板 | 可控制过滤、时间戳、电源与休眠；车型定义仍需取得 | 保留为打样基线。按实际总线选 CAN/CAN FD 控制器和收发器，X9 先查接口与踏板信号 |
| CANserver Wi‑Fi 网关 | 作者公开 Panda UDP / WebSocket 协议，Scan My Tesla 支持 [S1] [S4] [S5] | S3 可做 Wi‑Fi 客户端，优先验证；仍需车型线束、解码和丢包验收，不代表当前有货 |
| S3XY Commander | 厂商列仪表与 Scan My Tesla / tes·LAX / CANDash；Scan My Tesla 描述 Wi‑Fi UDP 路径 [S2] [S6] | 现成候选，需确认当前代次的接入模式、认证、帧格式与信号；不直接套用 CANserver 全部扩展协议 |
| BLE 适配器，如 OBDLink CX | 厂商确认 BLE 5.1、ISO 15765、ISO 11898 raw CAN [S9] | S3 有 BLE，但仍需 GATT、命令、过滤和吞吐量验证；尚未确认 CX 支持特斯拉/X9 |
| 经典蓝牙，如 OBDLink LX / MX+ | 厂商列 Bluetooth 3.0，Scan My Tesla 列出支持 [S1] [S7] [S8] | S3 无 BR/EDR，需要经典蓝牙客户端桥接；不能把适配器当 BLE 设备直接连接 |
| 手机中转 | 手机连接车辆适配器，再通过自建应用转发 | 需要应用、后台运行、导出接口与时延处理；演示备选，不能假定现成 App 向自研板开放数据 |
| Tesla Fleet Telemetry | 官方有 PedalPosition、VehicleSpeed、Gear，向公网服务器推送 [S11] [S12] | 有授权、服务器、区域和费用成本；适合后续记录/分析，不作为实时声浪首选 |
| Tesla 原生 BLE 命令 SDK | 官方支持认证后的 BLE/网络命令 [S14] | 本次未验证高频踏板订阅接口，不把车钥匙/命令能力当作实时油门数据源 |

这些均为候选。能显示电量不等于能读取踏板，能读到踏板也不等于刷新率、稳定性和
音频延迟已适合联动。“Bluetooth 5.x”“支持 OBD”“支持 Tesla”不能替代具体协议核验。

## 特斯拉的车型与转接线

Scan My Tesla 明确要求“转接线 + 兼容适配器”，分别提供以下线束入口 [S1]：

| 页面分类 | 选型边界 |
| --- | --- |
| Model 3/Y 2019–2023 | 仍需按生产时间与实际插头确认 |
| Model 3 Highland，2023-10 起 | 与旧款分开选线束 |
| Model Y Juniper，2025-02 起 | 与旧 Model Y 分开选线束 |
| Model S 2012–2015-09 | 单列早期版本 |
| Model S/X 2015-09–2021-09 | 单列中期版本 |
| Model S/X 2021 起（Plaid / Long Range） | 与旧版分开，交界日期以实际接口确认 |

这是第三方页面分类，不是本项目支持表或官方针脚表。作者提醒生产日期接近变更时
应对照实际插头，不能只按年份下单。

Enhance 的 Commander 商品选择器还区分 Highland、Juniper、标准版织物座椅、六座
YL、产地与日期，例如上海 Juniper 显示 2026-04-24/25 分界 [S6]。这是**该厂商线束
选项**，不能外推为全部适配器的统一硬件变更日期。选型需记录车型、产地、生产月份
与安装位置；Cybertruck 等未核对线束的车型不纳入首版承诺。

Scan My Tesla FAQ 说明其所需数据来自内部 CAN，总线专用线束不能由普通诊断口
替代，并提醒车辆软件更新可能改变解码 [S3]。其中旧版供电描述不能用于设计新款
车辆电源；实际低压、瞬态、针脚和适配器耐压须单独确认。Enhance 也声明其与 Tesla
无隶属关系，第三方配件的兼容说明不等于 Tesla 官方合作认证 [S6]。

## 实时信号与开放资料

- **CANserver UDP：**作者公开 Panda 会话、心跳和 CAN 帧格式，v2 支持按总线与报文
  过滤 [S4]。自研客户端有文档可循，车型信号定义仍须另外取得。
- **CANserver WebSocket：**固件 2.1 起可订阅 Analysis Items 和 Lua Variables，值
  变化时发送，最高 25 Hz，要求响应 PING [S5]。25 Hz 是上限，不能当成踏板保证
  每秒更新 25 次；连接心跳正常不代表车辆数据新鲜。
- **社区解码：**`joshwardell/model3dbc` 提供 Model 3/Y DBC，仓库标注 MIT [S10]。
  Scan My Tesla 信号表列有踏板条目，但默认展示含旧 Model S/X 范围 [S15]。必须锁定
  解码版本，验证对应总线、车型、固件、量程、有效位与频率，本文不固化未验证 CAN ID。
- **廉价 ELM327：**Scan My Tesla 说明满总线流量下可能漏掉较低频报文，过滤策略
  影响响应 [S2]。即使只订阅少量信号，仍要测丢包、时延，不能凭“能连接”入选。
- **自建参考：**`Adminius/ESP32-ScanMyTesla` 提供 CAN → 经典蓝牙参考，明确仅适用
  原版 ESP32、不支持 S3 等系列，采用 GPL-3.0 [S16]。本次未复制代码；实际复用前需
  核对许可与发布方式，不能直接并入 MIT 项目后省略上游义务。

CANserver 的旧商城入口本次返回 404，不作库存/现售承诺；作者仓库和上述协议 Wiki
可读。其固件与其他第三方组件许可须分别核验，不能因 Model 3 DBC 是 MIT 就推断
相关项目全部采用相同许可。各协议文档只证明有实现依据，不证明当前硬件已兼容。

## 云端接口的适用范围

Tesla 官方确实有 **PedalPosition（加速踏板位置）**，以及 VehicleSpeed、Gear [S12]。
Fleet Telemetry 要求公网服务器，采集器按 **500 毫秒批次**汇总，字段还受
`interval_seconds` 与数值变化条件约束；断网会缓存并在恢复后补传 [S11]。因此它
不提供本地低延迟保证，补传历史数据更不能直接驱动当前声浪，适合后续统计或回放。

官方列有中国区端点与单独应用流程 [S13]，但这不等于本项目已核实中国区目标车辆、
授权、字段和费用。原生 BLE 命令接口、云端遥测与无线 CAN 适配器分别评估；本次
没有注册应用、申请虚拟钥匙或访问车辆账户。

## 对核心 PCB 与固件的影响

建议连接方式：车型接口/专用线束 → 车端网关 → 有线、Wi‑Fi、BLE 或经典蓝牙桥接
→ ESP32-S3 声浪核心 → 独立 A2DP Source → 车机。玩具版把车辆输入替换为手把，
共用合成核心、AUX/蓝牙输出和外接电池接口。

使用 LX/MX+ 时，可以增加经典蓝牙客户端桥接，或研究原版 ESP32 同时承担该客户端
和 A2DP Source；后者是待测降成本方案，不作为初版成立的前提。数据连接的重试、
过滤和队列处理不能阻塞音频。Wi‑Fi/BLE 与 A2DP 同处 2.4 GHz，独立芯片仍需测
天线共存、供电峰值、丢包、音频下溢、心跳和各任务最低剩余栈。

统一输入层保留以下信息，而不是让每种外设各写一套声浪逻辑：

| 信息 | 约束 |
| --- | --- |
| 来源与版本 | 车型配置、线束/接口、适配器型号/固件、解码版本分别标识 |
| 踏板与可选数据 | 踏板归一化 0–1，携带有效性；车速/挡位缺失不能假装为零 |
| 时间 | 接收时间与源信号更新时间分开；有源时间戳时保留时钟依据 |
| 会话 | 重连清空旧值；有序号时检查乱序/重放，队列溢出可观测 |
| 失效处理 | 数据过期、掉线或解码不匹配时退出联动、回零并渐变静音 |

变化时推送、适配器缓存、云端补传等，都不能用“刚收到一个包”证明踏板有效。
无法确认源信号新鲜度时按失效处理；超时依据实测刷新机制制定，不靠连接心跳续命。
产品只消费驾驶数据生成声音；读取请求与纯监听分别记录，不实现车辆配置、刷写、
驾驶控制或安全功能自动化。外部网关具备控制能力时，只使用所需数据订阅，并核对
是否能关闭无关自动化。

## 验证与推进顺序

| 阶段 | 工作与通过条件 |
| --- | --- |
| A：音频基线 | 手把/台架数据 → 核心 → AUX/A2DP；验证 X9 音源选择、延迟、导航/来电/手机切换和重连 |
| B：X9 2026 | 确认动力/车机版本、接口与信号；取得可追溯依据并实测前，不标“支持” |
| C：特斯拉候选 | 选定精确车型与线束，比较 CANserver/Commander 或兼容 OBD 适配器；静止条件验证踏板、零点与失效值 |
| D：并发负载 | 记录有效更新率、数据年龄、P50/P95/最大延迟、丢包、音频下溢、心跳、栈余量和持续时间 |
| E：停车与版本 | 测锁车、休眠、唤醒、掉电与实际静态电流；软件版本更新后复验，不沿用旧兼容结论 |
| F：PCB 冻结 | 按已验证路线选择精确器件、电源预算、连接器与开源材料，保留有线数据/AUX 排障路径 |

首轮可用“有效踏板更新至少 20 Hz、输入链路新增延迟 P95 ≤ 50 ms”作为**评估目标**，
由体验测试调整，不是供应商承诺或实测结果。音频延迟另测，不能从数据更新率推算；
蓝牙若造成明显迟滞，保留 AUX 并如实说明体验边界。

实际实验按仓库规则建独立 Demo。兼容矩阵记录年款、动力、软件、总线/安装位置、
线束和适配器版本，只提交脱敏结论，不保存 VIN、钥匙、账户或完整车辆日志。当前
新路线均待硬件验证，不提升项目状态。

## 来源

以下均于 2026-09-25 读取。来源陈述不代替本项目实测。

- [S1：Scan My Tesla 适配器/车型线束](https://www.scanmytesla.com/adapters) — 软件作者。
- [S2：适配器速度与过滤](https://www.scanmytesla.com/adapter-speed-and-filters) — 软件作者，含 Wi‑Fi UDP、ELM/STN 比较。
- [S3：Scan My Tesla FAQ](https://www.scanmytesla.com/faq) — 软件作者；旧供电描述不外推。
- [S4：CANserver Panda UDP](https://github.com/joshwardell/CANserver/wiki/PandaProtocol) — 项目作者文档。
- [S5：CANserver WebSocket](https://github.com/joshwardell/CANserver/wiki/CANServer-v2-WebSocket) — 项目作者，固件 2.1 起、最高 25 Hz。
- [S6：Commander 产品](https://www.enhauto.com/products/commander) / [功能及支持 App](https://www.enhauto.com/pages/commander) — 第三方配件厂商。
- [S7：OBDLink LX](https://www.obdlink.com/products/obdlink-lx/) — 适配器厂商。
- [S8：OBDLink MX+](https://www.obdlink.com/products/obdlink-mxp/) — 适配器厂商。
- [S9：OBDLink CX](https://www.obdlink.com/products/obdlink-cx/) — 适配器厂商，BLE/raw CAN 能力不是车型兼容证明。
- [S10：Model 3/Y DBC](https://github.com/joshwardell/model3dbc) — 社区定义，MIT，未在本项目导入或验证。
- [S11：Fleet Telemetry 工作机制](https://developer.tesla.com/docs/fleet-api/fleet-telemetry) — Tesla 官方。
- [S12：Fleet Telemetry 字段](https://developer.tesla.com/docs/fleet-api/fleet-telemetry/available-data) — Tesla 官方。
- [S13：Fleet API 区域/中国区](https://developer.tesla.com/docs/fleet-api/getting-started/regions-countries) — Tesla 官方。
- [S14：Vehicle Command SDK](https://github.com/teslamotors/vehicle-command) — Tesla 官方。
- [S15：Scan My Tesla 信号表](https://docs.google.com/spreadsheets/d/1UBHw2eY3QyJL3vUz0CnTZ7iLlLB-ao5s61hexT0GuHM/edit?usp=sharing) — 软件作者；旧车型条目不外推。
- [S16：ESP32-ScanMyTesla](https://github.com/Adminius/ESP32-ScanMyTesla) — 社区参考，GPL-3.0。

X9 依据与未核验项沿用[共用核心方案中的官方入口](shared-core-hardware-plan.md#小鹏-x9-与-obd-直插的边界)。
本次补充检索仍未取得 X9 协议依据；搜索摘要不作为兼容性证明。

[S1]: https://www.scanmytesla.com/adapters
[S2]: https://www.scanmytesla.com/adapter-speed-and-filters
[S3]: https://www.scanmytesla.com/faq
[S4]: https://github.com/joshwardell/CANserver/wiki/PandaProtocol
[S5]: https://github.com/joshwardell/CANserver/wiki/CANServer-v2-WebSocket
[S6]: https://www.enhauto.com/products/commander
[S7]: https://www.obdlink.com/products/obdlink-lx/
[S8]: https://www.obdlink.com/products/obdlink-mxp/
[S9]: https://www.obdlink.com/products/obdlink-cx/
[S10]: https://github.com/joshwardell/model3dbc
[S11]: https://developer.tesla.com/docs/fleet-api/fleet-telemetry
[S12]: https://developer.tesla.com/docs/fleet-api/fleet-telemetry/available-data
[S13]: https://developer.tesla.com/docs/fleet-api/getting-started/regions-countries
[S14]: https://github.com/teslamotors/vehicle-command
[S15]: https://docs.google.com/spreadsheets/d/1UBHw2eY3QyJL3vUz0CnTZ7iLlLB-ao5s61hexT0GuHM/edit?usp=sharing
[S16]: https://github.com/Adminius/ESP32-ScanMyTesla
