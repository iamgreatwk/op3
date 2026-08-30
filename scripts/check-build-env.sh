#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=/dev/null
source "$project_root/BASELINE.env"

required=(
  git make dtc mkbootimg abootimg fastboot simg2img
  "$CROSS_GCC" arm-linux-gnueabihf-gcc-11
)

missing=0
for tool in "${required[@]}"; do
  if command -v "$tool" >/dev/null 2>&1; then
    printf 'PASS %-30s %s\n' "$tool" "$(command -v "$tool")"
  else
    printf 'FAIL %-30s missing\n' "$tool" >&2
    missing=1
  fi
done

printf '\nHost: '
. /etc/os-release
printf '%s\n' "${PRETTY_NAME}"
printf 'ARM64 compiler: '
"$CROSS_GCC" --version | head -n 1
printf 'Free disk: '
df -h . | awk 'NR == 2 { print $4 }'
printf 'Kernel baseline: %s (%s)\n' "${TARGET_KERNEL_RELEASE:-$TARGET_KERNEL_VERSION}" "$TARGET_KERNEL_TAG"

exit "$missing"
