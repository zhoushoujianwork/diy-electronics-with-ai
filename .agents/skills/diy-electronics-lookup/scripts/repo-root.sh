#!/usr/bin/env bash
set -euo pipefail
script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
repo_root="$(CDPATH= cd -- "$script_dir/../../../.." && pwd -P)"
[[ -f "$repo_root/AGENTS.md" && -f "$repo_root/.agents/skills/diy-electronics-lookup/SKILL.md" ]] || {
  echo "error: Skill is not linked to a DIY Electronics with AI checkout" >&2
  exit 1
}
printf '%s\n' "$repo_root"
