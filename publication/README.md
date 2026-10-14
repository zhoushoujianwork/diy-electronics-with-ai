# 发布单元

项目与 Demo 的发布元数据保存在各自的 `project.yaml`。这个目录用于管理跨项目的文档、
工具、索引等独立交付范围，从[模板](../templates/publication.yaml)创建新清单。

资料与技能的独立交付范围见 [knowledge-skills-2026-10.yaml](knowledge-skills-2026-10.yaml)，
对应验收见[交付记录](../docs/PUBLIC_KNOWLEDGE_RELEASE.md)。清单状态与内容摘要共同决定资格；
`ready` 表示审查完成、等待当前精确范围的人类批准，不表示已经公开。
不得将建立清单、本地 commit 或工具验证视为公开授权。
查看私有开发清单使用 `./scripts/pub status`；操作见[发布规则](../docs/PUBLICATION_POLICY.md)。
