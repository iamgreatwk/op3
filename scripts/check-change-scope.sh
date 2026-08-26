#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
required=(BASELINE.env docs/handoff/latest.md docs/bringup-status.md docs/git-workflow.md)
failed=0

for path in "${required[@]}"; do
  if [ -f "$project_root/$path" ]; then
    printf 'PASS required file: %s\n' "$path"
  else
    printf 'FAIL missing required file: %s\n' "$path" >&2
    failed=1
  fi
done

for forbidden in out artifacts cache diag_archive source; do
  if git -C "$project_root" diff --cached --name-only | grep -q "^${forbidden}/"; then
    printf 'FAIL generated/local path staged: %s/\n' "$forbidden" >&2
    failed=1
  fi
done

if git -C "$project_root" diff --cached --name-only | grep -q '^kernel/patches/' && \
   ! git -C "$project_root" diff --cached --name-only | grep -q '^docs/'; then
  printf 'FAIL a kernel patch requires an accompanying documentation change\n' >&2
  failed=1
fi

exit "$failed"
