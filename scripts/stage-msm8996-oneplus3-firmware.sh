#!/usr/bin/env bash
set -euo pipefail

# Stage the proprietary OnePlus 3 MSM8996 firmware used by OP3-INITRD-FW-001.
# This script does not generate an initramfs or boot image. It produces only a
# firmware tree whose output hashes must exactly match the checked-in manifest.

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
manifest="$project_root/boot/base-initramfs/msm8996-oneplus3-firmware.tsv"
destination="${1:-$project_root/artifacts/msm8996-oneplus3-firmware}"
destination="$(realpath -m "$destination")"
# shellcheck source=/dev/null
source "$project_root/manifests/op3-recovery-audio-full.env"

non_hlos_url="https://gitlab.com/DrGitX/firmware-oneplus3/-/raw/master/oneplus3/NON-HLOS.bin"
non_hlos_sha512="$NON_HLOS_SHA512"
zap_url="https://github.com/TheMuppets/proprietary_vendor_oneplus/raw/lineage-17.1/oneplus3/proprietary/vendor/firmware/a530_zap.elf"
zap_sha512="$A530_ZAP_SHA512"
external_inputs="${OP3_EXTERNAL_INPUTS:-}"
mcopy_cmd="${OP3_MCOPY:-mcopy}"
pil_squasher_cmd="${OP3_PIL_SQUASHER:-pil-squasher}"

for command in "$mcopy_cmd" "$pil_squasher_cmd" sha256sum sha512sum; do
	command -v "$command" >/dev/null || {
		printf 'Missing required command: %s\n' "$command" >&2
		printf 'Install mtools and pil-squasher through the owner-approved host package workflow.\n' >&2
		exit 1
	}
done
[ -n "$external_inputs" ] || command -v curl >/dev/null || {
	printf 'Missing required command: curl\n' >&2
	exit 1
}

test -f "$manifest" || { printf 'Missing manifest: %s\n' "$manifest" >&2; exit 1; }
test ! -e "$destination" || { printf 'Refusing to overwrite: %s\n' "$destination" >&2; exit 1; }

workdir="$(mktemp -d "${TMPDIR:-/tmp}/op3-msm8996-fw.XXXXXX")"
trap 'rm -rf "$workdir"' EXIT
target="$destination/lib/firmware/qcom/msm8996/oneplus3"
mkdir -p "$target"

fetch() {
	local url="$1" output="$2" expected="$3"
	curl --fail --location --silent --show-error --output "$output" "$url"
	test "$(sha512sum "$output" | awk '{ print $1 }')" = "$expected" || {
		printf 'SHA512 mismatch for %s\n' "$url" >&2
		exit 1
	}
}

if [ -n "$external_inputs" ]; then
	install -m 0644 "$external_inputs/qualcomm/NON-HLOS.bin" "$workdir/NON-HLOS.bin"
	install -m 0644 "$external_inputs/qualcomm/a530_zap.elf" "$workdir/a530_zap.elf"
	[ "$(sha512sum "$workdir/NON-HLOS.bin" | awk '{print $1}')" = "$non_hlos_sha512" ] || {
		printf 'SHA512 mismatch for external NON-HLOS.bin\n' >&2
		exit 1
	}
	[ "$(sha512sum "$workdir/a530_zap.elf" | awk '{print $1}')" = "$zap_sha512" ] || {
		printf 'SHA512 mismatch for external a530_zap.elf\n' >&2
		exit 1
	}
else
	fetch "$non_hlos_url" "$workdir/NON-HLOS.bin" "$non_hlos_sha512"
	fetch "$zap_url" "$workdir/a530_zap.elf" "$zap_sha512"
fi

for item in adsp.b00 adsp.b01 adsp.b02 adsp.b03 adsp.b04 adsp.b05 adsp.b06 adsp.b08 adsp.b09 adsp.mdt \
	modem.b00 modem.b01 modem.b02 modem.b03 modem.b04 modem.b05 modem.b06 modem.b07 modem.b08 modem.b09 modem.b10 modem.b11 modem.b12 modem.b13 modem.b15 modem.b16 modem.b17 modem.b18 modem.b19 modem.b20 modem.mdt \
	mba.mbn \
	slpi.b00 slpi.b01 slpi.b02 slpi.b03 slpi.b04 slpi.b05 slpi.b06 slpi.b07 slpi.b08 slpi.b09 slpi.b10 slpi.b11 slpi.b12 slpi.b13 slpi.b14 slpi.mdt \
	venus.b00 venus.b01 venus.b02 venus.b03 venus.b04 venus.mdt; do
	"$mcopy_cmd" -b -p -n -i "$workdir/NON-HLOS.bin" "::image/$item" "$workdir/$item"
done

for image in adsp modem slpi venus; do
	(
		cd "$workdir"
		"$pil_squasher_cmd" "$target/$image.mbn" "$image.mdt"
	)
done
install -m 0644 "$workdir/mba.mbn" "$target/mba.mbn"
install -m 0644 "$workdir/a530_zap.elf" "$target/a530_zap.mbn"

while IFS=$'\t' read -r path size hash source early; do
	case "$path" in
		''|'#'*) continue ;;
	esac
	file="$destination/$path"
	test -f "$file"
	test "$(wc -c < "$file")" = "$size"
	test "$(sha256sum "$file" | awk '{ print $1 }')" = "$hash"
	done < "$manifest"

printf 'staged firmware tree: %s\n' "$destination"
find "$destination" -type f -printf '%P\n' | LC_ALL=C sort | while IFS= read -r file; do
	sha256sum "$destination/$file"
done
