# 度云供应商知识

厂商：**成都度云未来科技有限公司**，品牌 **度云物联 / doiot**。
厂商身份和官网域名见 [doRTU-6E1 官方硬件手册](http://data.doiot.cn/docs/NT26-core/NT26-6E.html)。
以下入口于 **2026-10-11** 核对；这里整理公开资料，不代表硬件实测或销售库存。

## 官方资料入口

| 入口 | 用途 |
| --- | --- |
| [度云官网](https://www.doiot.cn/) | 厂商及产品入口 |
| [硬件资料中心](http://data.doiot.cn/index.html) | 核心板硬件手册、AT 资料及教程导航 |
| [NT26 doRTU-6E1 硬件手册](http://data.doiot.cn/docs/NT26-core/NT26-6E.html) | 6E1 TTL、RS485 两种核心板的 V1.0 资料 |
| [doLua 入门](https://dolua.cn/docs/get-started) | 工具安装、工程结构、下载及串口日志 |
| [doLua API](https://dolua.cn/docs/dolua-api/nt26/api) | 查询设备端运行时、网络和外设 API |
| [doLua 硬件脚位](https://dolua.cn/docs/dolua-api/nt26/hardware) | 按机型核对硬件资料，不能替代具体核心板的原理图 |
| [doLua 文档仓库](https://github.com/dolua-big-egg/dolua-api-docs) | 入门页 GitHub 导航指向的官方文档仓库 |

硬件资料中心使用官方 HTTP 地址；本次同路径 HTTPS 握手失败。
官网 HTTPS 证书校验也未通过。保留真实来源及访问说明，不把 HTTP 链接改写成未经验证的 HTTPS 链接。
doLua 入门和下文的 `charge` 文档通过 HTTPS 正常读取；其余 doLua 入口按入门页导航登记。

## 型号与版本核对

- `NT26` 是系列名称；本次硬件手册的板型为 **doRTU-6E1**，分为
  [6E1 TTL](products/nt26-6e1-ttl.yaml) 和 [6E1 RS485](products/nt26-6e1-rs485.yaml)。
  两种串口配置分别记录，不能共用同一接口清单。
- 手册页头标注硬件 **V1.0**；收到实物后仍需核对 PCB 修订、完整模组型号及固件版本。
  不把板型名称 `6E1` 当成内部模组的完整型号。
- 手册中的 **5–36 V** 是批量定制备注；标准 6E1 的 VCC 规格仍按 **5–18 V** 记录。
- doLua 入门当前介绍 NT26 / NT26 PRO、Lua 5.5 及编辑器扩展，但不能据此认定所有旧 RTU 固件
  都具备相同 API。升级前按具体型号、固件和资源说明核对。

## 供电、定位与外设的确认边界

- **BAT 是电池供电接口，不等于内置充电器。** 6E1 手册明确要求 BAT 与 VCC 二选一，
  未声明充电管理、电池保护、负载共享或自动电源切换。
- [`charge` 官方文档](https://dolua.cn/docs/dolua-api/nt26/api/peripherals/charge)
  （文档版本 1.0.1）说明该 API 读取外部充电 IC 的状态，不负责给电池充电；
  其具体接法以 NT26-F6E0 为例，不能反推每一种核心板的器件清单。
- 6E1 手册明确列出的融合定位方式为单基站、多基站及 Wi-Fi 定位。
  定制备注中的“GPS 上报”不足以确认标准 SKU 板载 GNSS；当前也没有足够证据断言其不支持 GNSS。
  判断 GPS 端子用途应核对完整模组型号、实际电路和固件能力。
- 通用 doLua 文档中出现 I²C、SPI 或低功耗 API，不代表 6E1 核心板已引出对应总线或深睡唤醒脚。
  本次未建立其 IMU 接入或运动唤醒能力。
- 功耗表是厂商声明，未给出完整测试条件。它不证明某种外设、电源或联网配置下的待机电流与续航。

具体规格和来源见两个产品记录；精确引脚与接线应另建经核对的 `boards/` 档案。
只链接官方手册与图片，不在仓库内复制第三方资料包。
