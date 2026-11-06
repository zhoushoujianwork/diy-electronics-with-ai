# 2026-11 合宙公开知识包待审记录

审查日期：2026-11-06。目标为 GitHub
[zhoushoujianwork/diy-electronics-with-ai](https://github.com/zhoushoujianwork/diy-electronics-with-ai)
的 `main` 分支。本记录是独立的资料索引与验收说明；当前仅准备待审内容，尚未获发布批准，未执行公开发布。

## 精确范围

本批只包含以下十个文件，不依赖工程源码、设备配置或第三方附件：

1. [流量卡 Skill](../.agents/skills/openluat-sim/SKILL.md)：接口选择、认证与传输条件、查询和写操作的授权边界。
2. [Skill 界面配置](../.agents/skills/openluat-sim/agents/openai.yaml)：名称、简介与调用提示。
3. [OpenAPI 契约摘要](../.agents/skills/openluat-sim/references/api.md)：八个查询接口、短信、充值及文档歧义；仅含合成请求示例。
4. [流量卡公开资料](../catalog/vendors/openluat/iot-sim.md)：官方公开月流量与服务年限组合，不是现行采购报价。
5. [LuatOS 应用开发与验证](research/luatos-application-bringup.md)：区分官方 API 事实和通用工程经验。
6. [Air8201G 硬件与音频排查](research/air8201g-official-hardware-audio.md)：官方图纸入口、原创信号流示意和排查方法。
7. [Air8201G-BLMQ 产品记录](../catalog/vendors/openluat/products/air8201g-blmq.yaml)：带来源引用的型号事实，验证等级保持 `cataloged`。
8. [仓库许可](../LICENSE)：本批原创文字、配置与示意适用的 MIT 许可。
9. [本待审记录](PUBLIC_KNOWLEDGE_RELEASE_2026_11.md)。
10. [精确发布清单](../publication/openluat-public-knowledge-2026-11.yaml)：目标、文件范围和资料审查结果，无批准记录。

本地链接仅在上述范围内闭合：Skill 链接 API 摘要，音频页链接产品记录，本记录链接全部文件。产品 YAML 的来源标识在本文件内定义；`vendor_id` 为目录标识，不是运行时文件导入。本批交付单条产品资料，不交付完整目录验证工具或宣称该工具可由本包独立运行。

## 匿名来源复核

2026-11-06 仅对公开资料进行匿名 GET；未读取账号凭据、登录平台、请求认证 API、查询卡数据、发送短信或充值。以下页面返回 HTTP 200，按正文复核相关事实：

- [Air8781P](https://docs.openluat.com/air8781p/product/) 与 [Air8782P](https://docs.openluat.com/air8782p/product/)：30M/月的 2.9 元/1 年、4.9 元/3 年及续费 2.9 元/1 年；100M/月的 3.9 元/1 年、6.9 元/3 年及续费 3.9 元/1 年。多年金额是总价，不补充五年价格。
- [官方 FAQ](https://docs.openluat.com/faq/2026-10-09/)：渠道与年份报价可能变化，以现行报价为准。
- [官方 HTTP 教程](https://docs.openluat.com/air724ug/luatos/app/socket/http/)：公开出现平台域名；不复制教程中的测试账号、认证示例或截图。
- [FSKV](https://docs.openluat.com/osapi/core/fskv/)：64KiB、16 个 4KiB 块；小值共用初始两块，大值每对占一块，理论最多 14 对。工程建议与该事实分别标注。
- [Air8201G 硬件页](https://docs.openluat.com/air8201/hardware/8201G/)：BLMQ 外设、配件及图纸入口；三份图纸的字节数和 SHA-256 与音频页历史指纹一致。原理图文字可核对 ES8311、AW8010B、SPK 和 DA267 标记；这不是实物连通或发声验证。
- [Air780EGH 系列介绍](https://docs.openluat.com/air780egh/product/)及[出厂软件说明](https://docs.openluat.com/air8201/hardware/8201G/8201G_software/)：LuatOS 开发边界、主控、默认外设状态与调试板供电说明。
- [供应商 API 文档](http://sim.taoyuan-tech.com/api/wechat/openapidoc)：HTTP Basic、POST JSON、十个接口路径、参数和单位；保留类型/容器、分页及重试等未定义事项，不照抄原文代码示例。

原知识页的历史核对日期仍保留。本次资料复核不验证现行成交价、真实账号、完整分页、HTTPS、短信、充值、回调或硬件功能，也不承诺所有外链以后持续有效。

## 内容、隐私与许可审查

逐个通读本批文件，检查本地链接及正文：范围内没有真实凭据、卡号、账号用量、设备日志、订单、私人决策记录或本机绝对路径。配置目录使用通用用户目录表达，示例使用占位符。API 摘要不包含私人在线测试经历。

本批是原创的事实摘要、方法、配置及信号流示意；接口名、字段名、型号与必要数值用于准确说明契约和公开产品事实。已对照上述公开来源检查摘要性质，没有整篇转载、复制官方示例程序或打包第三方图片、PDF、STEP、固件和网页。MIT 仅覆盖本批原创内容；外链目标仍归原权利人，访问、指纹或引用不提供第三方附件再分发许可。未来加入附件需另行审查。

资料验收覆盖上述独立范围、来源对应、隐私与依赖闭合；`stable` 只表示本资料单元完成整理，不代表产品或固件升级。没有新增硬件支持，没有进行构建、烧录或实机验收，不更改任何工程状态。`ready` 表示等待人类审核当前目标和精确摘要；不等于批准或发布。

## 检查记录

本次目录结构校验通过（23 家厂商、47 条产品记录），目录验证器 12 项回归及发布门槛 26 项回归通过；正文与清单另经逐文件审读和空白检查。这些工具检查不替代来源、隐私、许可或硬件验收。

精确范围与当前内容摘要由发布工具的 `review` 命令生成；批准前不导出发布快照。公开交付只应用获批快照，保留目标仓库的既有历史和无关内容。
