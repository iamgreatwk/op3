#!/usr/bin/env bash
set -euo pipefail

# Append the A530 GPU firmware to a reference initramfs. The sole archive
# change is lib/firmware/qcom/a530*: no launcher, init or recovery replacement
# — the recovery environment keeps running untouched in the foreground, and
# the GPU probes successfully at boot instead of dying with
# "a530_pm4.fw failed with error -2".
#
# Background: msm/MDP5 cannot be rebound at runtime (unbind leaks MDP5 CTL/SMP
# resources, rebind fails with -ENOSPC), so the firmware must be available at
# probe time — i.e. inside the initramfs.
#
# Usage:
#   scripts/make-op3-a530fw-initrd.sh [reference-initrd] [output]
# Default: artifacts/v100-reference-initrd.img -> artifacts/initrd-v100-a530fw.cpio.gz

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
reference="${1:-$project_root/artifacts/v100-reference-initrd.img}"
output="${2:-$project_root/artifacts/initrd-v100-a530fw.cpio.gz}"
fw_dir="$project_root/artifacts/a530-firmware/lib/firmware/qcom"

for f in a530_pm4.fw a530_pfp.fw a530v3_gpmu.fw2; do
	test -f "$fw_dir/$f" || { printf 'Missing firmware: %s/%s\n' "$fw_dir" "$f" >&2; exit 1; }
done
test -f "$reference" || { printf 'Missing reference initramfs: %s\n' "$reference" >&2; exit 1; }
command -v cpio >/dev/null && command -v gzip >/dev/null

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

mkdir -p "$tmpdir/stage/lib/firmware/qcom"
install -m 0644 "$fw_dir/a530_pm4.fw" "$fw_dir/a530_pfp.fw" "$fw_dir/a530v3_gpmu.fw2" \
	"$tmpdir/stage/lib/firmware/qcom/"

( cd "$tmpdir/stage" && find . -mindepth 1 -printf '%P\n' | LC_ALL=C sort |
	cpio -o -H newc --owner=0:0 --quiet ) | gzip -9 -n > "$tmpdir/overlay.cpio.gz"

cat "$reference" "$tmpdir/overlay.cpio.gz" > "$output"
gzip -t "$output"

printf 'reference=%s\n' "$reference"
printf 'appended entries:\n'
( cd "$tmpdir/stage" && find . -mindepth 1 -printf '%P\n' | LC_ALL=C sort )
printf 'output=%s\n' "$output"
ls -l "$output"
sha256sum "$output"
