#!/usr/bin/env bash
set -euo pipefail
script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
repo_root="$(CDPATH= cd -- "$script_dir/../../../.." && pwd -P)"
# A publication-staging checkout may route writes to private development.
# This machine-local Git setting is not shipped with a public clone.
development_root=""
if command -v git >/dev/null 2>&1; then
  development_root="$(git -C "$repo_root" config --local --get publication.developmentRoot 2>/dev/null || true)"
  if [[ -n "$development_root" ]]; then
    [[ "$development_root" == /* ]] || development_root="$repo_root/$development_root"
    repo_root="$(CDPATH= cd -- "$development_root" && pwd -P)" || {
      echo "error: configured publication.developmentRoot is unavailable" >&2
      exit 1
    }
  fi
fi
if [[ -f "$repo_root/AGENTS.md" ]] && grep -q "publication-staging workspace" "$repo_root/AGENTS.md"; then
  echo "error: publication staging is read-only; configure publication.developmentRoot to a private development checkout" >&2
  exit 1
fi
[[ -f "$repo_root/AGENTS.md" && -f "$repo_root/.agents/skills/diy-electronics-catalog/SKILL.md" ]] || {
  echo "error: Skill is not linked to a DIY Electronics with AI checkout" >&2
  exit 1
}
printf '%s\n' "$repo_root"
