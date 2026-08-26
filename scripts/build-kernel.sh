#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=/dev/null
source "$project_root/BASELINE.env"

kernel_source="${KERNEL_SOURCE:-$project_root/source/linux-${TARGET_KERNEL_VERSION}}"
output_dir="${KERNEL_OUTPUT:-$project_root/out/linux-${TARGET_KERNEL_VERSION}-defconfig}"
jobs="${JOBS:-$(nproc)}"

test -f "$kernel_source/Makefile"

source_version="$(awk -F ' = ' '$1 == "VERSION" { print $2 }' "$kernel_source/Makefile")"
source_patchlevel="$(awk -F ' = ' '$1 == "PATCHLEVEL" { print $2 }' "$kernel_source/Makefile")"
actual_kernel_version="${source_version}.${source_patchlevel}"
if [ "${ALLOW_LEGACY_KERNEL:-0}" != 1 ] && [ "$actual_kernel_version" != "$TARGET_KERNEL_VERSION" ]; then
  printf 'Refusing to build kernel %s: expected %s. Set ALLOW_LEGACY_KERNEL=1 only for an explicit legacy task.\n' \
    "$actual_kernel_version" "$TARGET_KERNEL_VERSION" >&2
  exit 1
fi

source_commit="$(git -C "$kernel_source" rev-parse HEAD)"
if [ "${ALLOW_LEGACY_KERNEL:-0}" != 1 ] && [ "$source_commit" != "$TARGET_KERNEL_COMMIT" ]; then
  printf 'Refusing to build source commit %s: expected pristine %s (%s).\n' \
    "$source_commit" "$TARGET_KERNEL_TAG" "$TARGET_KERNEL_COMMIT" >&2
  exit 1
fi

if [ "${ALLOW_LEGACY_KERNEL:-0}" != 1 ] && \
  { ! git -C "$kernel_source" diff --quiet || ! git -C "$kernel_source" diff --cached --quiet; }; then
  printf 'Refusing to build a modified kernel source tree: %s\n' "$kernel_source" >&2
  exit 1
fi

config_path="$output_dir/.config"
config_sha="not-created"
if [ -f "$config_path" ]; then
  config_sha="$(sha256sum "$config_path" | awk '{print $1}')"
fi

printf 'TARGET_KERNEL_VERSION=%s\n' "$TARGET_KERNEL_VERSION"
printf 'SOURCE_COMMIT=%s\n' "$source_commit"
printf 'DEVICE=%s\n' "$TARGET_DEVICE"
printf 'CONFIG_SHA=%s\n' "$config_sha"
printf 'TOOLCHAIN=%s\n' "$("$CROSS_GCC" --version | head -n 1)"

make -C "$kernel_source" O="$output_dir" \
  ARCH=arm64 CROSS_COMPILE="$CROSS_COMPILE" CC="$CROSS_GCC" \
  defconfig

make -C "$kernel_source" O="$output_dir" \
  ARCH=arm64 CROSS_COMPILE="$CROSS_COMPILE" CC="$CROSS_GCC" \
  -j"$jobs" Image.gz dtbs
