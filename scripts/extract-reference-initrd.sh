#!/usr/bin/env bash
set -euo pipefail

# Verify and copy the standalone historical OP3 reference ramdisk. The v100
# boot image was used once to create this file, but is not a rebuild input.
# Keeping the ramdisk byte-preserved avoids depending on abootimg or retaining
# the unrelated v100 kernel, DTB, and boot header.
#
# Usage:
#   scripts/extract-reference-initrd.sh <reference-initrd.img> [output]

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=/dev/null
source "$project_root/manifests/op3-recovery-audio-full.env"

if [ "$#" -ge 1 ]; then
	reference_initrd="$1"
elif [ -n "${OP3_EXTERNAL_INPUTS:-}" ]; then
	reference_initrd="$OP3_EXTERNAL_INPUTS/initrd/reference-initrd.img"
else
	printf 'usage: scripts/extract-reference-initrd.sh <reference-initrd.img> [output]\n' >&2
	exit 2
fi
output="${2:-$project_root/artifacts/reference-initrd.img}"
expected_initrd="$REFERENCE_INITRD_SHA256"

command -v gzip >/dev/null 2>&1 || { printf 'Missing gzip\n' >&2; exit 1; }
command -v sha256sum >/dev/null 2>&1 || { printf 'Missing sha256sum\n' >&2; exit 1; }
test -f "$reference_initrd" || { printf 'Missing reference initrd: %s\n' "$reference_initrd" >&2; exit 1; }
test ! -e "$output" || { printf 'Refusing to overwrite: %s\n' "$output" >&2; exit 1; }

gzip -t "$reference_initrd"
actual_initrd="$(sha256sum "$reference_initrd" | awk '{print $1}')"
[ "$actual_initrd" = "$expected_initrd" ] || {
	printf 'Reference initrd SHA256 mismatch: expected %s, got %s\n' \
		"$expected_initrd" "$actual_initrd" >&2
	exit 1
}

mkdir -p "$(dirname "$output")"
install -m 0644 "$reference_initrd" "$output"
printf 'reference_initrd=%s\n' "$reference_initrd"
printf 'reference_initrd_sha256=%s\n' "$actual_initrd"
printf 'output=%s\n' "$output"
printf 'output_sha256=%s\n' "$(sha256sum "$output" | awk '{print $1}')"
