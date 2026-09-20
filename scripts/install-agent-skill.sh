#!/usr/bin/env bash
set -euo pipefail

script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)"
repo_root="$(CDPATH= cd -- "$script_dir/.." && pwd -P)"
skills_root="$repo_root/.agents/skills"
target_kind="agents"
dry_run=0
legacy_skill_names=(
  "electronic-materials"
  "electronics-lab-lookup"
  "electronics-lab-catalog"
  "electronics-lab-project"
  "electronics-lab-contribute"
)

usage() {
  cat <<'EOF'
Usage: scripts/install-agent-skill.sh [--target agents|codex|claude|all] [--dry-run]

Install all DIY Electronics with AI skills using user-level symbolic links.

  agents  ~/.agents/skills (default, portable Agent Skills location)
  codex   ${CODEX_HOME:-~/.codex}/skills
  claude  ${CLAUDE_HOME:-~/.claude}/skills
  all     install all three compatibility targets
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --target)
      [[ $# -ge 2 ]] || { echo "error: --target requires a value" >&2; exit 2; }
      target_kind="$2"
      shift 2
      ;;
    --dry-run)
      dry_run=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "error: unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

case "$target_kind" in
  agents|codex|claude|all) ;;
  *) echo "error: unsupported target: $target_kind" >&2; exit 2 ;;
esac

skill_sources=()
for skill_file in "$skills_root"/*/SKILL.md; do
  [[ -f "$skill_file" ]] || continue
  skill_sources+=("${skill_file%/SKILL.md}")
done

[[ ${#skill_sources[@]} -gt 0 ]] || {
  echo "error: no Skill sources found below $skills_root" >&2
  exit 1
}

install_link() {
  local label="$1"
  local skills_dir="$2"
  local skill_source="$3"
  local skill_name
  skill_name="$(basename -- "$skill_source")"
  local target="$skills_dir/$skill_name"

  if [[ -L "$target" ]]; then
    local resolved_target
    resolved_target="$(CDPATH= cd -- "$target" 2>/dev/null && pwd -P)" || {
      echo "error: $label target is a broken symbolic link: $target" >&2
      return 1
    }
    if [[ "$resolved_target" == "$skill_source" ]]; then
      echo "$label: already installed at $target"
      return 0
    fi
    echo "error: $label target already links elsewhere: $target" >&2
    return 1
  fi

  if [[ -e "$target" ]]; then
    echo "error: $label target already exists and was not changed: $target" >&2
    return 1
  fi

  if [[ $dry_run -eq 1 ]]; then
    echo "$label: would link $target -> $skill_source"
    return 0
  fi

  mkdir -p "$skills_dir"
  ln -s "$skill_source" "$target"
  echo "$label: installed $target -> $skill_source"
}

remove_legacy_links() {
  local label="$1"
  local skills_dir="$2"
  local legacy_name legacy_target expected_source link_value
  for legacy_name in "${legacy_skill_names[@]}"; do
    legacy_target="$skills_dir/$legacy_name"
    [[ -L "$legacy_target" ]] || continue
    expected_source="$skills_root/$legacy_name"
    link_value="$(readlink "$legacy_target")"
    if [[ "$link_value" != "$expected_source" ]]; then
      echo "$label: preserved unrelated legacy link $legacy_target" >&2
      continue
    fi
    if [[ $dry_run -eq 1 ]]; then
      echo "$label: would remove legacy link $legacy_target"
    else
      rm "$legacy_target"
      echo "$label: removed legacy link $legacy_target"
    fi
  done
}

install_suite() {
  local label="$1"
  local skills_dir="$2"
  local skill_source
  remove_legacy_links "$label" "$skills_dir"
  for skill_source in "${skill_sources[@]}"; do
    install_link "$label" "$skills_dir" "$skill_source"
  done
}

install_agents() {
  install_suite "Agent Skills" "${AGENTS_HOME:-$HOME/.agents}/skills"
}

install_codex() {
  install_suite "Codex" "${CODEX_HOME:-$HOME/.codex}/skills"
}

install_claude() {
  install_suite "Claude" "${CLAUDE_HOME:-$HOME/.claude}/skills"
}

case "$target_kind" in
  agents) install_agents ;;
  codex) install_codex ;;
  claude) install_claude ;;
  all)
    install_agents
    install_codex
    install_claude
    ;;
esac

echo "Installed ${#skill_sources[@]} Skill(s). Restart or reload the Agent to discover them."
