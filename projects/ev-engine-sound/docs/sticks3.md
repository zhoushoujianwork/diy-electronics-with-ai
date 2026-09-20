# M5Stack StickS3 (K150)

独立板型 `m5_sticks3`；适用于 K150，不适用于 StickC / StickC Plus / Plus2。
ESP32-S3-PICO-1-N8R8，8 MB Flash / 8 MB PSRAM；本固件只用内部 RAM。
已在 StickS3 实机验证芯片/Flash 身份、PM1/ES8311、LVGL、A 键给油释放与 B 键
切换。屏幕和声音工作正常；未进行校色、声压或音色还原精度测量。

## 引脚与依据

| 功能 | GPIO / 地址 |
| --- | --- |
| 内部 I²C SDA / SCL | 47 / 48 |
| ES8311 / M5PM1 | 0x18 / 0x6e |
| I²S MCLK / BCLK / WS | 18 / 17 / 15 |
| I²S DOUT / DIN | 14 / 16；DIN 不使用 |
| LCD MOSI / SCLK | 39 / 40 |
| LCD DC / CS / RESET / BL | 45 / 41 / 21 / 38 |
| A / B | 11 / 12，低电平有效 |
| LCD 电源 / 扬声器使能 | M5PM1 GPIO2 / GPIO3；不是 ESP GPIO2/3 |

来源：

- [官方产品文档](https://docs.m5stack.com/en/core/StickS3)
- [M5Unified 引脚和音频、电源初始化](https://github.com/m5stack/M5Unified/blob/e79eb6e3137a41e50b0ec23c8c38738bdb3a11e2/src/M5Unified.cpp)
- [M5GFX 屏幕初始化](https://github.com/m5stack/M5GFX/blob/641944bd5b123e42b6f4379ff16768e292e203d1/src/M5GFX.cpp)

PM1 0x16 功能、0x10 方向、0x13 推挽、0x11 电平，只掩码修改 GPIO2/3 并回读；
不改充电状态、IR、Grove 输出。按 M5GFX 设置 0x09=0，禁用 I²C 空闲休眠。
ES8311 使用项目已有 16 kHz、MCLK=256Fs、16-bit stereo slots 初始化，
不是 M5Unified 默认的 BCLK 派生 22050 Hz 配置；扬声器为单声道。

ST7789P3 原生 135×240，RAM 原生窗口 (52,40)。本版横屏 240×135，SPI3 40 MHz
mode 0，反色，MV|MX 和窗口 (40,53)。整体显示已验证，首末行的像素级对齐仍可
进一步测量。GPIO38 使用 5 kHz PWM，默认半亮。

## 双按键交互

没有触摸屏，所以本板型不提供滑动操作。

| 操作 | 结果 |
| --- | --- |
| 主屏按住 A | 启动并给油；释放 A 收油回怠速 |
| 主屏短按 B | 循环下一种发动机，共 18 种、最多 12 缸 |
| 长按 B（700 ms） | 主屏 → 音量 → 红线转速 → 主屏 |
| 设置页 A / B 短按 | 减 / 加；音量步长 5%，转速步长 500 RPM |
| 同时按 A+B | 停机；必须两个键都松开才恢复响应 |

按键有 30 ms 防抖；长按不产生短按动作；开机时按住按键不会意外给油。
侧面电源键保留 PMIC 原有行为，不用作油门。
设置暂存在 RAM，重启恢复默认，车型切换会恢复该模型默认红线。

默认音量 60%。官方建议电池供电时低于 75% 以降低大音量复位风险；软件仍可调
到 100%，设置页有提示。板载 1 W 小扬声器适合验证，不等同于车用外放功放。

## 构建

在 `firmware/` 下：

```sh
. "$IDF_PATH/export.sh"
idf.py -B build-portable-sticks3 \
  -DSDKCONFIG=build-portable-sticks3/sdkconfig \
  '-DSDKCONFIG_DEFAULTS=sdkconfig.defaults;config/boards/m5_sticks3/sdkconfig.defaults' \
  -DEV_BOARD=m5_sticks3 -DIDF_TARGET=esp32s3 build
```

不要给此板加载立创的 16 MB 配置。烧录前确认 USB 身份、芯片与 8 MB Flash，
完整备份原固件，再用此构建目录的 `flash_args` 写入。无需整片擦除。

## 验证边界

- 2026-09-17：普通 CTest 4/4、UBSan CTest 4/4 通过；StickS3、立创和历史板
  ESP-IDF 构建均通过。Stick 应用 728816 B，1 MiB app 分区余量约 30%。
- ASan 在本机 AppleClang 17 的运行时初始化阶段死锁，未进入测试，不计通过。
  已采样并停止进程；证据 `build-stick-sanitized/asan-sample.txt`。这与先前记录的
  `AsanInitInternal → InitializeShadowMemory → StaticSpinMutex` 问题一致。
- 共用 `engine_canvas` 渲染器：320×116 与 Stick 的 240×80，18 种模型、12 个相位
  的边界测试；像素缓冲静态分配，音频内环无改动。
- `stick_controls` 主机测试：抖动、给油释放、短按、长按菜单循环、设置 +/-、
  A+B 停声锁存、开机按住和毫秒时钟回绕。
- LVGL task 10240 B：机械绘制、控件布局/flush、按钮动作、状态查询的最大调用路径；
  38400 B 画布不在栈上。音频 6144 B、心跳 4096 B、console 10240 B 保持原预算。
- 真机需要持续串口日志、`AUDIO_READBACK` 的 PM1/ES8311 值、`UI_READBACK` 和
  所有任务最低剩余栈 ≥1024 B；构建成功不代表以上测量通过。
- 可用 `tools/verify_device.py PORT --log build/sticks3-serial.log` 做 67 秒
  启停/收油/V12 16000 RPM 检查；随后实体按钮验收，检查 `BUTTON`、`UI_ACTION`
  和 `STATE_TRANSITION`。按键给油/松手、组合停机、菜单切换必须逐项测试。
- 检查 panic/Guru Meditation/stack overflow/reset reason/启动失败及心跳连续性。
  本版本没有 SD/BLE/4G，因此没有 SD 启动日志，也没有这些负载下的栈数据。
  将来接入这些模块后必须重新压力测试，不能沿用现有栈预算结论。

若 USB 消失/反复重现，先检查首条 fatal 和复位原因，不直接归因电池或扬声器。

## 实机验证

- ROM 识别：ESP32-S3-PICO-1 (LGA56) rev 0.2，GD 8 MB quad Flash、AP 8 MB PSRAM。
- 烧录前已在本地完整备份 8 MB 原 Flash；`backups/` 不进入 Git。
- 写入 bootloader、分区表和应用，各段均通过 esptool Hash 校验，未整片擦除。
- 烧录后从 ROM USB-Serial/JTAG 切换到应用 TinyUSB CDC。ROM 日志为
  `USB_UART_CHIP_RESET / SPI_FAST_FLASH_BOOT`，应用 `reset_reason=11`，
  对应本次烧录后的 USB 复位，不是观察到的异常重启。
- 初次串口证据：`build/sticks3-rom-after-flash-20260917.log` 和
  `build/sticks3-boot-20260917.log`。ROM 到 TinyUSB 切换时 ROM 端口断开为预期事件。
- PM1 `out=0x04` 静音 / `0x0c` 发声、`dir=0x0c`；ES8311 `format=0x0c`、
  `volume=0xbf`、`clocks=0x3f`；LVGL `ready=1`、`animation=slider_crank`。
- 实体操作已记录：A 按下 `action=5 value=100`、释放 `action=6` 后
  `throttle=0`，B 短按 `action=0` 从 single 切换 twin180；动作均 `result=OK`。
  菜单音量/红线操作与 A+B 组合停机尚待实体按键验收。
- `tools/verify_device.py` 67 秒测试通过，证据 `build/sticks3-load-20260917.log`：
  66 个连续心跳，`write_errors=0`、`fault=0`，无栈溢出、panic、异常复位或
  任务启动失败；独立 `check_log.py --min-heartbeats 60` 通过。
- 测试前段包含 A/B 实体动作（切至 inline5）；随后
  V12 持续高负载、flat6、最终 inline4 均有串口控制/状态记录。
- V12 16000 RPM 目标下实测 15999 RPM。两个 V12 UI 回读间约 13 秒增加
  372 次画布绘制，约 28.6 FPS。音频最大合成耗时 3204 µs / 16000 µs 块预算；
  UI 最大更新 6094 µs、LVGL 最大渲染 24232 µs（含首屏，不等同于音频耗时）。
- 最低剩余栈：audio 3808 B、console 8064 B、heartbeat 1600 B、LVGL 5964 B，
  均高于 1024 B 验收阈值。本版本无 SD/BLE/4G，此结果不覆盖未来这些并发负载。
- 结束回读：inline4，`running=0 rpm=0 throttle=0 volume=60`，PM1 功放关闭。
  屏幕和随转速变化的声音均正常。
