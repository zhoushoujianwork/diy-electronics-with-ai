# Repository tests

这里保存跨项目的目录校验、清单生成、硬件在环调度和通用日志检查。项目自身测试应留在对应
的 `projects/<slug>/tests` 或 `demos/<slug>/tests`。

`test_install_agent_skill.sh` 在隔离的临时用户目录中自动发现全部仓库 Skill，并验证它们对
Agent Skills、Codex 和 Claude 三种用户级目标的首次安装、重复安装、软链接解析和仓库定位，
同时验证旧名称软链接的安全迁移，不修改真实用户配置。

`test_publication.py` 使用隔离的临时 Git 仓库验证发布门槛、批准失效、精确快照范围、
危险路径与附件、依赖检查，以及真实本地 `git push` 被 pre-push 阻断。不访问网络或真实远程。

```sh
python3 -m unittest discover -s tests -p 'test_publication.py' -v
```

YAML 清单使用 PyYAML；JSON 也是合法 YAML，检查器和测试支持标准库读取 JSON 格式清单。
