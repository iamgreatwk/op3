#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$project_root"

git rev-parse --verify HEAD >/dev/null
for branch in upstream-7.2 bringup userspace-baseline legacy/6.3.1-reference; do
  if git show-ref --verify --quiet "refs/heads/$branch"; then
    printf 'EXISTS %s\n' "$branch"
  else
    git branch "$branch"
    printf 'CREATED %s\n' "$branch"
  fi
done

printf 'Current branch: '
git branch --show-current
