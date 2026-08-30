#!/usr/bin/env bash
set -euo pipefail

# Collect the Buildroot-built EGL stack into a tarball that unpacks to
# /opt/op3-egl on the device's sda15 root filesystem.
#
# The boot image stays minimal: this tarball is deployed to sda15, never packed
# into the initramfs (docs/decisions.md, OnePlus 3 boot.img size limit).
#
# The bundle is self-contained: it also carries the glibc dynamic loader and
# every shared-library dependency found in the Buildroot target, so it does not
# depend on the libc of whatever rootfs sda15 happens to hold. run.sh invokes
# the test through that loader explicitly.
#
# Mesa 26 layout notes:
#   - libglapi no longer exists (merged away).
#   - there is no per-driver msm_dri.so any more: all Gallium drivers,
#     freedreno included, live in libgallium-<version>.so.
#   - libgbm's DRI backend is lib/gbm/dri_gbm.so.
#
# Usage:
#   scripts/stage-egl-rootfs.sh [buildroot-target] [output-tarball]

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
target="${1:-$project_root/out/buildroot-op3-egl/target}"
output="${2:-$project_root/artifacts/op3-egl-bundle.tar.gz}"
run_script="$project_root/boot/egl-test/opt/op3-egl/run.sh"

required_libs=(libEGL libGLESv2 libgbm libdrm)
optional_libs=(libGLESv1_CM libexpat libz libzstd)

for input in "$target" "$run_script"; do
  test -e "$input" || { printf 'Missing input: %s\n' "$input" >&2; exit 1; }
done
command -v readelf >/dev/null || {
  printf 'readelf is required: sudo apt install binutils\n' >&2
  exit 1
}

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

bundle="$tmpdir/opt/op3-egl"
mkdir -p "$bundle/bin" "$bundle/lib/gbm"

copy_glob() { # $1 = glob, $2 = destination directory
  local matches=($(ls $1 2>/dev/null || true))
  if [ "${#matches[@]}" -eq 0 ]; then
    return 1
  fi
  cp -a "${matches[@]}" "$2"
}

for lib in "${required_libs[@]}"; do
  copy_glob "$target/usr/lib/$lib.so*" "$bundle/lib" || {
    printf 'Missing library in %s: %s\n' "$target/usr/lib" "$lib" >&2
    exit 1
  }
done

for lib in "${optional_libs[@]}"; do
  copy_glob "$target/usr/lib/$lib.so*" "$bundle/lib" || true
done

# Mesa >= 26: one library holds every Gallium driver. Without it there is no
# freedreno support at all.
copy_glob "$target/usr/lib/libgallium-*.so*" "$bundle/lib" || {
  printf 'Missing libgallium-*.so: the Gallium drivers were not built\n' >&2
  exit 1
}

copy_glob "$target/usr/lib/gbm/dri_gbm.so" "$bundle/lib/gbm" || {
  printf 'Missing gbm/dri_gbm.so\n' >&2
  exit 1
}

if [ -f "$target/usr/bin/kmscube" ]; then
  cp -a "$target/usr/bin/kmscube" "$bundle/bin/kmscube"
else
  printf 'Missing %s: enable BR2_PACKAGE_KMSCUBE\n' "$target/usr/bin/kmscube" >&2
  exit 1
fi

install -m 0755 "$run_script" "$bundle/run.sh"

# Pull in every shared-library dependency that exists in the Buildroot target,
# libc included, until the bundle is closed under its own NEEDED lists.
# LC_ALL=C: readelf output is localised and this host prints 共享库, not
# "Shared library". cp -L: target entries such as libc.so.6 are symlinks, and
# the bundle must contain the real file, not a dangling link.
echo "library dependency sweep:"
unresolved=1
while [ "$unresolved" = 1 ]; do
  unresolved=0
  while IFS= read -r so; do
    while IFS= read -r needed; do
      [ -e "$bundle/lib/$needed" ] && continue
      src=""
      [ -f "$target/usr/lib/$needed" ] && src="$target/usr/lib/$needed"
      [ -z "$src" ] && [ -f "$target/lib/$needed" ] && src="$target/lib/$needed"
      if [ -n "$src" ]; then
        cp -L "$src" "$bundle/lib/"
        printf '  + %s\n' "$needed"
        unresolved=1
      fi
    done < <(LC_ALL=C readelf -d "$so" 2>/dev/null |
             sed -n 's/.*Shared library: \[\([^]]*\)\].*/\1/p')
  done < <(find "$bundle/lib" -maxdepth 1 -type f)
done

# The glibc dynamic loader, for the explicit-invocation form used by run.sh.
copy_glob "$target/lib/ld-linux-*.so*" "$bundle/lib" ||
  copy_glob "$target/usr/lib/ld-linux-*.so*" "$bundle/lib" || {
  printf 'Missing dynamic loader ld-linux-*.so\n' >&2
  exit 1
}

tar -czf "$output" -C "$tmpdir" opt/op3-egl

printf 'bundle contents:\n'
tar -tzf "$output" | LC_ALL=C sort
printf 'output=%s\n' "$output"
ls -l "$output"
sha256sum "$output"
printf '\nDeploy on the device (sda15 is /newroot):\n'
printf '  mkdir -p /newroot/opt && tar -xzf op3-egl-bundle.tar.gz -C /newroot\n'
