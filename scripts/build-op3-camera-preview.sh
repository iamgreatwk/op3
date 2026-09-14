#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
output="${1:-$project_root/out/recovery/op3-v4l2-live-preview}"

if [ -n "${CC:-}" ]; then
	cc="$CC"
elif command -v aarch64-linux-gnu-gcc-11 >/dev/null 2>&1; then
	cc=aarch64-linux-gnu-gcc-11
else
	cc=aarch64-linux-gnu-gcc
fi
command -v "$cc" >/dev/null 2>&1 || {
	printf 'Missing aarch64 compiler: %s\n' "$cc" >&2
	exit 1
}

mkdir -p "$(dirname "$output")"
"$cc" -static -O2 -pipe -pthread -Wno-unused-result \
	-I"$project_root/third_party/libtsm/src/tsm" \
	-I"$project_root/third_party/libtsm/src/shared" \
	-I"$project_root/third_party/libtsm/external" \
	-I"$project_root/recovery" \
	-o "$output" \
	"$project_root/scripts/op3-v4l2-live-preview.c" \
	"$project_root/recovery/recovery_drm.c"
chmod 0755 "$output"
file "$output"
sha256sum "$output"

