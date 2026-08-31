#!/usr/bin/env bash
set -euo pipefail

# Apply the Wi-Fi initramfs overlay to an already validated base archive.  The
# sole archive change is usr/bin/wifi_auto.sh; modules and credentials remain
# on sda15 so they never consume boot.img space or enter version control.

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
reference="${1:-$project_root/artifacts/initrd-op3-firmware-provenance-v2.cpio.gz}"
output="${2:-$project_root/artifacts/initrd-op3-wifi.cpio.gz}"
overlay="$project_root/boot/wifi/initramfs"

for command in cpio fakeroot find gzip sha256sum sort; do
	command -v "$command" >/dev/null || { printf 'Missing required command: %s\n' "$command" >&2; exit 1; }
done
for input in "$reference" "$overlay/usr/bin/wifi_auto.sh"; do
	test -e "$input" || { printf 'Missing input: %s\n' "$input" >&2; exit 1; }
done
test ! -e "$output" || { printf 'Refusing to overwrite output: %s\n' "$output" >&2; exit 1; }

if [ -z "${FAKEROOTKEY:-}" ]; then
	exec fakeroot -- "$0" "$reference" "$output"
fi

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT
stage="$tmpdir/rootfs"
mkdir "$stage"

gzip -t "$reference"
gzip -cd "$reference" | (cd "$stage" && cpio -idm --no-absolute-filenames --quiet)
install -m 0755 "$overlay/usr/bin/wifi_auto.sh" "$stage/usr/bin/wifi_auto.sh"

(cd "$stage" && LC_ALL=C find . -print0 | LC_ALL=C sort -z |
	cpio --null -o -H newc --reproducible --quiet) | gzip -9 -n > "$output"
gzip -t "$output"

printf 'reference_sha256 %s\n' "$(sha256sum "$reference" | awk '{print $1}')"
printf 'output_sha256 %s\n' "$(sha256sum "$output" | awk '{print $1}')"
printf 'output=%s\n' "$output"
