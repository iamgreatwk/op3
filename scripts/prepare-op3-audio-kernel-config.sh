#!/usr/bin/env bash
set -euo pipefail

# Prepare, but do not compile, the kernel configuration for OP3-AUDIO-MIC-001.
# Kernel compilation remains an owner-run action.  Run this after applying the
# existing own-DTB series, then use the printed make command to build Image.gz
# and DTBs from the prepared output directory.

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=/dev/null
source "$project_root/BASELINE.env"

kernel_source="${KERNEL_SOURCE:-$project_root/$TARGET_KERNEL_SOURCE_DIR}"
output_dir="${KERNEL_OUTPUT:-$project_root/$TARGET_KERNEL_OUTPUT_DIR-audio}"
fragment="$project_root/kernel/configs/oneplus3-audio.fragment"

test -f "$kernel_source/Makefile"
test -f "$fragment"

source_commit="$(git -C "$kernel_source" rev-parse HEAD)"
case "$source_commit" in
  "$TARGET_KERNEL_COMMIT"|91df7ccd284e5c62c5aed13c2738192b96c1f8dd)
    ;;
  *)
    printf 'Refusing unexpected kernel source commit: %s\n' "$source_commit" >&2
    printf 'Expected baseline %s or the documented own-DTB commit 91df7ccd.\n' \
      "$TARGET_KERNEL_COMMIT" >&2
    exit 1
    ;;
esac

mkdir -p "$output_dir"
make -C "$kernel_source" O="$output_dir" ARCH=arm64 \
  CROSS_COMPILE="$CROSS_COMPILE" CC="$CROSS_GCC" defconfig
"$kernel_source/scripts/kconfig/merge_config.sh" -m -O "$output_dir" \
  "$output_dir/.config" "$fragment"
make -C "$kernel_source" O="$output_dir" ARCH=arm64 \
  CROSS_COMPILE="$CROSS_COMPILE" CC="$CROSS_GCC" olddefconfig

printf 'Prepared audio config: %s\n' "$output_dir/.config"
printf 'Config SHA256: %s\n' "$(sha256sum "$output_dir/.config" | awk '{print $1}')"
printf 'Owner build command (not run by this script):\n'
printf '  make -C %q O=%q ARCH=arm64 CROSS_COMPILE=%q CC=%q -j"$(nproc)" Image.gz dtbs\n' \
  "$kernel_source" "$output_dir" "$CROSS_COMPILE" "$CROSS_GCC"
