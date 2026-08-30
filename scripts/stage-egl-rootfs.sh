#!/usr/bin/env bash
set -euo pipefail

# Collect the Buildroot-built EGL stack into a tarball that unpacks to
# /opt/op3-egl on the device's sda15 root filesystem.
#
# The boot image stays minimal: the tarball is deployed to sda15, not packed
# into the initramfs (docs/decisions.md, OnePlus 3 boot.img size limit).
#
# Usage:
#   scripts/stage-egl-rootfs.sh [buildroot-target] [output-tarball]

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
target="${1:-$project_root/out/buildroot-op3-egl/target}"
output="${2:-$project_root/artifacts/op3-egl-bundle.tar.gz}"
run_script="$project_root/boot/egl-test/opt/op3-egl/run.sh"

required_libs=(libEGL libGLESv2 libgbm libdrm libglapi)
optional_libs=(libOSMesa)

for input in "$target" "$run_script"; do
  test -e "$input" || { printf 'Missing input: %s\n' "$input" >&2; exit 1; }
done

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

bundle="$tmpdir/opt/op3-egl"
mkdir -p "$bundle/bin" "$bundle/lib/dri"

for lib in "${required_libs[@]}"; do
  matches=($(ls "$target"/usr/lib/$lib.so* 2>/dev/null || true))
  if [ "${#matches[@]}" -eq 0 ]; then
    printf 'Missing library in %s: %s\n' "$target/usr/lib" "$lib" >&2
    exit 1
  fi
  cp -a "${matches[@]}" "$bundle/lib/"
done

for lib in "${optional_libs[@]}"; do
  cp -a "$target"/usr/lib/$lib.so* "$bundle/lib/" 2>/dev/null || true
done

if [ -d "$target/usr/lib/dri" ]; then
  cp -a "$target"/usr/lib/dri/. "$bundle/lib/dri/"
fi

if [ -f "$target/usr/bin/kmscube" ]; then
  cp -a "$target/usr/bin/kmscube" "$bundle/bin/kmscube"
else
  printf 'Missing %s: enable BR2_PACKAGE_MESA3D_DEMOS_KMSCUBE\n' \
    "$target/usr/bin/kmscube" >&2
  exit 1
fi

install -m 0755 "$run_script" "$bundle/run.sh"

tar -czf "$output" -C "$tmpdir" opt/op3-egl

printf 'bundle contents:\n'
tar -tzf "$output" | LC_ALL=C sort
printf 'output=%s\n' "$output"
ls -l "$output"
sha256sum "$output"
printf '\nDeploy on the device (sda15 is /newroot):\n'
printf '  mkdir -p /newroot/opt && tar -xzf op3-egl-bundle.tar.gz -C /newroot\n'
