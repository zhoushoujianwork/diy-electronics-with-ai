---
name: diy-electronics-contribute
description: "把已验证更新整理为聚焦的 Commit、分支和 Pull Request；负责 Git 协作，不替代技术验证。 Package verified updates into focused commits, branches, and pull requests; handles Git collaboration, not technical verification."
---

# Electronics Lab Contribution

写入技能会遵循 checkout 本地 Git 配置 `publication.developmentRoot`；标记为 publication-staging 的公开克隆缺少配置时拒绝写入定位。
始终先阅读解析后仓库的协作规则；公开克隆不包含未发布项目，缺少的目标不能视为已有实例。

Resolve the checkout with `scripts/repo-root.sh` and read its `AGENTS.md`. This Skill packages already verified
catalog or engineering work; use `$diy-electronics-catalog` or `$diy-electronics-project` for content rules.

1. Inspect `git status`, remotes, current branch and the complete diff. Preserve unrelated user changes and
   never discard work to make the contribution convenient.
2. Ensure the update cites public sources, contains no credentials, order evidence, personal inventory,
   private logs or generated build output, and states assumptions and pending hardware checks.
3. Run proportionate validators, tests, builds or hardware checks. Do not claim more than the evidence proves.
4. Commit every completed update or coherent milestone with a focused outcome-oriented message.
5. Before any public operation, read `docs/PUBLICATION_POLICY.md`. Implementation and local commits
   do not approve publication. Keep development commits/private backups separate from public delivery.
6. When public contribution is explicitly requested, require a stable, accepted publication unit, human
   approval for the exact target/content digest, and a passing `scripts/pub --check <manifest> --target git`.
   Export with `scripts/pub export` outside this checkout. Use only that approved snapshot in a separate
   public repository; never push/merge private development history or draft PRs. Preserve the public
   repository's history and unrelated content. Branches/PRs must not expose unapproved files or history.
   Summarize verified facts, tests, limitations and rollback.
7. Attach or report the PR using the current Agent environment's supported mechanism.

Do not push or open a PR merely because a possible improvement was noticed. External publication must be in
scope. When contribution is not authorized, leave a clean local commit only if requested; otherwise report the
finding and proposed repository location.
