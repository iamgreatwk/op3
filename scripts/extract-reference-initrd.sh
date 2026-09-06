#!/usr/bin/env bash
set -euo pipefail

# Extract and verify the historical OP3 reference ramdisk from the known-good
# v100 boot image. The boot image is intentionally an external input; this
# script makes the dependency explicit and refuses an unknown image.
#
# Usage:
#   scripts/extract-reference-initrd.sh <boot_fa5_v100_auto.img> [output]

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
boot_image="${1:?usage: scripts/extract-reference-initrd.sh <boot-image> [output] }"
output="${2:-$project_root/artifacts/reference-initrd.img}"
expected_boot="29ccd3eb8b093b29fc44435bd6e5f98367cf3794c117f9527a6bf3c1ebc5d781"
expected_initrd="c3358a1cadb747996ddaa492e636827f2d72974040e8fd40d81f8a213e676366"

command -v abootimg >/dev/null 2>&1 || { printf 'Missing abootimg\n' >&2; exit 1; }
command -v gzip >/dev/null 2>&1 || { printf 'Missing gzip\n' >&2; exit 1; }
command -v sha256sum >/dev/null 2>&1 || { printf 'Missing sha256sum\n' >&2; exit 1; }
test -f "$boot_image" || { printf 'Missing boot image: %s\n' "$boot_image" >&2; exit 1; }
test ! -e "$output" || { printf 'Refusing to overwrite: %s\n' "$output" >&2; exit 1; }

actual_boot="$(sha256sum "$boot_image" | awk '{print $1}')"
[ "$actual_boot" = "$expected_boot" ] || {
	printf 'Boot image SHA256 mismatch: expected %s, got %s\n' "$expected_boot" "$actual_boot" >&2
	exit 1
}

tmpdir="$(mktemp -d "${TMPDIR:-/tmp}/op3-reference-initrd.XXXXXX")"
trap 'rm -rf "$tmpdir"' EXIT
abootimg -x "$boot_image" "$tmpdir/bootimg.cfg" "$tmpdir/kernel" "$tmpdir/ramdisk"
gzip -t "$tmpdir/ramdisk"
actual_initrd="$(sha256sum "$tmpdir/ramdisk" | awk '{print $1}')"
[ "$actual_initrd" = "$expected_initrd" ] || {
	printf 'Extracted ramdisk SHA256 mismatch: expected %s, got %s\n' \
		"$expected_initrd" "$actual_initrd" >&2
	exit 1
}

mkdir -p "$(dirname "$output")"
install -m 0644 "$tmpdir/ramdisk" "$output"
printf 'boot_image=%s\n' "$boot_image"
printf 'boot_sha256=%s\n' "$actual_boot"
printf 'output=%s\n' "$output"
printf 'output_sha256=%s\n' "$(sha256sum "$output" | awk '{print $1}')"
