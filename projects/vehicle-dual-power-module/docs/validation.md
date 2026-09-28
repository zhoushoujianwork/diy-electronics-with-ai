# 项目验证状态

**结论：仅电气设计源数据得到离线检查；原理图未布线，不能制板或接车载电源。** 详细记录、失败布局证据和首板验收表保存在[硬件验证记录](../hardware/vehicle-dual-power-r1/validation.md)。

| 范围 | 结果 | 边界 |
| --- | --- | --- |
| 嘉立创 EDA 器件身份 | 299 件已放置；VIN 新增 78 件均保存、重载并回读实际脚位 | [首批回读](../hardware/vehicle-dual-power-r1/r2-vin-checkpoint.json)与[外围回读](../hardware/vehicle-dual-power-r1/r2-vin-peripherals-checkpoint.json)只证明器件身份与未布线状态 |
| 电气源数据 | 12 项单元测试通过，引脚网络与明确 NC 已定义 | 不是 EDA 导线、网表或规则检查 |
| 参数计算 | 已生成 BAT、VIN 压降、峰值电流、NTC、静置电流等计算 | 效率、温升、脉冲和七天待机尚未测量；VIN 全温 5V±5% 尚未保证 |
| 完成态原理图与 PCB | 未完成 | 新增 VIN 功能区仅局部布局候选成功；旧区仍有失败，尚无完整布线、NC、DRC、PCB |
| 固件与实板 | 未实现/未测试 | 主控适配、STM32 时序和车载/电池实测仍待完成 |
| VIN 标称 5V / 2A 可选输出 | 完成[引脚级源数据](vin-schematic-r2.md)、78 件器件放置回读及初步电流计算 | 器件仍在纸外测量位置、没有导线，不能作为已支持接口 |

本项目 `project.yaml` 使用 `prototype`，表示电气设计原型已有源数据；`validation.build` 与 `validation.hardware` 均保持 `pending`。下一里程碑是完成四页连线、规则检查、保存重开回读，再进入 PCB 与首板测试。任何接入项目必须另记其整机验证结果。
