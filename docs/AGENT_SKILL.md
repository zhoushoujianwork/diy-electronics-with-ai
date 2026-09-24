# Install the repository's Agent Skills

DIY Electronics with AI provides several focused user-level Agent Skills. Installation uses symbolic links, so
an Agent working from another project can search this checkout and contribute verified electronics knowledge
back to the same Git repository. New Skill directories are discovered automatically by the installer.

## 中文速查：怎么呼出

相关任务通常会自动匹配 Skill，不写名字也可以。需要手动指定时输入 `$diy`，从 Skill 列表中
选择；不要输入 `@diy`，因为 `@` 搜索的是文件和目录，所以只会看到英文文件夹名。Skill 选择器中的名称、
简介和默认提示采用中英双语，方便中文理解和英文关键词检索。

| 想做什么 | 应调用的 Skill | 中文含义 |
| --- | --- | --- |
| 查已有板卡、模块、接线或项目经验 | `$diy-electronics-lookup` | 只读知识查询 |
| 查官方资料并补厂商、产品或配件目录 | `$diy-electronics-catalog` | 产品目录沉淀 |
| 建板卡档案、Demo、固件或完整项目 | `$diy-electronics-project` | 电子项目工程化 |
| 按 M5Stack 硬件和项目固件制作图解使用说明书 | `$m5-product-manual` | M5 产品图解手册 |
| 提交代码、建分支或创建 PR | `$diy-electronics-contribute` | Git 与 PR 贡献 |
| 查自己买过的物料和盘点数量 | `$diy-electronics-materials` | 个人物料查询 |

例如：`使用 $diy-electronics-lookup 查一下 M5Stack HAT PIR 的资料和验证边界。`

## Install

```sh
git clone https://github.com/zhoushoujianwork/diy-electronics-with-ai.git
cd diy-electronics-with-ai
./scripts/install-agent-skill.sh
```

The default target is the portable user-level `~/.agents/skills` location. Each repository Skill gets its own
link. Install client-specific compatibility links only when a client does not scan `~/.agents/skills`:

```sh
./scripts/install-agent-skill.sh --target codex
./scripts/install-agent-skill.sh --target claude
./scripts/install-agent-skill.sh --target all
```

The installer is idempotent when links already point to this checkout. It refuses to overwrite a real
directory, a broken link, or a link to another checkout. Use `--dry-run` to preview every destination.

General lab Skills use the `diy-electronics-` namespace; the M5Stack manual Skill is named `m5-product-manual`.
During an upgrade, the installer removes only legacy
`electronic-materials` or `electronics-lab-*` symbolic links that it can prove pointed to this same checkout;
real directories and unrelated links are preserved.

Restart or reload the Agent after installation. Automatic discovery is enabled, with separate responsibilities:

| Skill | Responsibility |
| --- | --- |
| `$diy-electronics-lookup` | 只读查询产品目录、板卡、Demo 和项目知识 |
| `$diy-electronics-catalog` | 核验厂商官方资料并维护公开产品目录 |
| `$diy-electronics-project` | 建设板卡档案、Demo、项目、组件和验证证据 |
| `$m5-product-manual` | 根据 M5Stack 硬件、项目固件和验证资料制作图解说明书，校对按键、接线与操作流程 |
| `$diy-electronics-contribute` | 校验并组织 Commit、分支、推送和 Pull Request |
| `$diy-electronics-materials` | 查询本机购买与盘点数据库，不公开个人数据 |

## Use from another project

The installed Skills support a connected workflow:

1. Search the lab before rediscovering existing electronics knowledge.
2. Research official sources or perform real hardware validation using the appropriate specialized Skill.
3. Write reusable public facts and sanitized evidence back to the linked checkout.
4. Validate and commit each coherent milestone, then create a focused pull request when requested.

The source checkout remains an ordinary Git repository. Contributors can inspect changes, create branches,
commit, push and open PRs with their Agent's normal Git/GitHub tools. The Skill never makes purchase history,
credentials, personal inventory or private evidence public.

## Update or move the checkout

Pulling new commits updates all installed Skills immediately because their links point at the checkout. If the
checkout is moved, rerun the installer from the new location after removing the old links manually. The script
does not replace existing targets automatically.
