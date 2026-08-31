#!/usr/bin/env bash
set -euo pipefail

# Collect the Buildroot-built browser stack (cog + WPE WebKit + weston) into a
# tarball that unpacks to /opt/op3-browser on the device's sda15 root
# filesystem. Whole-tree staging, same rationale as the weston bundle:
# WebKit/weston locate helpers, modules and data files through
# compile-time-absolute paths that a NEEDED sweep cannot cover.
#
# Usage:
#   scripts/stage-browser-rootfs.sh [buildroot-target] [output-tarball]

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
target="${1:-$project_root/out/buildroot-op3-egl/target}"
output="${2:-$project_root/artifacts/op3-browser-bundle.tar.gz}"
run_script="$project_root/boot/browser-test/opt/op3-browser/run.sh"
page="$project_root/boot/browser-test/opt/op3-browser/test-page.html"

required_bins=(weston cog udevadm)
# globs are intentional; see stage-weston-rootfs.sh for the same pattern
required_libs=(usr/lib/libweston-14/drm-backend.so
               usr/lib/libweston-14/gl-renderer.so
               sbin/udevd
               usr/lib/gbm/dri_gbm.so
               usr/lib/libEGL.so.1
               usr/lib/libWPEWebKit*.so*
               usr/lib/libWPEBackend-fdo*.so*
               usr/lib/cog/modules/libcogplatform-wl.so
               usr/libexec/wpe-webkit-2.0/WPEWebProcess
               usr/libexec/wpe-webkit-2.0/WPENetworkProcess
               usr/share/fonts/dejavu
               usr/lib/gio/modules/libgio*.so
               etc/ssl/certs/ca-certificates.crt)

for input in "$target" "$run_script" "$page"; do
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
  ls $target/$lib >/dev/null 2>&1 || {
    printf 'Missing library in target: %s\n' "$lib" >&2
    missing=1
  }
done
[ "$missing" = 0 ] || exit 1

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

bundle="$tmpdir/opt/op3-browser"
mkdir -p "$bundle"

cp -a "$target/." "$bundle/"

install -m 0755 "$run_script" "$bundle/run.sh"
install -m 0644 "$page" "$bundle/test-page.html"

ls "$bundle"/lib/ld-linux-*.so* >/dev/null 2>&1 ||
  ls "$bundle"/usr/lib/ld-linux-*.so* >/dev/null 2>&1 || {
  printf 'Missing dynamic loader ld-linux-*.so\n' >&2
  exit 1
}

tar -czf "$output" -C "$tmpdir" opt/op3-browser

printf 'output=%s\n' "$output"
ls -lh "$output"
sha256sum "$output"
printf '\nDeploy on the device (sda15 is /newroot):\n'
printf '  mkdir -p /newroot/opt && tar -xzf op3-browser-bundle.tar.gz -C /newroot\n'
