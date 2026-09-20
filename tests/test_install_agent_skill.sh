#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
test_root="$(mktemp -d "${TMPDIR:-/tmp}/ai-diy-skill-test.XXXXXX")"
trap 'rm -rf "$test_root"' EXIT

export HOME="$test_root/home"
export AGENTS_HOME="$test_root/agents-home"
export CODEX_HOME="$test_root/codex-home"
export CLAUDE_HOME="$test_root/claude-home"

mkdir -p "$AGENTS_HOME/skills" "$CODEX_HOME/skills" "$CLAUDE_HOME/skills"
ln -s "$repo_root/.agents/skills/electronic-materials" "$AGENTS_HOME/skills/electronic-materials"
ln -s "$repo_root/.agents/skills/electronics-lab-lookup" "$CODEX_HOME/skills/electronics-lab-lookup"
ln -s "$repo_root/.agents/skills/electronics-lab-contribute" "$CLAUDE_HOME/skills/electronics-lab-contribute"

"$repo_root/scripts/install-agent-skill.sh" --target all
"$repo_root/scripts/install-agent-skill.sh" --target all

[[ ! -L "$AGENTS_HOME/skills/electronic-materials" ]]
[[ ! -L "$CODEX_HOME/skills/electronics-lab-lookup" ]]
[[ ! -L "$CLAUDE_HOME/skills/electronics-lab-contribute" ]]

for skill_file in "$repo_root"/.agents/skills/*/SKILL.md; do
  skill_source="${skill_file%/SKILL.md}"
  skill_name="$(basename -- "$skill_source")"
  for skills_home in "$AGENTS_HOME/skills" "$CODEX_HOME/skills" "$CLAUDE_HOME/skills"; do
    skill_link="$skills_home/$skill_name"
    [[ -L "$skill_link" ]]
    [[ "$(CDPATH= cd -- "$skill_link" && pwd -P)" == "$skill_source" ]]
    if [[ -x "$skill_link/scripts/repo-root.sh" ]]; then
      [[ "$("$skill_link/scripts/repo-root.sh")" == "$repo_root" ]]
    fi
  done
done

echo "agent skill installer test passed"
