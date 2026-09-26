# M5Stack 蜂窝通信、GNSS 与 Air780EG 选型

核验日期：2026-09-27。依据为公开官方资料，验证等级为 `cataloged`。
这是规格与选型边界整理，不是相同供电、天线和网络下的实机性能比较，也没有核实当前售价。

## 1. 蜂窝联网与 GNSS 是两个功能

蜂窝模块负责数据传输；GNSS 接收器负责卫星定位。有的产品集成二者，有的只有联网。
LBS 基站位置不等于卫星定位；仅支持 MQTT、AT 指令或标注“4G”均不能证明具备 GNSS。

| 产品 | 模块/芯片 | 网络 | GNSS 与使用边界 |
| --- | --- | --- | --- |
| M5 Atom DTU Cat1，K120/A120 [2] | SIM-A7680C | 国内 LTE Cat.1 | 产品规格列 LBS；未将 GNSS 列为该成品已具备功能。附通用 A76XX GNSS 手册不能证明具体 SKU 装有 GNSS。A120 不含 ATOM Lite |
| M5 Unit Cat1-CN，U204 [3] | ML307R-DL | 国内 LTE Cat.1 | 官方规格未列 GNSS；应另配定位接收器或核验精确硬件能力 |
| M5 Unit CatM GNSS，U137 [4] | SIM7080G | LTE Cat-M / NB-IoT | 明确集成 GNSS，支持 GPS/GLONASS/北斗/Galileo；需匹配运营商网络、SIM 业务和频段 |
| 合宙 Air780EG [7][8] | Air780EG | 面向国内的 LTE Cat.1 | 官方明确通信定位二合一、GPS/北斗；支持 AT 与 LuatOS |

SIM7080G 可实现定位加无线传输，但与 Cat.1 模块不是网络层面的直接替代。
LTE-M/NB-IoT 的网络部署与卡业务必须核验，不能笼统承诺“普通 4G 卡插上就能用”，
也不能笼统宣称整个国家没有某种网络。移动性、注册/重连、GNSS 与上传调度应按具体固件和部署测试。
本轮未验证 SIM7080G 的 GNSS/蜂窝并发工作方式，不据此承诺连续定位和持续数据传输可以任意并行。

Air780EG 官方资料支持作为 AT 外设，也可运行 LuatOS 应用独立完成定位与联网。
其模组支持的协议与电气范围不等于第三方核心板的接口规格；不同核心板仍需逐版本查电源和电平。

## 2. GPS Unit v1.1 与 GPS SMA

| 项目 | Unit GPS v1.1 / U032-V11 [5] | Unit GPS SMA / U190 [6] |
| --- | --- | --- |
| 芯片/模组 | AT6668 / ATGM336H-6N | AT6668 / ATGM336H 系列，官方链接 ATGM336H-6N 手册 |
| 卫星系统 | GPS、QZSS、BD2/BD3、Galileo、GLONASS | 相同 |
| 标称精度 | <1.5 m CEP50 | 相同 |
| 定位更新率 | 最大 10 Hz | 最大 10 Hz |
| 默认接口 | UART 115200 bit/s，8N1；NMEA0183 4.1 | 相同 |
| 天线 | 板载陶瓷天线 | SMA 外置有源天线，附 1 m 线缆 |
| 主体尺寸 | 48 × 24 × 8 mm | 71.4 × 24 × 8 mm，天线另计 |
| 官方功耗数据 | 5 V / 31.64 mA | 待机 5 V / 31.64 mA；工作 5 V / 40.90 mA |

两者主要区别在天线与安装自由度。外置天线可以放在天空可见度更好的位置；有源放大也不能恢复
被建筑或金属完全遮挡的卫星信号。CEP50 是统计指标，不是每次定位都在 1.5 m 内的保证。
最高 10 Hz 不代表出厂配置或网络上报速率；上述电流不是已测启动/峰值电流。

M5 SMA 页面对比表列出同条件北斗卫星数量 18 对 8；未给出完整可复现实验条件。
只记录为厂商样例，不将其外推成固定增益、固定精度提升或室内定位承诺。
页面“多频”措辞不能自动推导成支持 L1+L5 双频或 RTK，应以列出的频点与精确模组手册为准。

## 3. ATOM 主控、底座与供电

- ATOM Lite / C008 [1]：ESP32-PICO-D4，4 MB Flash、520 KB SRAM，官方机身 24×24×9.5 mm；
  有 USB Type-C、Grove 与底部扩展 GPIO。官方 5 V @ 500 mA 是主控输入规格，不能当作外设口可供电流保证。
- ATOMIC ECHO BASE 原路径现显示 Atomic Voice Base / A149 [9]：24×24×14.1 mm。
  它是语音底座；外形参考不等于电气兼容、任意多底座串叠或供电能力相同。
- U204 [3]：成品输入为 5 V，UART 默认 115200；官方列出的一个发射工况为 5 V / 831.12 mA。
  原表中 FDD/TDD 与频段文字存在疑点，因此不采用该关联做频段功耗模型，也不把这个电流当峰值。
- U137 [4]：Grove 5 V 或 DC 插座 9–24 V 供电，3.3 V TTL UART，双 SMA 天线口。
  其待机 12 V / 13 mA 不能与另一产品不同工况的 5 V 电流直接比较效率。

设计单 Type-C 蜂窝设备时，先核对电源额定能力、Type-C 可用电流、线材压降、稳压瞬态、
局部储能与回流路径。选择 5 V / 3 A 适配器只能作为预算提议，不能代替整机峰值测量。
若主控 USB 与底座 USB 可以同时插入，要明确防倒灌和电源优先关系。

## 4. 成本与结论边界

未读取到可核实的淘宝现价，不记录具体“贵多少”、套件优惠或长期流量赠送承诺。
成本比较应包括模块、GNSS/天线、连接器、PCB/组装、电源、外壳、测试和迭代成本。
模块规格相近不证明 GNSS 精度、功耗或移动轨迹实际表现相同。

## 官方来源

以下均于 2026-09-27 查阅；只链接，不复制第三方手册或商品图片。

1. [ATOM Lite](https://docs.m5stack.com/zh_CN/core/ATOM%20Lite)。
2. [Atom DTU Cat1](https://docs.m5stack.com/zh_CN/atom/atom_dtu_cat1)。
3. [Unit Cat1-CN](https://docs.m5stack.com/en/unit/Unit_Cat1-CN)。
4. [Unit CatM GNSS](https://docs.m5stack.com/zh_CN/unit/catm_gnss)。
5. [Unit GPS v1.1](https://docs.m5stack.com/zh_CN/unit/Unit-GPS%20v1.1)。
6. [Unit GPS SMA](https://docs.m5stack.com/zh_CN/unit/Unit-GPS%20SMA)。
7. [合宙 Air780EG 说明](https://docs.openluat.com/air780eg/)。
8. [合宙 Air780EG 产品资料](https://docs.openluat.com/air780eg/product/)；该页提示旧 PDF 与当前产品定义不一致时应查当前硬件指南。
9. [Atomic Voice Base，原 ECHO BASE 路径](https://docs.m5stack.com/zh_CN/atom/Atomic%20Echo%20Base)。
