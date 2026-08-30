#!/usr/bin/env bash
set -euo pipefail

# Collect the Buildroot-built Wayland/Weston stack into a tarball that unpacks
# to /opt/op3-weston on the device's sda15 root filesystem.
#
# Unlike the EGL bundle (a NEEDED sweep), this stages the WHOLE Buildroot
# target tree: weston dlopens its backend/renderer/shell modules through the
# compile-time-absolute path /usr/lib/weston, and libinput / libxkbcommon /
# eudev read data from /usr/share and /usr/lib/udev — none of these can be
# redirected with an environment variable. run.sh symlinks those absolute
# paths into the bundle on the device. A whole-tree copy guarantees that every
# such path exists inside the bundle.
#
# Self-contained like the EGL bundle: carries the glibc loader and all
# libraries, so it does not depend on the libc of the sda15 rootfs. run.sh
# invokes every binary through the bundled loader explicitly.
#
# Usage:
#   scripts/stage-weston-rootfs.sh [buildroot-target] [output-tarball]

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
target="${1:-$project_root/out/buildroot-op3-egl/target}"
output="${2:-$project_root/artifacts/op3-weston-bundle.tar.gz}"
run_script="$project_root/boot/weston-test/opt/op3-weston/run.sh"

# bin/lib pieces that must exist for the gate to make any sense. weston 14
# keeps backends/renderers in /usr/lib/libweston-14 and shells in
# /usr/lib/weston; eudev installs its daemon at /sbin/udevd.
required_bins=(weston weston-simple-egl udevadm)
required_libs=(usr/lib/libweston-14/drm-backend.so
               usr/lib/libweston-14/gl-renderer.so
               sbin/udevd
               usr/lib/gbm/dri_gbm.so
               usr/lib/libEGL.so.1
               usr/lib/libgallium-*.so*)

for input in "$target" "$run_script"; do
  test -e "$input" || { printf 'Missing input: %s\n' "$input" >&2; exit 1; }
done

missing=0
for bin in "${required_bins[@]}"; do
  test -e "$target/usr/bin/$bin" || {
    printf 'Missing binary in target: usr/bin/%s\n' "$bin" >&2
    missing=1
  }
done
for lib in "${required_libs[@]}"; do
  # unquoted glob on purpose: patterns like libgallium-*.so* must expand
  ls $target/$lib >/dev/null 2>&1 || {
    printf 'Missing library in target: %s\n' "$lib" >&2
    missing=1
  }
done
[ "$missing" = 0 ] || exit 1

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

bundle="$tmpdir/opt/op3-weston"
mkdir -p "$bundle"

# Whole target tree. dev/proc/sys/run/tmp inside it are empty directories and
# harmless on sda15; nothing outside usr/lib/etc/sbin/bin is executed.
cp -a "$target/." "$bundle/"

install -m 0755 "$run_script" "$bundle/run.sh"

# Sanity: the loader the run.sh relies on.
ls "$bundle"/lib/ld-linux-*.so* >/dev/null 2>&1 ||
  ls "$bundle"/usr/lib/ld-linux-*.so* >/dev/null 2>&1 || {
  printf 'Missing dynamic loader ld-linux-*.so\n' >&2
  exit 1
}

tar -czf "$output" -C "$tmpdir" opt/op3-weston

printf 'bundle top-level:\n'
tar -tzf "$output" | cut -d/ -f1-3 | LC_ALL=C sort -u | head -n 30
printf 'output=%s\n' "$output"
ls -lh "$output"
sha256sum "$output"
printf '\nDeploy on the device (sda15 is /newroot):\n'
printf '  mkdir -p /newroot/opt && tar -xzf op3-weston-bundle.tar.gz -C /newroot\n'
