# Projects

| 项目 | 状态 | 简介 |
| --- | --- | --- |
| [EV Engine Sound](ev-engine-sound/) | hardware-verified | ESP32 实时程序化发动机声浪与机械动画 |
| [StickS3 GPS → MotoBox](m5stack-sticks3-gps-motobox/) | prototype | StickS3 + ML307R-DL Tiny 的 4G 遥测上报室内运行 31 分钟、372 帧确认并入库（室内无定位）；先前 Wi-Fi 固件已验证绑定和室外静止定位；4G 断连补传、断电冷启动、移动轨迹与位置页面待验收 |
| [ATOM Lite 4G/GNSS 堆叠底座](atom-lite-cellular-gnss-base/) | idea | 通信核心板 + 主机适配 PCB + 打印外壳，ATOM 24×24 mm 底座先行，评估 StickS3 顶部变体；从需求到试产的教学项目，尚无原生 PCB 或实机验证 |
| [车载双电源模块](vehicle-dual-power-module/) | prototype | 独立 12V / 1S 电池供电设计；主控 5V/1A、BAT 约 3.9V/2A 与标称 VIN 5V/2A 均有未布线电气源数据；四页共放置 299 件元件，无 PCB 或实板验证 |

完整项目应能从自己的目录独立构建，不依赖仓库根目录的隐式环境。
