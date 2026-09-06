#!/usr/bin/env bash
set -euo pipefail

# Append the Issue #6 recovery selector to an already validated initramfs.
# The kernel unpacks concatenated gzip cpio members in order, so only the
# inittab respawn entry changes; firmware and every other baseline entry stay
# byte-identical.
#
# Usage:
#   scripts/make-recovery-browser-initrd.sh [reference-initrd] [output]

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
reference="${1:-$project_root/artifacts/initrd-op3-firmware-provenance-v2.cpio.gz}"
output="${2:-$project_root/artifacts/initrd-op3-recovery-browser.cpio.gz}"
overlay_source="$project_root/boot/recovery-browser-test"

for input in "$reference" "$overlay_source/sbin/run_recovery.sh"; do
	test -f "$input" || { printf 'Missing input: %s\n' "$input" >&2; exit 1; }
done
command -v cpio >/dev/null 2>&1
command -v gzip >/dev/null 2>&1

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT
stage="$tmpdir/stage"
mkdir -p "$stage/sbin"
install -m 0755 "$overlay_source/sbin/run_recovery.sh" "$stage/sbin/run_recovery.sh"

( cd "$stage" && find . -mindepth 1 -printf '%P\n' | LC_ALL=C sort |
	cpio -o -H newc --owner=0:0 --quiet ) > "$tmpdir/overlay.cpio"
gzip -9 -n -c "$tmpdir/overlay.cpio" > "$tmpdir/overlay.cpio.gz"

mkdir -p "$(dirname "$output")"
cat "$reference" "$tmpdir/overlay.cpio.gz" > "$output"

printf 'reference=%s\n' "$reference"
printf 'appended entries:\n'
cpio -t < "$tmpdir/overlay.cpio"
printf 'output=%s\n' "$output"
ls -l "$output"
sha256sum "$output"
