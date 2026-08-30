#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=/dev/null
source "$project_root/BASELINE.env"

kernel_source="${KERNEL_SOURCE:-$project_root/${TARGET_KERNEL_SOURCE_DIR:-source/linux-${TARGET_KERNEL_VERSION}}}"

if [ -e "$kernel_source" ]; then
  printf 'Refusing to overwrite existing path: %s\n' "$kernel_source" >&2
  exit 1
fi

printf 'Fetching %s (%s) from %s\n' "$TARGET_KERNEL_TAG" "$TARGET_KERNEL_TREE" "$TARGET_KERNEL_REMOTE"
git clone --branch "$TARGET_KERNEL_TAG" --single-branch --depth=1 \
  "$TARGET_KERNEL_REMOTE" "$kernel_source"
git -C "$kernel_source" rev-parse HEAD
