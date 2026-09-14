#!/usr/bin/env bash
set -euo pipefail

# Stage the OP3 vendor Sensor Manager registry into the initramfs firmware
# tree. The registry is a proprietary binary input recovered from the phone's
# persist partition; it must remain outside GitHub. This script is separate
# from the default recovery firmware staging so the accepted recovery build
# does not acquire an untested sensor input implicitly.
#
# Usage:
#   scripts/stage-op3-sensor-registry.sh [external-input-root] [firmware-root]

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=/dev/null
source "$project_root/manifests/op3-sensor-smgr.env"

external_root="${1:-${OP3_EXTERNAL_INPUTS:-}}"
destination_root="${2:-$project_root/artifacts/op3-initramfs-firmware}"

die() {
	printf 'OP3 sensor registry staging failed: %s\n' "$*" >&2
	exit 1
}

[ -n "$external_root" ] || die 'set OP3_EXTERNAL_INPUTS or pass the external-input root'
external_root="$(readlink -f "$external_root")"
destination_root="$(readlink -m "$destination_root")"
source_file="$external_root/$SENSOR_EXTERNAL_REL"
destination_file="$destination_root/$SENSOR_FIRMWARE_REL"

test -f "$source_file" || die "missing external registry: $source_file"
actual="$(sha256sum "$source_file" | awk '{print $1}')"
[ "$actual" = "$SENSOR_REGISTRY_SHA256" ] ||
	die "registry SHA256 mismatch: expected $SENSOR_REGISTRY_SHA256, got $actual"

if [ -e "$destination_file" ]; then
	test -f "$destination_file" || die "destination is not a regular file: $destination_file"
	installed="$(sha256sum "$destination_file" | awk '{print $1}')"
	[ "$installed" = "$SENSOR_REGISTRY_SHA256" ] ||
		die "refusing to replace different registry: $destination_file"
	printf 'sensor registry already staged: %s\n' "$destination_file"
	exit 0
fi

mkdir -p "$(dirname "$destination_file")"
tmp="$(mktemp "$(dirname "$destination_file")/.sns.reg.XXXXXX")"
trap 'rm -f "$tmp"' EXIT
install -m 0644 "$source_file" "$tmp"
mv "$tmp" "$destination_file"
trap - EXIT

printf 'staged sensor registry: %s\n' "$destination_file"
sha256sum "$destination_file"
