#!/usr/bin/env bash
set -euo pipefail

# Stage the exact QCA6174 controller firmware required by the OP3 Bluetooth
# UART. This script does not build a rootfs or boot image.
#
# Usage:
#   scripts/stage-op3-bluetooth-firmware.sh [external-input-root] [output-dir]

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=/dev/null
source "$project_root/manifests/op3-bluetooth.env"

external_root="${1:-${OP3_EXTERNAL_INPUTS:-}}"
destination="${2:-$project_root/artifacts/op3-bluetooth-firmware}"

die() {
	printf 'OP3 Bluetooth firmware staging failed: %s\n' "$*" >&2
	exit 1
}

[ -n "$external_root" ] || die 'set OP3_EXTERNAL_INPUTS or pass the external-input root'
external_root="$(readlink -f "$external_root")"
destination="$(readlink -m "$destination")"
[ -d "$external_root" ] || die "missing external-input root: $external_root"
[ ! -e "$destination" ] || die "refusing to overwrite: $destination"

stage_one() {
	local external_rel="$1" firmware_rel="$2" expected="$3"
	local source_file="$external_root/$external_rel"
	local destination_file="$destination/$firmware_rel"

	[ -f "$source_file" ] || die "missing external firmware: $source_file"
	actual="$(sha256sum "$source_file" | awk '{print $1}')"
	[ "$actual" = "$expected" ] ||
		die "SHA256 mismatch for $source_file: expected $expected, got $actual"
	mkdir -p "$(dirname "$destination_file")"
	install -m 0644 "$source_file" "$destination_file"
	printf '%s  %s\n' "$actual" "$destination_file"
}

stage_one "$BLUETOOTH_RAMPATCH_EXTERNAL_REL" \
	"$BLUETOOTH_RAMPATCH_FIRMWARE_REL" "$BLUETOOTH_RAMPATCH_SHA256"
stage_one "$BLUETOOTH_NVM_EXTERNAL_REL" \
	"$BLUETOOTH_NVM_FIRMWARE_REL" "$BLUETOOTH_NVM_SHA256"

printf 'staged QCA6174 Bluetooth firmware for controller %s: %s\n' \
	"$BLUETOOTH_CONTROLLER_VERSION" "$destination"
