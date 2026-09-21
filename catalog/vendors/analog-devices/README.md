# Analog Devices / Maxim supplier knowledge

Analog Devices 官网保存 MAX 系列等原 Maxim 产品的现行产品页和数据手册。查询时用精确芯片
型号检索，并分别核对数据手册修订、封装后缀、产品生命周期与评估板型号。

- 官网：[Analog Devices](https://www.analog.com/)
- 产品入口：[Products](https://www.analog.com/en/products.html)
- 当前记录：[MAX9814 麦克风放大器 IC](products/max9814.yaml)

MAX9814 是放大器芯片，MAX9814EVKIT 是官方评估板，第三方的“MAX9814 麦克风模块”又是另一
层产品。不要把官方芯片或评估板参数直接当成某块商家小板的整板规格；必须另行确认麦克风
型号、原理图、供电、增益/AGC 配置、输出耦合、电平与连接器。

当前 schema 没有独立的音频 IC 类别，MAX9814 暂归 `audio-module`，记录名称和摘要明确
标注 IC 身份。比较与可迁移的选型建议见
[PDM 与 MAX9814 音频前端比较](../../../docs/research/pdm-vs-max9814-audio-frontends.md)。
