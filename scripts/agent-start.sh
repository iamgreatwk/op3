#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$project_root"

required=(AGENTS.md BASELINE.env docs/handoff/latest.md docs/bringup-status.md docs/test-matrix.md docs/decisions.md docs/build-environment.md docs/collaboration-framework.md)
for path in "${required[@]}"; do
  if [ ! -f "$path" ]; then
    printf '%s\n' "Missing required project entry file: $path" >&2
    exit 1
  fi
done

printf '%s\n' 'Project: OnePlus 3 clean rebuild'
printf 'Branch: '
git branch --show-current
printf 'Commit: '
git rev-parse HEAD
# shellcheck source=/dev/null
source "$project_root/BASELINE.env"
printf 'Baseline: %s (%s)\n' "${TARGET_KERNEL_RELEASE:-$TARGET_KERNEL_VERSION}" "$TARGET_KERNEL_TREE"
printf '%s\n' 'Read before changing files:'
printf '%s\n' AGENTS.md BASELINE.env docs/handoff/latest.md docs/bringup-status.md docs/test-matrix.md docs/decisions.md docs/build-environment.md docs/collaboration-framework.md
printf '%s\n' 'Boundary: agents do not run large builds or device flashing.'
