# PCB 与结构设计输入

当前交付物包括设计输入、接口核对表与 Tiny 完整卡扣外壳 CAD；原理图、布线 PCB 和制造发布包尚未完成。

- [design-inputs.json](design-inputs.json)：单位、来源、实测/标称值及未知尺寸；不是可执行 EDA playbook。
- [interface-review.csv](interface-review.csv)：Tiny 商品图中的丝印和待核实电气项。
- [preliminary-bom.csv](preliminary-bom.csv)：功能级 BOM 候选；未定料号明确留空，不可直接下单。
- [首版产品目标与选型](../docs/rev-a-product-and-bom.md)、[rev-a-bom.csv](rev-a-bom.csv)：首选料号、条件式器件、未决项与单页原理图目标。
- [主机适配方案](../docs/host-adapters.md)：ATOM 底座、StickS3 顶部适配、电源分配与 3D 打印外壳。
- [Tiny 完整卡扣外壳](enclosure/README.md)：底壳、可拆盖与两种排针出口，几何检查通过，待打印试装。
- [GNSS 四线接口与双 UART](../docs/gnss-uart-expansion.md)：现成接收板的电源、针序、信号方向和兼容边界。
- [PCB 设计调研](../docs/pcb-design-research.md)：板级资料与裸模组要求、电源方案、机械限制、缺口及官方来源。
- [pcb-interface-plan.csv](pcb-interface-plan.csv)：两款主机 GPIO 分配提案；官方逻辑针号不等于 PCB pad 编号，空值表示未核实。
- [pcb-part-candidates.csv](pcb-part-candidates.csv)：已查数据手册的器件候选；未冻结 BOM、库器件或采购状态。
- [EasyEDA Header A4 原理图草稿](easyeda/README.md)：一张 A4 图纸，排针受电逻辑示意与 UART 候选子电路；尚未布板。

## 坐标与封装

机械记录默认 mm；实际导入 EasyEDA PCB 时按当前 typed CLI 使用的 mil 转换，`1 inch = 25.4 mm`。
尚未确定板原点、连接器 anchor、Pin 1、孔径或圆角半径，因此不生成虚构的 PCB 封装。
24 mm 外壳不自动等于 24 mm PCB；20×20 mm Tiny 外框不代表连接器 courtyard 也只有 20×20 mm。

需要补齐的工程图必须包含：ATOM 底面视图、Tiny 元件面及焊接面、板框、基准原点、孔坐标、
插针高度、壳壁、SIM/USB/天线接头可达空间和各层最高件。
StickS3 变体另需顶部 Hat2 观察方向、配对高度、壳体卡位与屏幕/按键/USB 避让；不复用 ATOM 板框。
先制作占位和配合试片，再冻结适配 PCB 与打印模型，记录打印材料、方向、公差和温升验证。

## EasyEDA 实施路径

当前环境使用已安装的 `easyeda` typed CLI，不进行 GUI 落图或任意脚本注入。
2026-09-27 初始查询没有编辑器连接；随后已在用户指定的内置浏览器完成工程准备。
当前操作使用 CLI/daemon/Connector `1.8.0` 与 Web `4.1.60`。原生工程已重建为 Header A4 单页，
包含一个不进 BOM/PCB 的排针供电参考符号及四器件 UART 候选电路，18 条普通中文说明。
适配板无 Type-C，GNSS 使用主机现有 Grove G26/G32；5 个器件的 16 个引脚网络经保存重开核对。
完整供电和接口、PCB、5 条官方 DRC warning 的逐项原因仍待完成。工程身份与证据见上方入口。

后续沿用已创建的独立工程，只连接内置浏览器，不启动桌面 EDA 客户端：

1. `easyeda health` 读到实际编辑器/连接器，记录版本。
2. 核对已有项目、页面和 Board UUID；新变体确实需要独立工程时再用 `project create`，避免重复创建。
3. 按精确型号取得器件、符号、真实引脚表和封装；完成 pin→net 审核。
4. 参数化生成原理图，先 dry-run，再 apply；核对实际连接并保存。
5. 创建并关联 PCB，机械尺寸冻结后建立板框、连接器和固定结构，再布局/布线。
6. 分别检查电气连接、机械、规则和 DRC；执行 save → bounded reload → readback，保留未覆盖项。
7. 最后导出制造文件并用独立查看器核对。没有回读证据不宣称原生文件已完成。

## 原型到生产的两种 PCB

**载板版本（当前提议）**：复用 Tiny，设计 ATOM 接口、Tiny 连接、电源和测试点，GNSS 可先外接。
连接器高度和 Tiny 自身布局会约束壳体；不要在未核实 RF/地结构前为了压高度强行改焊模组。
同一核心板后续可配第二块 StickS3 适配 PCB；先共用电路和核心板侧接口，不预设两种主机共用同一块 PCB。

**一体版本（后续选项）**：直接使用蜂窝模组并集成 GNSS、SIM、电源和天线接口。
需要重新设计参考电路和 RF/电源布局，不能把载板的验证直接升级成该版支持。

层数、铜厚、板厚、最小间距、线宽、表面处理及 SMT 面数尚未冻结。
电源线宽按实际峰值、温升、铜厚和制造规则计算，不将通用 6/10/20 mil 经验当作载流保证。
