#!/usr/bin/env bash
set -euo pipefail

# Append the Issue #6 recovery selector, validated Wi-Fi auto-start hook, and
# A530 GPU firmware to an already validated initramfs. The kernel unpacks
# concatenated gzip cpio members in order, so the inittab respawn entry,
# post-/newroot Wi-Fi hook, and early-probe firmware are overlaid; every other
# baseline entry stays byte-identical.
#
# Usage:
#   scripts/make-recovery-browser-initrd.sh [reference-initrd] [output]

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
reference="${1:-$project_root/artifacts/initrd-op3-firmware-provenance-v2.cpio.gz}"
output="${2:-$project_root/artifacts/initrd-op3-recovery-browser.cpio.gz}"
overlay_source="$project_root/boot/recovery-browser-test"
wifi_source="$project_root/boot/wifi/initramfs/usr/bin/wifi_auto.sh"
firmware_source="$project_root/artifacts/a530-firmware/lib/firmware/qcom"

for input in "$reference" "$overlay_source/sbin/run_recovery.sh" \
	"$wifi_source" \
	"$firmware_source/a530_pm4.fw" "$firmware_source/a530_pfp.fw" \
	"$firmware_source/a530v3_gpmu.fw2"; do
	test -f "$input" || { printf 'Missing input: %s\n' "$input" >&2; exit 1; }
done
command -v cpio >/dev/null 2>&1
command -v gzip >/dev/null 2>&1

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT
stage="$tmpdir/stage"
epoch="${SOURCE_DATE_EPOCH:-0}"
mkdir -p "$stage/sbin" "$stage/usr/bin" "$stage/lib/firmware/qcom"
install -m 0755 "$overlay_source/sbin/run_recovery.sh" "$stage/sbin/run_recovery.sh"
install -m 0755 "$wifi_source" "$stage/usr/bin/wifi_auto.sh"
install -m 0644 \
	"$firmware_source/a530_pm4.fw" \
	"$firmware_source/a530_pfp.fw" \
	"$firmware_source/a530v3_gpmu.fw2" \
	"$stage/lib/firmware/qcom/"
find "$stage" -exec touch -d "@$epoch" {} +

( cd "$stage" && find . -mindepth 1 -printf '%P\n' | LC_ALL=C sort |
	cpio -o -H newc --owner=0:0 --reproducible --quiet ) > "$tmpdir/overlay.cpio"
gzip -9 -n -c "$tmpdir/overlay.cpio" > "$tmpdir/overlay.cpio.gz"

mkdir -p "$(dirname "$output")"
cat "$reference" "$tmpdir/overlay.cpio.gz" > "$output"

printf 'reference=%s\n' "$reference"
printf 'appended entries:\n'
cpio -t < "$tmpdir/overlay.cpio"
printf 'output=%s\n' "$output"
ls -l "$output"
sha256sum "$output"
