# DIY Electronics with AI

一个 AI-native、harness-friendly 的开放电子工程实验室：人负责目标、判断和真实世界验证，
AI Agent 负责检索、整理、实现、检查与协作编排。这里保存可复现的硬件 Demo、完整项目、
可复用驱动、接线依据、测试证据和去标识化工程经验，让其他电子爱好者和 Agent 都能直接复用，
而不只看到一段脱离硬件环境的示例代码。

项目身份为 **DIY Electronics with AI**，GitHub 仓库名为 `diy-electronics-with-ai`。

## 内容入口

- [`projects/`](projects/)：可独立构建、具有明确用途的完整工程。
- [`demos/`](demos/)：验证一种模块、接口或组合的最小可复现实验。
- [`components/`](components/)：经过复用验证的驱动和公共组件。
- [`boards/`](boards/)：开发板能力、引脚、电源和已验证配置。
- [`catalog/`](catalog/)：主流厂商、产品系列、规格字段和官方资料入口。
- [`hardware/`](hardware/)：原理图、PCB、线束、外壳和机械设计。
- [`docs/`](docs/)：仓库约定、协作方法和开源发布检查表。
- [`templates/`](templates/)：新建 Demo/项目时复制的模板。
- [`tests/`](tests/)：跨项目验证、硬件在环和日志检查工具。

第一个完整项目是 [`EV Engine Sound`](projects/ev-engine-sound/)，它保留原来的桌面音频核心、
ESP-IDF 固件、三种板级配置和实机验证记录。

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
其他项目中的 Agent 就能分别查询知识、维护产品目录、建设硬件项目，并把验证后的成果通过
Commit 或 PR 贡献回同一个 Git 仓库。显式调用使用 `$diy...`，不是用于搜索文件的 `@diy...`；
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

无法继续维护的内容标为 `retired`，不把未上板代码描述成“已支持”。

## 开源许可

本仓库以 [MIT License](LICENSE) 开源。使用、修改、分发或商用时，请保留原版权和许可声明。
厂商文档、商标、外部链接内容及硬件资料仍归各自权利人所有；仓库中的引用不改变其原始许可。
