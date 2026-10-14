#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)"
test_root="$(mktemp -d "${TMPDIR:-/tmp}/ai-diy-skill-test.XXXXXX")"
trap 'rm -rf "$test_root"' EXIT

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
      expected_root="$repo_root"
      if [[ "$skill_name" != "diy-electronics-lookup" ]]; then
        configured_root="$(git -C "$repo_root" config --local --get publication.developmentRoot || true)"
        if [[ -n "$configured_root" ]]; then
          [[ "$configured_root" == /* ]] || configured_root="$repo_root/$configured_root"
          expected_root="$(CDPATH= cd -- "$configured_root" && pwd -P)"
        fi
      fi
      if [[ "$skill_name" != "diy-electronics-lookup" ]] && grep -q "publication-staging workspace" "$expected_root/AGENTS.md"; then
        if "$skill_link/scripts/repo-root.sh" >"$test_root/rejected-root.out" 2>"$test_root/rejected-root.err"; then
          echo "error: write Skill accepted a publication-staging workspace" >&2
          exit 1
        fi
        grep -q "read-only" "$test_root/rejected-root.err"
      else
        [[ "$("$skill_link/scripts/repo-root.sh")" == "$expected_root" ]]
      fi
    fi
  done
done

# Exercise public-clone routing using isolated local Git configuration.
public_root="$test_root/public"
private_root="$test_root/private"
mkdir -p "$public_root" "$private_root"
cp -R "$repo_root/.agents" "$public_root/.agents"
cp -R "$repo_root/.agents" "$private_root/.agents"
printf '%s\n' 'This checkout is a publication-staging workspace.' >"$public_root/AGENTS.md"
printf '%s\n' 'This checkout is a private development workspace.' >"$private_root/AGENTS.md"
git init -q "$public_root"
git init -q "$private_root"
public_root="$(CDPATH= cd -- "$public_root" && pwd -P)"
private_root="$(CDPATH= cd -- "$private_root" && pwd -P)"
[[ "$("$public_root/.agents/skills/diy-electronics-lookup/scripts/repo-root.sh")" == "$public_root" ]]
for skill_name in diy-electronics-catalog diy-electronics-project diy-electronics-contribute m5-product-manual; do
  if "$public_root/.agents/skills/$skill_name/scripts/repo-root.sh" >"$test_root/rejected-root.out" 2>"$test_root/rejected-root.err"; then
    echo "error: write Skill accepted public staging without a private development root" >&2
    exit 1
  fi
  grep -q "read-only" "$test_root/rejected-root.err"
done
git -C "$public_root" config --local publication.developmentRoot "$public_root"
if "$public_root/.agents/skills/diy-electronics-project/scripts/repo-root.sh" >"$test_root/rejected-root.out" 2>"$test_root/rejected-root.err"; then
  echo "error: configured staging root was accepted as private development" >&2
  exit 1
fi
git -C "$public_root" config --local publication.developmentRoot "$private_root"
for skill_name in diy-electronics-catalog diy-electronics-project diy-electronics-contribute m5-product-manual; do
  [[ "$("$public_root/.agents/skills/$skill_name/scripts/repo-root.sh")" == "$private_root" ]]
done

echo "agent skill installer test passed"
