# Vehicle dual power R1 / 车载双电源板

**电气源数据已建立，EDA原理图尚未布线，不能用于制板或装车。**

当前 R2 源数据包含 BAT 约 3.9V/2A 与人工选择的标称 VIN 5V/2A 两路 4G 输出。[VIN 设计依据](../../docs/vin-schematic-r2.md)、[首批回读](r2-vin-checkpoint.json)和[外围回读](r2-vin-peripherals-checkpoint.json)记录新增 78 件器件的放置、保存、重载和脚位核查。导线、NC、正式排版和 DRC 尚未完成。

本板独立于主控，支持车载 12V、单节锂聚合物备用电池、按键开关机、硬件总开关和停车运动唤醒。
主控输出约 5V / 1A；4G 接口由用户在断电时选择 BAT 约 3.9V / 2A 或 VIN 标称 5V / 2A。电源板不识别所接模块，两路不能同时接到同一模块。

## 当前成果与状态

- 嘉立创 EDA 专业版独立工程：`MotoBox Vehicle Dual Power R1`。
- 四个原理图页面已建立：299 个元件从真实库放置；VIN 新增 78 件经过保存、重载与实际脚位回读。测量位置并非最终排版。
- 当前板上 BOM 为 84 种料号，包含 C 号、封装、数量和位号；参数化源数据覆盖全部引脚网络及明确 NC。新选无源件的原厂 PDF 尚有缺项，以库参数为初筛依据。
- 12 项电气意图测试通过。自动布局仍有功能区求解失败，完整布线、NC 写入、规则检查和完成态回读尚未完成。
- 本阶段不布 PCB、不实现固件、不声称已通过车载或电池实测。

| 文件 | 内容 |
|---|---|
| [电气设计与参数计算](engineering.md) | 架构、电压压降、电流温升、充电温度、静置预算和资料链接 |
| [当前板上 BOM](bom.csv) | 299 个元件、84 种料号，VIN 外围 C 号已回读；尚非可投产 BOM |
| [外部电池与线束](accessories.md) | NTC、电池、配套插头和线束要求；尚未选定的外部商品明确标注 |
| [接口与控制时序](interfaces-and-control.md) | 端子针脚、STM32分配、关机/唤醒状态与UART协议草案 |
| [验证与首板验收](validation.md) | 已验证范围、布局阻塞、实板测试清单 |
| [完整引脚源](source/design.json) | 电气连接、NC理由、功能归属；由`tools/build_design.py`生成 |
| [器件锁定表](source/selected-parts.json) | 实际所用器件的C号、库UUID、符号引脚与封装 |

### 重现离线检查

在本目录运行：

```sh
python3 tools/build_design.py
python3 tools/calculate.py
python3 tools/calculate_vin.py
python3 -m unittest discover -s tests -v
```

这些命令生成电气源和计算，并核查设计意图；不会把导线写入EDA。
保留的局部布局输入可用`easyeda sch layout-plan --zones --from <input.json> --out <new-geometry.json> --report <new-report.json>`重现。
只有完整布局及实际朝向测量通过后，才能继续合页、Compose、受保护Apply、检查与保存重开验证。

## 接口约定

- 车载电源：螺丝端子 `BAT+ / GND / ACC`，点烟器模式通过跳线将 ACC 检测接到输入电源。
- 外置电池：XT30 功率接口，独立 NTC 接口。采用有保护板、约 3000mAh、连续放电不低于 9A 的 1S 电池。
- 主控和 Tiny 输出：分别提供螺丝端子、XH 系列插座和 2.54mm 排针；同一路接口并联，共享该路电流额度。
- XH 正式间距为 **2.50mm**；不能与 2.54mm 排针封装混用。
- 外部唤醒：`3V0_AON / GND / WAKE_N`，WAKE_N 仅接受低压开漏触发。
- 主控管理 UART 与 Tiny UART 均规划掉电隔离；不得在电源板之外用串口信号反向给已关机模块供电。

## 关键行为

- 按键开机；运行时长按约 2 秒请求保存和结束通信，确认后断电，最多等待 10 秒。
- 手动关机屏蔽 IMU / 外部唤醒，再按键或 ACC 经关闭后重新开启才启动。
- ACC 关闭且持续静止 120 秒后进入停车待机，保留按键、IMU、外部唤醒和 ACC 启动。
- 双刀拨动开关只控制硬件使能；OFF 同时隔离车载和电池两路，覆盖软件请求并禁止充电与唤醒。
- 总开关 ON 后，ACC 有效则启动，否则进入停车待机。所有供电均不存在时当然不能待机或唤醒。
- 主控直接接入另一路 USB 电源会绕过本板的关机控制；调试须隔离 USB VBUS。

本板采用独立控制器，不沿用依赖 StickS3 内部电池、IMU 和 GPIO 的早期电源方案。接口与固件协议以本项目的[接口与控制合同](interfaces-and-control.md)为准。
