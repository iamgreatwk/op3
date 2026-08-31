#!/usr/bin/env bash
set -euo pipefail

# Create a minimal OP3-AUDIO-MIC-001 diagnostic initramfs.
#
# This appends a gzip-compressed cpio member to the validated firmware-
# provenance initramfs.  It contains only the audio route helper and launcher:
# tinycap/tinymix/tinyplay plus their runtime loader are already verified in
# the base archive.  No sda15/rootfs content is changed.

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
reference="${1:-$project_root/artifacts/initrd-op3-firmware-provenance-v2.cpio.gz}"
output="${2:-$project_root/artifacts/initrd-op3-audio-diagnostic.cpio.gz}"
overlay_source="$project_root/boot/audio-test"

if [ "$#" -gt 2 ]; then
	printf 'Usage: %s [reference-initrd] [output-initrd]\n' "$0" >&2
	exit 2
fi

for input in "$reference" "$overlay_source/sbin/run_recovery.sh" \
	"$overlay_source/opt/op3-audio/route.sh"; do
	test -f "$input" || { printf 'Missing input: %s\n' "$input" >&2; exit 1; }
done
test ! -e "$output" || { printf 'Refusing to overwrite output: %s\n' "$output" >&2; exit 1; }
command -v cpio >/dev/null
command -v gzip >/dev/null

for required in usr/bin/tinycap usr/bin/tinymix usr/bin/tinyplay \
	lib/ld-linux-aarch64.so.1 lib/libc.so.6 usr/lib/libtinyalsa.so.2; do
	if ! gzip -dc "$reference" | cpio -it --quiet | grep -Fxq "$required"; then
		printf 'Reference initramfs lacks required runtime entry: %s\n' "$required" >&2
		exit 1
	fi
done

workdir="$(mktemp -d "${TMPDIR:-/tmp}/op3-audio-initrd.XXXXXX")"
trap 'rm -rf "$workdir"' EXIT
stage="$workdir/stage"
mkdir -p "$stage/sbin" "$stage/opt/op3-audio"
install -m 0755 "$overlay_source/sbin/run_recovery.sh" "$stage/sbin/run_recovery.sh"
install -m 0755 "$overlay_source/opt/op3-audio/route.sh" "$stage/opt/op3-audio/route.sh"

( cd "$stage" && find . -mindepth 1 -printf '%P\n' | LC_ALL=C sort |
	cpio -o -H newc --owner=0:0 --quiet ) > "$workdir/overlay.cpio"
gzip -9 -n -c "$workdir/overlay.cpio" > "$workdir/overlay.cpio.gz"
cat "$reference" "$workdir/overlay.cpio.gz" > "$output"

gzip -t "$output"
printf 'reference=%s\n' "$reference"
printf 'appended entries:\n'
cpio -t < "$workdir/overlay.cpio"
printf 'output=%s\n' "$output"
sha256sum "$output"
