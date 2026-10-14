# DIY Electronics with AI

一个 AI-native、harness-friendly 的开放电子工程实验室：人负责目标、判断和真实世界验证，
AI Agent 负责检索、整理、实现、检查与协作编排。这里保存可复现的硬件 Demo、完整项目、
可复用驱动、接线依据、测试证据和去标识化工程经验，让其他电子爱好者和 Agent 都能直接复用，
而不只看到一段脱离硬件环境的示例代码。

项目身份为 **DIY Electronics with AI**，也称 **DIY Lab**，GitHub 仓库名为 `diy-electronics-with-ai`。
欢迎从一个项目的接线、运行步骤或验证记录开始体验，也把完成验收并获准公开的实验整理成别人能接着使用的资料。

## 先从资料与技能开始

无需连接硬件即可使用产品目录、官方文档索引和 Agent Skills：

- [硬件产品目录](catalog/)：按厂商和精确型号查找规格与中英文官方资料；目录收录不代表实机验证。
- [板卡资料](boards/)：查看官方硬件能力、接口、电源和接线前的核对事项。
- [Agent Skills](docs/AGENT_SKILL.md)：查询知识、维护目录、开发项目、整理贡献、管理本机物料和制作 M5 图解手册。
- [选型研究](docs/research/open-source-electronics-catalogs.md)：比较资料来源与许可边界，再按研究文档查阅通信、定位和音频方案。

安装技能：`./scripts/install-agent-skill.sh`。查询技能直接读取公开资料；写入技能需要通过本地
`publication.developmentRoot` 配置独立私有开发目录。个人物料库与购买证据始终保存在 Git 之外。
本批资料与技能的审查范围及验证见[交付记录](docs/PUBLIC_KNOWLEDGE_RELEASE.md)。

## 项目与 Demo 展示

当前没有通过验收并获准发布的 Project 或 Demo，展示区暂为空。
进行中的实现与素材已经迁入仓库外的独立私有开发工作区；当前目录只保留公开交付准备资料。
验收和批准后才导入公开快照。
[已发布 Projects](projects/) · [已发布 Demos](demos/)。

工程验证进度与发布许可分别记录在 `project.yaml` 的 `status` 和 `publication` 中。
本仓库的完整开发历史禁止直接公开推送；公开交付由
[`scripts/pub`](scripts/pub) 按批准的文件范围导出，不附带 Git 历史。
使用方法与边界见[发布规则](docs/PUBLICATION_POLICY.md)。开发位置仅保存在本地 Git 配置
`publication.developmentRoot` 中；个人路径不进入 Git。

## 从项目代码到图解说明书

仓库内置 **[`m5-product-manual` · M5 产品图解手册](docs/m5-product-manual.md)**：
根据官方硬件资料、当前固件和实测记录整理接线、按键、屏幕状态与操作流程。
手册、截图与配图遵守同一发布门槛；草稿和未批准的项目样页不进入公开展示。

## 内容入口

- [`projects/`](projects/)：可独立构建、具有明确用途的完整工程。
- [`demos/`](demos/)：验证一种模块、接口或组合的最小可复现实验。
- [`components/`](components/)：经过复用验证的驱动和公共组件。
- [`boards/`](boards/)：开发板官方能力、接口、电源和接线核对资料。
- [`catalog/`](catalog/)：主流厂商、产品系列、规格字段和官方资料入口。
- [`hardware/`](hardware/)：原理图、PCB、线束、外壳和机械设计。
- [`docs/`](docs/)：仓库约定、协作方法和开源发布检查表。
- [`templates/demo/`](templates/demo/)：新建 Demo 时复制的模板。
- [`tests/`](tests/)：跨项目验证、硬件在环和日志检查工具。

## 基本原则

1. 一个 Demo 只证明一个主要结论，并给出复现步骤。
2. 接线前记录电压、电流、逻辑电平、引脚冲突和供电风险。
3. 编译通过不等于硬件验证通过；状态必须明确区分。
4. 串口日志、测量数据和测试脚本优先于照片或主观描述。
5. 失败实验也保留结论，但不提交密钥、订单隐私、固件备份和大体积构建产物。
6. 物料购买记录使用本机全局电子物料库，不复制进 Git；购买数量不等于现存库存。

## 人 + AI 协作入口

仓库以 `.agents/` 和 `AGENTS.md` 为规范源，便于 Codex、Claude Code 以及其他兼容 Agent
共享同一套项目规则和技能。`.claude` 指向 `.agents`，`CLAUDE.md` 指向 `AGENTS.md`，因此
不要在兼容入口维护重复内容。仓库内置的 `diy-electronics-materials` 技能只提供查询工具和数据
约定；个人库存数据库仍保存在用户数据目录中，不进入版本控制。

产品目录保存经过归一化的事实、验证状态和官方来源链接，不收录来源不明的参数，也不整份
复制厂商手册。目录内容用于选型和发现资料，实际接线前仍需核对具体 SKU、硬件修订版和原理图。

仓库也提供一组职责明确的用户级 Agent Skills。克隆后运行 `./scripts/install-agent-skill.sh`，
其他项目中的 Agent 就能分别查询知识、维护产品目录、建设硬件项目和制作 M5 图解说明书。
成果先在私有工作区提交，再由经过批准的公开快照贡献到公开仓库。显式调用使用 `$diy...` 或 `$m5-product-manual`，不是用于搜索文件的 `@`；
中文速查、安装和跨项目流程见 [`docs/AGENT_SKILL.md`](docs/AGENT_SKILL.md)。

这里把“AI-native”当作一种工程组织方式，而不是给传统仓库附加一个聊天入口：

- 任务边界、目录职责、安全规则和验证要求以 Agent 可直接读取的文件表达。
- 公开事实、工程经验、项目实现和 Git 贡献由不同 Skill 分工，便于 harness 组合与审计。
- 每个结论都尽量附带来源、验证等级或可复现步骤，Agent 不以推测补齐硬件事实。
- 私有资料留在仓库之外；只沉淀公开可核验资料和去标识化、可迁移的工程经验。
- 兼容入口只是适配层，规范源保持唯一，使不同 Agent harness 能在同一套约束下协作。

因此，这个仓库也可作为一种开放范式：把电子工程知识、真实硬件验证和 Agent 工作流放在
同一个可版本化、可审查、可复现的协作系统里。

详细约定见 [`docs/PROJECT_STRUCTURE.md`](docs/PROJECT_STRUCTURE.md) 和
[`docs/AI_COLLABORATION.md`](docs/AI_COLLABORATION.md)。同类开源项目与许可证边界的调研见
[`docs/research/open-source-electronics-catalogs.md`](docs/research/open-source-electronics-catalogs.md)。

## 状态词

`idea` → `prototype` → `build-verified` → `hardware-verified` → `stable`

| 状态 | 面向使用者的含义 |
| --- | --- |
| `idea` | 规划中，尚未形成可复现实现。 |
| `prototype` | 开发中，已有原型，关键验收仍未完成。 |
| `build-verified` | 开发中，构建已通过，硬件效果待验证。 |
| `hardware-verified` | 已有实机验证；必须查看具体板型、版本与验证范围，可能仍在开发中。 |
| `stable` | 当前声明范围已完成并通过验收，进入稳定维护；不代表所有设想功能都已实现。 |
| `retired` | 已停止维护，保留退役原因和替代方案。 |

状态以各项目的 `project.yaml` 和验证记录为准。只有达到 `stable` 且声明范围的验收完成，
才在展示区标为“已完成 / 稳定维护”；部分硬件验证不能代替项目完成验收。

发布资格另外要求 `publication.acceptance: passed`，以及对应目标的当前内容批准；
`stable` 本身不授权公开。

## 开源许可

本仓库以 [MIT License](LICENSE) 开源。使用、修改、分发或商用时，请保留原版权和许可声明。
厂商文档、商标、外部链接内容及硬件资料仍归各自权利人所有；仓库中的引用不改变其原始许可。
