# 蓝牙音频外放：A2DP Source 选型与常见外形

核验日期：2026-09-23。验证等级：公开厂商资料核验，`cataloged`。本文没有完成模块接线、配对、
传输延迟或音质实测。下列厂商照片均为官方站点外链，版权归原厂；仓库不保存其图片副本。

## 先确认角色

```text
音频生成器 ──PCM/模拟音频──> A2DP Source（发射） ))) Bluetooth Classic ((( A2DP Sink（接收）──> 喇叭
```

车载音乐系统、普通蓝牙音箱和耳机通常处于 **A2DP Sink** 角色。要向它们送声音，设备必须
提供 **A2DP Source**。产品写有“Bluetooth”“双模”或“A2DP”仍不足以判断方向：A2DP
可能只实现 Sink，也可能需要特定固件才有 Source。BLE 连接能力同样不等于 Bluetooth
Classic A2DP 音频能力；LE Audio 是另一套协议，不能用“支持 BLE”代替兼容性确认。

- [Espressif ESP32-S3](../../catalog/vendors/espressif/products/esp32-s3-series.yaml) 官方只列
  Bluetooth 5 LE，因此不能直接向普通 A2DP 音响发射音频。
- [原版 ESP32](../../catalog/vendors/espressif/products/esp32-series.yaml) 有 Bluetooth Classic；
  ESP-IDF 提供 [A2DP Source API][esp-a2dp] 和示例。能否稳定工作仍取决于实际固件与硬件测试。

## 三种确实能发射的形态，以及一个易误买的接收模块

| 形态与具体例子 | 官方外观 | 官方确认的音频路径 | 集成判断 |
| --- | --- | --- | --- |
| [ESP32-DevKitC-32E](../../catalog/vendors/espressif/products/esp32-devkitc-32e.yaml)：带 USB 和双排引脚的开发板 | <img src="https://www.espressif.com/sites/all/themes/espressif/images/esp32-devkitc/esp32-devkitc-32e.png" width="150" alt="Espressif ESP32-DevKitC-32E 开发板官方照片"><br>[官方原图][esp32-photo] | 原版 ESP32 运行自编 A2DP Source 固件；音频由应用提供给 ESP-IDF | 开放可编程，适合验证；需自行完成 PCM 传输、采样率转换、配对和缓冲。不是插上即用的音频模块。 |
| [Microchip BM83SM1-00TA](../../catalog/vendors/microchip/products/bm83sm1-00ta.yaml)：带屏蔽罩的小型贴片射频模块 | <img src="https://media.microchip.com/assets/6/8/6/0/11b1147286573fb066fefdfb9ecd.l.png" width="150" alt="Microchip BM83 贴片音频模块官方照片"><br>[官方原图][bm83-photo] | 厂商明确说明 BM83SM1-00Tx 使用 Audio Transceiver 固件可做 Source，音频输入可用 AUX 或 I²S | 是真正的发射模块候选；必须核对实际出厂/烧录固件、供电、I²S 时钟、控制接口和评估板接线。 |
| [Avantree Audikast 3][audikast]：带外壳、音频插座和配对按键的成品发射盒 | <img src="https://avantree.com/cdn/shop/files/Audikast-m1_.1.png?v=1733280807" width="150" alt="Avantree Audikast 3 成品蓝牙音频发射盒官方照片"><br>[官方原图][audikast-photo] | 3.5 mm AUX、RCA 或光纤输入，向耳机发射；官方列 SBC 等编码 | 适合先验证接收端和延迟；需要已验证的**线电平**输出，不能把功放后的扬声器端直接接 AUX。它不是可焊接模块。 |
| [Feasycom FSC-BT1026C](../../catalog/vendors/feasycom/products/fsc-bt1026c.yaml)：外形很像 BM83 的小贴片模块 | <img src="https://www.feasycom.com/wp-content/uploads/2025/03/BT1026_1.webp" width="150" alt="Feasycom FSC-BT1026C 蓝牙音频接收模块官方照片"><br>[官方原图][bt1026-photo] | 官方产品页称 *Audio Receiver*；厂商工程师确认标准固件只有 A2DP Sink | **不适合本用途**。通用 AT 手册出现 A2DP Source 选项，也不表示该型号支持。 |

以上是结构与协议方向比较，不是采购推荐或整机兼容性认证。外观相似的模组可能具有完全
相反的音频角色；同一系列的后缀和固件也可能改变功能。不要凭芯片大类、商品标题、
“Bluetooth 5.x”或板上天线认定它是发射器。

## 接入前要核对的项目

1. **角色与固件：**精确型号、后缀及固件明确支持 A2DP Source，且有到目标 Sink 的配对方式。
2. **音频输入：**I²S 是输入还是输出，主从时钟、采样率、位宽和声道；AUX 必须匹配线电平，
   不得直接接扬声器功放输出。ESP-IDF 原版 ESP32 的 A2DP 示例通常使用 44.1 kHz、
   双声道 16 bit PCM 编码 SBC；上游 PCM 若不同，需转换并测试缓冲连续性。[来源][esp-example]
3. **电气：**模块工作电压、峰值电流、逻辑电平、共地、天线净空和线束方向应按精确修订核对。
   未找到整板供电余量前，不从开发板扩展口直接给另一块无线发射板供电。
4. **实际接收端：**车机是否提供 A2DP 音乐接收、是否允许当前手机和新音源同时配对、切换
   后是否正常播放；耳机型号与编解码器要实测。
5. **时延与恢复：**测量油门动作至耳机/车机出声的时延、静音后的首声、断连重连以及长时间
   播放。发射端或接收端宣称的“低延迟”不等于整条链路的实测值。

## 来源

- [ESP32-S3 产品页][esp32s3]：Espressif，Bluetooth 5 LE。
- [ESP32-DevKitC 产品页][devkit] 与 [ESP-IDF A2DP API][esp-a2dp]：Espressif，原版 ESP32
  开发板与 A2DP Source 接口。
- [BM83 产品页][bm83]：Microchip，BM83SM1-00Tx 在 Audio Transceiver 固件下的
  A2DP Source、AUX/I²S 输入和型号区别。
- [Audikast 3 产品页][audikast]：Avantree，成品发射器的输入和编码说明。
- [FSC-BT1026C 产品页][bt1026]、[厂商工程师答复][bt1026-forum]：Feasycom，标准固件
  仅接收。

[esp32s3]: https://www.espressif.com/en/products/socs/esp32-s3
[devkit]: https://www.espressif.com/en/products/devkits/esp32-devkitc
[esp-a2dp]: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/bluetooth/esp_a2dp.html
[esp-example]: https://github.com/espressif/esp-idf/tree/v5.5.2/examples/bluetooth/bluedroid/classic_bt/a2dp_source
[bm83]: https://www.microchip.com/en-us/product/BM83
[audikast]: https://avantree.com/products/audikast-3-bluetooth-transmitter-for-tv
[bt1026]: https://www.feasycom.com/product/fsc-bt1026c/
[bt1026-forum]: https://forum.feasycom.cn/d/81-a2dp-source
[esp32-photo]: https://www.espressif.com/sites/all/themes/espressif/images/esp32-devkitc/esp32-devkitc-32e.png
[bm83-photo]: https://media.microchip.com/assets/6/8/6/0/11b1147286573fb066fefdfb9ecd.l.png
[audikast-photo]: https://avantree.com/cdn/shop/files/Audikast-m1_.1.png?v=1733280807
[bt1026-photo]: https://www.feasycom.com/wp-content/uploads/2025/03/BT1026_1.webp
