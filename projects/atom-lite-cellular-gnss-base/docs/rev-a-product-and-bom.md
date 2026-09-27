# 首版产品目标与器件选型

日期：2026-09-27。当前原生修订为 Header A4；整板电气设计尚未冻结。

## 我们做什么

做一款 **ATOM Lite 的 Tiny 4G 适配底座**，与外接 GPS Unit 组成定位上报设备。
自己设计适配 PCB 和打印外壳，复用 ATOM Lite、Tiny 通信核心板和 GPS Unit。
适配板负责堆叠排针受电、主机与 Tiny 的连接，以及需要的控制和电平适配。
SIM、蜂窝基带和射频由 Tiny 承担；定位由现成 GNSS 接收器承担。

首次完成 SIM/APN、服务端凭据和设备绑定后，日常使用目标为：
**主机插入 Type-C → 自动联网 → 上报在线状态 → 获得有效 GNSS 位置后持续上报。**
上电自动运行需要固件与 Tiny 启动电路共同实现；取得有效位置还取决于天线和接收环境。

| 项目 | 首版选择 |
| --- | --- |
| 主机 | ATOM Lite C008 |
| 蜂窝 | 芯引者 ML307R-DL Tiny；板级供电/电平待取得资料 |
| 定位 | 外接 M5Stack Unit GPS v1.1 / U032-V11 |
| GNSS 入口 | ATOM 自带 Grove，G26 为主机 TX、G32 为主机 RX；接线前核对电平与线束 |
| Tiny 串口 | 底部 G19 为主机 TX、G22 为主机 RX；独立于 GNSS UART |
| 供电 | 主机 Type-C 经堆叠排针 5V/GND 向适配板供电，带载能力待验证 |
| 电源模块 | 后续单独开发；另定义电源方向、隔离及切换 |
| 自制件 | 一块 ATOM 适配 PCB、对应打印壳；Tiny 独立壳沿用已有基线 |
| 结构目标 | 24 × 24 mm 外轮廓；完整底座高度、PCB 板框和装配空间待核 |
| 第二版本 | StickS3 使用另一块适配 PCB 和外壳，独立验证 Hat2 接口和电源路径 |

G23/G33 留作备用；两路设备 TX 不并联。其他市售 GNSS 通过专用线束适配，逐款核对
电源、电平与针序。Grove、PH、SH 等插头名称不能证明互插兼容。

## 一页原理图的当前内容

已在内置浏览器重建为一张 A4 图纸，页面名 `ATOM Tiny 4G - Header A4 DRAFT`。
上半部为排针受电示意和条件式 UART 电路，下半部为普通中文说明；详细调研与测试留在文档。
原生旧五页已由用户删除，仓库 A1 参数保留为历史记录。

当前共 5 个器件实例、16 个引脚网络。J1 仅为供电逻辑示意，**不进入 PCB 或采购 BOM**；
示意 1=STACK_5V、2=GND 不能用作 ATOM 正式针号。真实主机/Tiny 配对件和完整电源尚待补齐。
U2、C3、C4、R3 是条件式 UART 子电路，不能据此判断整个适配板已经可接线或投板。

## 器件选择

完整清单见 [Rev A BOM](../hardware/rev-a-bom.csv)。条件式器件虽已放置，仍需满足采用条件。

| 功能 | 型号 / 立创编号 | 决定与理由 |
| --- | --- | --- |
| UART 电平转换 U2 | TI TXU0202DCUR / C5186957，1 颗 | 条件式；Tiny 电平不兼容时采用，还需确定低压侧供电与掉电时序 |
| UART 去耦 C3/C4 | Yageo CC0603KRX7R9BB104 / C14663，2 颗 | 100 nF / 0603，分别为两侧电源去耦，随 U2 采用 |
| UART OE 下拉 R3 | UNI-ROYAL 0603WAF1003T5E / C25803，1 颗 | 100 kΩ / 0603，OE 默认关闭，随 U2 采用 |
| ATOM 配对排针 | 待定 | 实测针位、插入长度、堆叠高度和额定载流后锁 MPN/封装 |
| Tiny 配对排母 | 1 × 6、2.54 mm 方向，具体型号待定 | 核对孔坐标、配合高度和载流；图片顺序不是 PCB 针号 |
| Tiny 供电稳压与保护 | 待定 | 取决于该 Tiny 修订的 VIN/BAT、板内稳压、启动和发射峰值 |
| GNSS 板载插座 | 复用 ATOM 自带 Grove | 适配板不另购此插座，线束与 GPS 电平仍待确认 |
| 测试点 | PCB 裸露焊盘，未布置 | 随最终网络安排；装配后可触达供电、GND、UART 和控制信号 |

USB-C C165948、CC 电阻 C23186、LM66100 C2869734 已从当前适配板移除，BOM 数量记为 0、
状态为 `excluded-superseded`。C15850 电容、C14165 保险仅保留历史选型依据，当前没有选用数量。
TUSB320、TPS2121 可供后续独立电源模块比较，不属于当前已实现电路。

## 电源设计边界

当前供电意图：**主机 Type-C → ATOM 电源路径 → STACK_5V/GND 排针 → 适配板**。
适配板没有 Type-C 接口。后续独立电源模块另行设计，当前没有双源并接电路。

- ATOM 已查官方路径包含二极管和 1 A PTC；主机、GNSS 与蜂窝启动/发射负载是否可同时工作待实测。
  USB 电源标称 3 A 不能消除主机内部路径和连接器的限制。
- Tiny VIN/BAT 范围未确认，STACK_5V 尚未接至 Tiny。是否需要降压、软启动、储能及保护取决于板级规格。
- MODEM_VIO_TBD 没有已确定的电压或来源，不能把 VIN/BAT 当作低压 UART 参考电源。
- 独立电源模块接入时需定义额定电压、峰值电流、回流路径、插拔时序及主机 USB 的隔离/切换。
  不能直接把第二个 5 V 电源并到当前 STACK_5V。

详见[排针供电修订](header-power-revision.md)。

## 最后需要补齐的输入

| 信息 | 影响的选择 | 当前处理 |
| --- | --- | --- |
| Tiny 板级 VIN/BAT 范围、峰值、板内稳压 | 电源入口、稳压器、储能与保护 | 不锁 Tiny 电源电路 |
| UART 实际电平、EN 定义、上电是否自动启动 | TXU0202、VIO 电源、EN 驱动 | 保留条件式选项；不默认 EN 等于裸模组 PWR_ON |
| 两端连接器几何、针号和载流 | 具体 MPN、封装、总高度 | 按实物核对后冻结，外壳此轮不改 |
| 主机供电路径的负载能力 | 能否仅靠主机 USB 运行蜂窝 | 测启动/注册/发射压降、温升和复位；独立电源模块作为后续设计 |
| GPS Unit 当前修订 UART 电平与线束 | Grove 能否直接连接 | 官方页面与所链旧图不一致，保留实物核验 |

保存重开后引脚网络一致；局部几何与连接检查通过。官方 DRC 仍为 5 条 warning，
只有聚合数量，逐项原因待核查。**完整原理图和可下单 BOM 尚未完成。**

## 来源与核验

查询日期均为 2026-09-27。第 3–7 项中的 Type-C、CC 下拉、LM66100、保险和电源电容为历史调研，未作为 Header A4 的适配板选料。原厂 PDF 留在本机临时目录，只提交来源、必要摘录和库身份。

1. [Tiny 公开商品入口](https://item.taobao.com/item.htm?id=1055078978369)：型号线索为芯引者 Tiny，
   页面存在 ML307R-DL / ML307C-DL-CN 变体；用户商品图指向 ML307R-DL。
   本次浏览器读取超时，尚未获得板级手册；不能确认实物修订或把另一变体参数混用。
2. [ATOM Lite 官方文档](https://docs.m5stack.com/en/core/ATOM%20Lite)、
   [GPS Unit v1.1 官方文档](https://docs.m5stack.com/en/unit/Unit-GPS%20v1.1)。
   ATOM 电源路径和 GPS 旧图冲突见 [PCB 调研](pcb-design-research.md)。
3. [HRO TYPE-C-31-M-12 图纸](https://datasheet.lcsc.com/datasheet/pdf/9e56b777c022540fcce7c7f67825f55e.pdf?productCode=C165948)，
   2020-12-08，单页：外形约 8.94 × 7.35 mm，针脚表、5 A/20 V 与推荐 PCB 焊盘图已目视核对。
   连接器额定电流不代表电源协议允许的电流；是否能放入最终底座仍待装配检查。
4. [UNI-ROYAL 厚膜电阻手册](https://datasheet.lcsc.com/datasheet/pdf/0a975aaa49b7c97f38a963127be4a823.pdf?productCode=C23186)，
   V3 p2：F 为 1%；该系列 5101 = 510 × 10¹ Ω = 5.1 kΩ。
   JLC C23186 具名 Resistance 为 `5.1kΩ`，EasyEDA 实例身份查询值一致。
5. [TI LM66100 手册](https://www.ti.com/lit/ds/symlink/lm66100.pdf)，SNOSD25A，2019-11；
   p3 引脚、p4–5 额定与电阻、p8 反向阻断、p13 供电去耦。
6. [Samsung MLCC 手册](https://datasheet.lcsc.com/datasheet/pdf/02336ea48ea44ca18c72517dd3cb7b47.pdf?productCode=C15850)，
   2015-11，Part Numbering System：21=0805，A 介质=X5R，106=10 µF，K=10%，电压 A=25 V。
   与 JLC C15850 的具名 Capacitance/Voltage/Tolerance 和 EasyEDA 身份一致；未取得该料号偏压曲线。
7. [Littelfuse 466 系列手册](https://datasheet.lcsc.com/datasheet/pdf/68c0a0f84b2b437b80451cb918a64da7.pdf?productCode=C14165)，
   2011-06-16，p1 3 A 条目：32 V、名义冷阻 0.020 Ω、I²t 0.576 A²s；p2 连续运行降额说明。
   查询采用完整后缀 NRHF，对应 C14165，不与 NR 混写。
8. JLC SMT 目录 API `selectSmtComponentList` 与 EasyEDA `lib by-lcsc` 对照了上述六个新查询料号。
   C722729 虽目录名称为 HY2.0-4P-WT，返回图纸标题却为 PH2.0-卧贴带扣，且没有足够证据证明
   与 M5 线束匹配；本轮不采用。保存的[选型库身份](../hardware/easyeda/selection-a2-library.json)包含这个排除结果。

验证仅覆盖资料、料号与库身份，不代表完整符号/封装逐脚核验、原理图接线、PCB、装配或实机通过。
