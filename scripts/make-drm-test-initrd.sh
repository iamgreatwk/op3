#!/usr/bin/env bash
set -euo pipefail

# Build the derived initramfs for the OnePlus 3 direct DRM dumb-buffer test.
#
# It never repacks the reference initramfs. It appends one extra gzip cpio
# member that overrides sbin/run_recovery.sh and adds usr/bin/op3-drm-dumb.
# The kernel unpacks concatenated compressed initramfs members in order
# (init/initramfs.c, unpack_to_rootfs), and a regular-file entry is opened with
# O_WRONLY|O_CREAT|O_TRUNC, so the appended launcher replaces the reference one
# while every other file, owner and device node stays byte-identical.
#
# Usage:
#   scripts/make-drm-test-initrd.sh [reference-initrd] [test-binary] [output]

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

reference="${1:-$project_root/artifacts/reference-initrd.img}"
binary="${2:-$project_root/artifacts/op3-drm-dumb}"
output="${3:-$project_root/artifacts/initrd-op3-drm-test.cpio.gz}"
overlay_source="$project_root/boot/drm-test-initramfs"

for input in "$reference" "$binary" "$overlay_source/sbin/run_recovery.sh"; do
  test -f "$input" || { printf 'Missing input: %s\n' "$input" >&2; exit 1; }
done
command -v cpio >/dev/null
command -v gzip >/dev/null

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

stage="$tmpdir/stage"
firmware_stage="$project_root/artifacts/a530-firmware"

mkdir -p "$stage/sbin" "$stage/usr/bin"
install -m 0755 "$overlay_source/sbin/run_recovery.sh" "$stage/sbin/run_recovery.sh"
install -m 0755 "$binary" "$stage/usr/bin/op3-drm-dumb"

# The GPU probes while the initramfs is still the root, so its firmware has to
# be inside the initramfs. About 35 KB; irrelevant to the boot.img size limit.
if [ -d "$firmware_stage" ]; then
  mkdir -p "$stage/lib/firmware/qcom"
  cp -a "$firmware_stage/." "$stage/"
  printf 'GPU firmware staged from %s:\n' "$firmware_stage"
  ( cd "$firmware_stage" && find . -type f -printf '  %P\n' | LC_ALL=C sort )
fi

( cd "$stage" && find . -mindepth 1 -printf '%P\n' | LC_ALL=C sort |
	cpio -o -H newc --owner=0:0 --quiet ) > "$tmpdir/overlay.cpio"
gzip -9 -n -c "$tmpdir/overlay.cpio" > "$tmpdir/overlay.cpio.gz"

cat "$reference" "$tmpdir/overlay.cpio.gz" > "$output"

printf 'reference=%s\n' "$reference"
printf 'appended entries:\n'
cpio -t < "$tmpdir/overlay.cpio"
printf 'output=%s\n' "$output"
ls -l "$output"
sha256sum "$output"
