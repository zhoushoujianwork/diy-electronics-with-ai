# 合宙供应商知识

官网：[合宙通信](https://www.openluat.com/)。
资料入口：[合宙模组资料中心](https://docs.openluat.com/)。
选型入口：[产品选型指南](https://docs.openluat.com/SelectionGuide/SelectionGuide/)。

## 型号与二次开发

- 区分 SoC 模组、定位器 PCBA、调试板和带电池外壳的整机。套装标题、价格和外置天线不能当作核心板规格。
- Air8201G 的 BLMQ、BQ、LM 与基础版外设不同，按[硬件资料](https://docs.openluat.com/air8201/hardware/)核对精确后缀；不能把 BLMQ 的蓝牙、音频和气压计能力套给基础版。
- Air8202G 的当前主控、SIM 装配和出厂软件按[使用手册](https://docs.openluat.com/air8202/product/)的“重要说明”核对；旧图片和历史原理图可能对应较早版本。
- [Air780EGH/EGG/EGP 总体介绍](https://docs.openluat.com/air780egh/product/)说明这些型号采用 LuatOS 二次开发；应用语言为 Lua，不按 AT 指令模组使用。Air780EGH 明确不支持 C-SDK；底层固件有 C 实现不等于开放该器件的 C 应用开发接口。
- 用 LuaTools 下载、调试 Lua 脚本及匹配型号的 LuatOS 固件。官方出厂软件只是一个应用，可参考[Air8201G 使用指南](https://docs.openluat.com/air8201/hardware/8201G_guide/)中的脚本源码和 Air8202G 手册中的定位器源码入口。
- 同为 LuatOS 不代表板级脚本直接兼容；运动传感器、充电管理、看门狗、LED、蓝牙和音频配置需按对应板资料适配。
- G-Sensor 是加速度传感器；三轴加速度、GNSS 航向与带陀螺仪的六轴姿态方案分别核对，不用“姿态”宣传推定陀螺仪支持。

## 产品记录

- [Air8201G-BLMQ](products/air8201g-blmq.yaml)：带蓝牙、气压计、音频和 BTB 扩展的定位器 PCBA。
- [Air8202G](products/air8202g.yaml)：板载 GNSS/4G 天线、USB-C 和充电管理的定位器 PCBA。

这里只保存官方可核验资料及开发入口，不记录购买、库存、设备账号或未经验证的引脚图。
