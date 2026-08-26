#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
profile="${BOOT_PROFILE:-$project_root/boot/oneplus3-fa5.env}"

usage() {
  cat <<'EOF'
Usage:
  scripts/pack-boot.sh <Image.gz> <msm8996-oneplus3.dtb> <initrd.img> <boot.img>

Optional environment:
  BOOT_PROFILE=/path/to/profile.env
  BOOT_CMDLINE_OVERRIDE='complete kernel command line'
EOF
}

if [ "$#" -ne 4 ]; then
  usage >&2
  exit 2
fi

# shellcheck source=/dev/null
source "$profile"

kernel="$1"
dtb="$2"
ramdisk="$3"
output="$4"
for input in "$kernel" "$dtb" "$ramdisk"; do
  test -f "$input" || { printf 'Missing input: %s\n' "$input" >&2; exit 1; }
done
command -v mkbootimg >/dev/null
command -v abootimg >/dev/null

mkdir -p "$(dirname "$output")"
tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

kernel_payload="$kernel"
if [ "${BOOT_APPEND_DTB}" = 1 ]; then
  kernel_payload="$tmpdir/Image.gz-dtb"
  cat "$kernel" "$dtb" > "$kernel_payload"
fi

cmdline="${BOOT_CMDLINE_OVERRIDE:-$BOOT_CMDLINE}"
printf 'profile=%s\n' "$profile"
printf 'kernel=%s\n' "$kernel"
printf 'dtb=%s\n' "$dtb"
printf 'ramdisk=%s\n' "$ramdisk"
printf 'header_version=%s page_size=%s\n' "$BOOT_HEADER_VERSION" "$BOOT_PAGE_SIZE"
printf 'cmdline=%s\n' "$cmdline"

mkbootimg \
  --header_version "$BOOT_HEADER_VERSION" \
  --pagesize "$BOOT_PAGE_SIZE" \
  --base "$BOOT_BASE" \
  --kernel_offset "$BOOT_KERNEL_OFFSET" \
  --ramdisk_offset "$BOOT_RAMDISK_OFFSET" \
  --second_offset "$BOOT_SECOND_OFFSET" \
  --tags_offset "$BOOT_TAGS_OFFSET" \
  --kernel "$kernel_payload" \
  --ramdisk "$ramdisk" \
  --cmdline "$cmdline" \
  --id \
  --output "$output"

printf '\n-- packed image --\n'
abootimg -i "$output"
sha256sum "$output"
