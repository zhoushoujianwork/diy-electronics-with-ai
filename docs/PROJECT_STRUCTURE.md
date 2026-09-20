# 目录与工程约定

## 顶层目录

| 目录 | 放什么 | 不放什么 |
| --- | --- | --- |
| `projects/` | 有完整用途、可以独立构建和发布的工程 | 单传感器试验、随手代码 |
| `demos/` | 单一模块、协议或硬件组合的最小验证 | 尚未拆清边界的大型产品 |
| `components/` | 多个工程可复用的驱动、协议库和算法 | 只被一个 Demo 使用的私有实现 |
| `boards/` | 板卡规格、引脚图、供电限制、板级配置 | 未核对来源的网络引脚表 |
| `catalog/` | 厂商、产品系列、归一化规格、官方资料入口 | 无来源参数、整份第三方手册 |
| `hardware/` | KiCad/EasyEDA/CAD、线束、BOM、装配资料 | 商城订单、私人地址、无授权资料 |
| `docs/` | 全仓库知识、决策和协作规范 | 某个工程独有的详细实现 |
| `templates/` | 新工程模板 | 正在运行的业务代码 |
| `tests/` | 跨工程测试和硬件在环工具 | 项目内部单元测试 |
| `.agents/` | 仓库内共享技能和 Agent 资源的规范源 | 个人数据库、凭证、机器专属状态 |

## `projects` 与 `demos` 的边界

出现以下任一情况时使用 `projects/`：

- 有多个功能模块或多个硬件角色；
- 有面向使用者的完整使用流程；
- 需要独立版本、发布物或长期维护。

只验证一种器件、接口、协议或组合时使用 `demos/`。Demo 经两个以上工程复用后，可以把
稳定部分提炼到 `components/`，但原 Demo 继续作为最小示例和回归验证入口。

## 标准工程布局

```text
<slug>/
├── README.md
├── project.yaml
├── firmware/       # MCU/FPGA 固件，可选
├── software/       # PC/Web/App，可选
├── hardware/       # 原理图、PCB、线束、结构，可选
├── docs/           # 设计与验证记录
├── tests/          # 工程内测试
├── tools/          # 工程专用工具
└── assets/         # 小型、可再分发的图片和样例
```

不为了凑目录而建立空目录；没有内容的可选目录可以省略。

## 产品目录

`catalog/` 用于跨厂商发现和选型，记录厂商元数据、产品系列、来源访问日期和验证状态。目录
不能替代具体硬件修订版的官方原理图；除非再分发许可明确允许，否则只链接第三方手册，不把
整份文件提交到仓库。

## 命名

- 目录：小写 ASCII `kebab-case`，例如 `at6668-gps-uart`。
- C/C++：跟随工程既有风格；公共 API 必须带模块前缀。
- 板型：优先使用制造商 + 精确型号/修订，例如 `m5stack-sticks3-k150`。
- 文件中出现“支持”时必须注明 `build-verified` 或 `hardware-verified`。

## `project.yaml`

该文件用于目录索引和后续自动生成文档，最少包含：

```yaml
schema: 1
id: vendor-or-purpose-slug
name: Human-readable name
kind: demo
status: prototype
summary: One sentence
owners:
  - human-ai-lab
toolchains: []
boards: []
modules: []
validation:
  build: pending
  hardware: pending
```

字段必须描述当前事实；不能因为计划购买或代码已写好就把硬件状态标为通过。

## 本地产物与隐私

以下内容默认不进入 Git：构建目录、`sdkconfig`、下载依赖、固件备份、完整串口日志、音频/视频
大文件、商城订单证据、物料数据库、凭证和 `.env`。需要共享的验证结果应先去隐私并压缩成文档摘要。
