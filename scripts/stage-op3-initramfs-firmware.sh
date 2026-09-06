#!/usr/bin/env bash
set -euo pipefail

# Merge the verified firmware trees needed while the Buildroot-generated
# initramfs is already running as /. This script stages no kernel modules and
# never creates a boot image.
#
# Inputs:
#   artifacts/a530-firmware
#   artifacts/msm8996-oneplus3-firmware-verified
#   external ath10k firmware-6.bin and board-2.bin
#
# Usage:
#   scripts/stage-op3-initramfs-firmware.sh [output-dir]

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=/dev/null
source "$project_root/manifests/op3-recovery-audio-full.env"

a530_root="${OP3_A530_FIRMWARE_ROOT:-$project_root/artifacts/a530-firmware}"
qcom_root="${OP3_MSM8996_FIRMWARE_ROOT:-$project_root/artifacts/msm8996-oneplus3-firmware-verified}"
external_inputs="${OP3_EXTERNAL_INPUTS:-}"
ath10k_source="${OP3_ATH10K_EXTFW_SOURCE:-}"
destination="${1:-$project_root/artifacts/op3-initramfs-firmware}"

die() {
	printf 'OP3 initramfs firmware staging failed: %s\n' "$*" >&2
	exit 1
}

test -d "$a530_root/lib/firmware/qcom" || die "missing A530 firmware tree: $a530_root"
test -d "$qcom_root/lib/firmware/qcom/msm8996/oneplus3" || \
	die "missing MSM8996 firmware tree: $qcom_root"
test ! -e "$destination" || die "refusing to overwrite: $destination"

if [ -z "$ath10k_source" ] && [ -n "$external_inputs" ]; then
	ath10k_source="$external_inputs/ath10k"
fi
[ -n "$ath10k_source" ] || die 'set OP3_EXTERNAL_INPUTS or OP3_ATH10K_EXTFW_SOURCE'
ath10k_source="$(readlink -f "$ath10k_source")"

ath10k_rel=ath10k/QCA6174/hw3.0
if [ -f "$ath10k_source/$ath10k_rel/firmware-6.bin" ]; then
	ath10k_dir="$ath10k_source/$ath10k_rel"
elif [ -f "$ath10k_source/ath10k/$ath10k_rel/firmware-6.bin" ]; then
	ath10k_dir="$ath10k_source/ath10k/$ath10k_rel"
else
	die "missing ath10k firmware under $ath10k_source"
fi

for input in \
	"$a530_root/lib/firmware/qcom/a530_pm4.fw" \
	"$a530_root/lib/firmware/qcom/a530_pfp.fw" \
	"$a530_root/lib/firmware/qcom/a530v3_gpmu.fw2" \
	"$qcom_root/lib/firmware/qcom/msm8996/oneplus3/a530_zap.mbn" \
	"$ath10k_dir/firmware-6.bin" \
	"$ath10k_dir/board-2.bin"; do
	test -f "$input" || die "missing firmware input: $input"
done

[ "$(sha256sum "$a530_root/lib/firmware/qcom/a530_pm4.fw" | awk '{print $1}')" = "$A530_PM4_SHA256" ] ||
	die 'A530 PM4 checksum mismatch'
[ "$(sha256sum "$a530_root/lib/firmware/qcom/a530_pfp.fw" | awk '{print $1}')" = "$A530_PFP_SHA256" ] ||
	die 'A530 PFP checksum mismatch'
[ "$(sha256sum "$a530_root/lib/firmware/qcom/a530v3_gpmu.fw2" | awk '{print $1}')" = "$A530_GPMU_SHA256" ] ||
	die 'A530 GPMU checksum mismatch'
[ "$(sha256sum "$ath10k_dir/firmware-6.bin" | awk '{print $1}')" = "$ATH10K_FIRMWARE_SHA256" ] ||
	die 'ath10k firmware-6 checksum mismatch'
[ "$(sha256sum "$ath10k_dir/board-2.bin" | awk '{print $1}')" = "$ATH10K_BOARD_SHA256" ] ||
	die 'ath10k board-2 checksum mismatch'

tmpdir="$(mktemp -d "${TMPDIR:-/tmp}/op3-initramfs-fw.XXXXXX")"
trap 'rm -rf "$tmpdir"' EXIT
stage="$tmpdir/root"
mkdir -p "$stage/lib/firmware/qcom" "$stage/lib/firmware/ath10k/QCA6174/hw3.0"
cp -a "$a530_root/lib/firmware/qcom/." "$stage/lib/firmware/qcom/"
cp -a "$qcom_root/lib/firmware/qcom/msm8996" \
	"$stage/lib/firmware/qcom/"
install -m 0644 "$ath10k_dir/firmware-6.bin" \
	"$stage/lib/firmware/$ath10k_rel/firmware-6.bin"
install -m 0644 "$ath10k_dir/board-2.bin" \
	"$stage/lib/firmware/$ath10k_rel/board-2.bin"

mkdir -p "$(dirname "$destination")"
mv "$stage" "$destination"

printf 'staged initramfs firmware: %s\n' "$destination"
find "$destination" -type f -printf '%P\n' | LC_ALL=C sort | while IFS= read -r file; do
	sha256sum "$destination/$file"
done
