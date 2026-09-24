# M5 产品图解手册：把项目讲明白

`m5-product-manual` 是 DIY Lab / DIY Electronics with AI 的仓库独立技能。
它把 M5Stack 硬件资料、项目代码和验证记录整理为中文图解说明书，帮助使用者找到接口、理解按键、判断状态并完成操作。
已有实例是 [MotoBox GPS 的四页手册](../projects/m5stack-sticks3-gps-motobox/docs/manual/README.md)。

## 从哪里获得灵感

| 参考 | 带来的启发 | 在本技能中的做法 |
| --- | --- | --- |
| GitHub 的 [product-design-three-views](https://github.com/zhoushoujianwork/product-design-three-views)，及其[上游项目](https://github.com/Gayaya999/product-design-three-views) | 明确产品身份、区分参考图作用、保持连续图稿的一致性 | 产品图约束外形，版式图帮助组织内容，每页明确用途。 |
| M5 产品图解说明方式，尤其本次参考的 T-LITE 手册 | 在设备旁标注按钮与接口，再按步骤解释操作反馈 | 将硬件、屏幕、使用流程分成独立页面，按用户阅读顺序组织。 |
| [StickS3](https://docs.m5stack.com/en/core/StickS3) 与 [Unit GPS v1.1](https://docs.m5stack.com/en/unit/Unit-GPS%20v1.1) 官方资料 | 精确型号、硬件外观、接口和供电条件 | 核对实际 SKU / 修订版，结合目标项目实现确定说明范围。 |

在这些启发上，DIY Lab 增加了代码与验证核对：板子具备什么、固件实现什么、实物测过什么，分别记录。
技能直接保存在本仓库的 [`.agents/skills/m5-product-manual/`](../.agents/skills/m5-product-manual/SKILL.md)，运行时无需安装外部三视图技能。

所检查的参考 GitHub 仓库未附加开源许可证；此处致谢工作流思路，不据此声明其代码或素材可自由再分发。
M5 官方原图、手册和商标归原权利人，本仓库保留来源链接，示例图明确标为 AI 图解。

## 为什么先查事实再生图

说明书里的一个按钮、一条接线或一个成功状态，都应有依据。

| 事实层级 | 从哪里核对 | MotoBox 实例 |
| --- | --- | --- |
| 硬件能力 | 官方手册、板卡档案与精确型号 | GPS 是外接 Unit；StickS3 负责处理、显示与联网。 |
| 固件行为 | 按键事件、UI 字符串、联网配置与协议实现 | A 只切换状态页/绑定页；B 暂无应用动作；没有扫码配网菜单。 |
| 实测结果 | 带硬件版本、固件、供电、时长和范围的验证记录 | 已有静止定位和上报证据；车辆移动轨迹与微信分享完整链路仍待验收。 |

例如，`GPS MODULE ONLINE` 表示收到模块数据，还要看是否有有效定位；Wi-Fi 连上也不代表 MQTT 服务已连接。
这些区别先写进文案，再进入图稿，避免把另一款产品的菜单或尚未实现的功能画进说明书。

## MotoBox 实例：四页说明不同问题

[![MotoBox 图解示例：StickS3、外接 GPS、接口与供电，AI 示意图](../projects/m5stack-sticks3-gps-motobox/docs/manual/01-hardware.png)](../projects/m5stack-sticks3-gps-motobox/docs/manual/README.md)

| 页 | 帮助读者回答的问题 |
| --- | --- |
| 认识硬件与接线 | GPS 在哪里？哪个键有用？串口与供电怎样对应？ |
| 按键与屏幕状态 | 当前是模块在线、已定位、已联网，还是等待服务器确认？ |
| 联网绑定与排查 | 首次配置需要什么？什么时候输入绑定码？断网数据会不会丢？ |
| 技术原理篇 | 定位、上传、平台账号、设备接入和微信分享各自做什么？ |

完整链路是 **GPS Unit → UART → StickS3 → Wi-Fi / 热点 → MQTT TLS → MotoBox → 微信小程序**。
GPS 提供位置，热点提供互联网；用户登录、设备 MQTT 凭据、绑定所有权和分享查看权限是不同环节。
详见[技术原理与 Demo](../projects/m5stack-sticks3-gps-motobox/docs/manual/README.md#5-技术原理各部分如何组成定位-demo)。

图稿固定在 `a743599` 固件快照，配套文字注明后续版本变化；图片不是实机照片或最新 UI 截图。
项目保持 `prototype`。当前验证结论直接查阅[项目记录](../projects/m5stack-sticks3-gps-motobox/docs/validation.md)，避免把说明图当作新增实测。

## 如何使用

准备一个具体目标项目，以及它的硬件型号、README、固件和已有验证记录。可提供官方产品图作为外形依据，另选一张说明书作为版式参考。
参考资料中的功能不会自动继承到目标设备。

在包含 `.agents/skills/m5-product-manual/SKILL.md` 的仓库版本中，支持仓库技能的 Agent 可直接发现它。
需要跨项目使用时，按[安装文档](AGENT_SKILL.md)在仓库根目录运行：

```sh
./scripts/install-agent-skill.sh
```

重载 Agent 后，输入 `$m5` 选择技能，或使用完整名字。图像生成功能默认采用 **Codex 内置 GPT 生图**，需要当前 Codex 环境提供该工具；这条路径无需另配图像 API Key。

**制作本仓库的 MotoBox 手册：**

```text
使用 $m5-product-manual，阅读 projects/m5stack-sticks3-gps-motobox 的硬件、固件和验证记录，
制作中文图解说明书：接线与供电、按键与屏幕、联网绑定与排查，再补一页定位到微信分享的原理。
先核对事实与版本，标出尚未验收的步骤。
```

**换成自己的项目：**

```text
使用 $m5-product-manual，为当前 M5 项目制作图解使用说明书。
以当前仓库代码为功能依据；官方产品图用于外形，参考手册只用于版式。
讲清每个按键的动作、成功反馈、配置前提和常见问题，把硬件能力与本固件已用功能分开。
```

没有对应硬件时，可以先浏览手册与事实表，了解这种组织方法。
复现 MotoBox 云端流程还需要服务方提供设备凭据和可访问的小程序版本；安装技能不会自动开通服务。

## 生成后留下什么

通常在目标项目的 `docs/manual/` 下保存：

| 文件 | 用途 |
| --- | --- |
| `README.md` | 阅读入口与精确文字说明，图片之外也能查询步骤。 |
| `facts.md` | 每条事实的依据、版本和限制。 |
| `generation.md` | 参考图角色、提示词、源文件哈希、实际图片尺寸与校对记录。 |
| 实际图片 | 按内容拆页；文件数量与交付清单逐项确认。 |

图稿需要逐页检查中文、GPIO、信号方向、按钮动作、状态含义和版本。
本例曾修正两个不同时间的屏幕状态说明，并在漏页后增加交付清单；这些检查已沉淀到技能中。
后续固件或验证结果变化时，重新核对受影响页面与文字，保留图稿对应的版本。

欢迎用 DIY Lab 为自己的项目补一份能照着理解和操作的说明书，也把新的核对经验带回仓库。
