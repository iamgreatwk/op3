#!/usr/bin/env bash
set -euo pipefail

# Compatibility tool for older images: append a separately staged recovery
# bundle to the validated recovery initramfs. The canonical recovery binary
# and helpers now come from the op3-recovery Buildroot package and live in the
# persistent target; new builds should use make-recovery-browser-initrd.sh.
#
# Usage:
#   scripts/make-recovery-audio-initrd.sh [reference-initrd] [audio-bundle] [output]

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
reference="${1:-$project_root/artifacts/initrd-op3-recovery-browser.cpio.gz}"
bundle="${2:-$project_root/artifacts/op3-recovery-browser-audio-bundle.tar.gz}"
output="${3:-$project_root/artifacts/initrd-op3-recovery-browser-audio.cpio.gz}"

for command in cpio find gzip sha256sum stat tar; do
	command -v "$command" >/dev/null || {
		printf 'Missing required command: %s\n' "$command" >&2
		exit 1
	}
done

for input in "$reference" "$bundle"; do
	test -f "$input" || { printf 'Missing input: %s\n' "$input" >&2; exit 1; }
done
test ! -e "$output" || { printf 'Refusing to overwrite: %s\n' "$output" >&2; exit 1; }

tmpdir="$(mktemp -d "${TMPDIR:-/tmp}/op3-recovery-audio-initrd.XXXXXX")"
trap 'rm -rf "$tmpdir"' EXIT
stage="$tmpdir/stage"
mkdir -p "$stage"

tar --extract --gzip --file "$bundle" --directory "$stage" \
	--no-same-owner --no-same-permissions

for required in \
	sbin/recovery_mainline \
	usr/bin/browser \
	usr/bin/op3-browser-session \
	opt/op3-recovery/chromium-run.sh \
	opt/op3-recovery/cog-run.sh; do
	test -f "$stage/$required" || {
		printf 'Bundle is missing required path: %s\n' "$required" >&2
		exit 1
	}
done

( cd "$stage" && find . -mindepth 1 -printf '%P\n' | LC_ALL=C sort |
	cpio -o -H newc --owner=0:0 --reproducible --quiet ) > "$tmpdir/overlay.cpio"
gzip -9 -n -c "$tmpdir/overlay.cpio" > "$tmpdir/overlay.cpio.gz"

mkdir -p "$(dirname "$output")"
cat "$reference" "$tmpdir/overlay.cpio.gz" > "$output"

printf 'reference=%s\n' "$reference"
printf 'bundle=%s\n' "$bundle"
printf 'appended entries:\n'
cpio -t < "$tmpdir/overlay.cpio"
printf 'output=%s\n' "$output"
stat -c 'size=%s bytes' "$output"
sha256sum "$output"
