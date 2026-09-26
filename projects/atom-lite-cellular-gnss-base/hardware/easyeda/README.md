# EasyEDA 原生工程准备

用户指定：**只使用 Codex 内置浏览器中的 EasyEDA Pro 网页版，禁止改用桌面 EDA 客户端。**
工程写入、保存及验证走 `easyeda` typed CLI；GUI 只读观察不能替代数据回读。

[打开工程](https://pro.lceda.cn/editor#id=247f2261614c4e20861e8a96a79cb7ba)

当前仅建立原生工程框架：一个 ATOM Board、五个功能页、一块空白 PCB。
页面标记 `DRAFT - NOT WIRED` / `A0-inputs`。尚无器件实例、电气连接、PCB 板框或布线。
StickS3 的第二块板仍处于接口调研阶段，没有通过复制空板宣称完成适配。

## 文件

- [project-state.json](project-state.json)：工程、原理图、PCB 和五个页面的真实 UUID，以及保存重开后的核对结果。
- [configure-scaffold.playbook.json](configure-scaffold.playbook.json)：已有页面的名称、草稿标记和保存参数，使用现有 Apply v1 格式。
- [library-candidates.json](library-candidates.json)：三款 TI 候选的精确 MPN、C 号、库器件和符号/封装关联。
- `local/atom-adapter-a0-scaffold.epro2`：本机原生快照，目录忽略 Git；不包含制造交付含义。

参数文件不包含易失的 `windowId`。每次先运行 `easyeda health`，核对内置浏览器的工程连接，
用当次 windowId 和稳定 project/doc UUID 限定目标。存在多个连接时先辨认宿主，不能依赖默认窗口。
2026-09-27 的连接已通过本机连接来源核对为 Codex 内置浏览器，并与当前标签的工程 URL 对应。

## 功能页与下一项工作

| 页面 | 下一步输入 |
| --- | --- |
| 01 Power - DRAFT | Tiny 实际输入范围、三支路预算、Type-C 受电与主机防倒灌拓扑 |
| 02 ATOM Host - DRAFT | 底部连接器型号、针号/方向、GPIO 分配与真实电源路径 |
| 03 Tiny Interface - DRAFT | 六针板级定义、UART 电压域、EN 时序；再决定是否采用 TXU0202 |
| 04 GNSS Interface - DRAFT | 对应修订的供电/电平、连接器和线束映射 |
| 05 Test Points - DRAFT | 网络冻结后设置可接触的电源/UART/控制测试点 |

Tiny 原始数值仍见[设计输入](../design-inputs.json)，未核实项不填默认电压或 NC。
现有外壳和排针坐标沿用原提交；本轮没有新增 PCB 机械尺寸。

## 执行与验证范围

创建工程与页面使用 `project create` / `sch page-new`，没有 GUI 落图。
读取默认页面与 Board 关联后，执行参数化名称/标题栏设置，逐页保存并通过有界
`doc reload` 重开，再读取标题栏、器件和导线。五页的名称、草稿标记、版本和空电路状态均符合预期。

配置文件绑定本工程现有五页，**不是从零创建工程的通用模板**。进入正式设计后不要重放草稿标记。
当前源文件可从仓库根目录离线检查：

```sh
easyeda apply projects/atom-lite-cellular-gnss-base/hardware/easyeda/configure-scaffold.playbook.json --dry-run
```

导出原生快照使用 `project export --project-uuid <UUID> --window <当次连接> --out <新文件>`，
该命令拒绝全局 `--project/--doc`。本轮首个带全局路由的调用在预检阶段被拒绝，按帮助补充的错误提示
修正参数后导出成功。ZIP 完整性已检查，导入恢复仍未验证。

首次执行中，Board 改名成功；原理图文档改名返回 `result.ok:false`，但 CLI 总结仍把该步计为成功。
独立回读发现名称实际仍为 `Schematic1`，据此保留原名，并从最终参数文件移除无效的改名步骤。
最终文件保留 15 个已落地的页面设置/保存步骤，离线预检通过；没有以总成功数代替实际状态。

库查询确认的是器件身份和关联。`lib symbol get` / `lib footprint get` 本次只返回资产元数据，
不能用来声称真实 symbol pin 与 footprint pad 已逐脚检查；这些工作放在实例化之前完成。
库摘要的电压/方向描述若与原厂手册不同，以原厂手册为电气依据。

运行环境：CLI/daemon `v1.6.0-dirty`、Connector `1.6.0`、Web `4.1.60`，加载的 Skill 为 `1.5.3-dev.3`。
显式安装对账显示最新发布为 `1.7.0`；本轮未升级，未宣称当前组件具有同一发布版的一致性。
本轮没有 ERC/DRC 或硬件通过结论。调研依据及落图前置输入见[PCB 调研](../../docs/pcb-design-research.md)。
