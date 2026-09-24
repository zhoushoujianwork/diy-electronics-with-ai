#!/usr/bin/env bash
set -euo pipefail
script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
repo_root="$(CDPATH= cd -- "$script_dir/../../../.." && pwd -P)"
[[ -f "$repo_root/AGENTS.md" && -d "$repo_root/projects" ]] || {
  echo "error: Skill is not linked to a DIY Electronics with AI checkout" >&2
  exit 1
}
printf '%s\n' "$repo_root"
