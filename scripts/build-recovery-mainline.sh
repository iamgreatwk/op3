#!/usr/bin/env bash
set -euo pipefail

# Build the owner-supplied mainline recovery program as a small static
# aarch64 binary.  This is a userspace program build, not a kernel/Buildroot/
# Mesa/WebKit build; the resulting binary is staged on sda15 by the companion
# script so it does not enlarge boot.img.

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
output="${1:-$project_root/out/recovery/recovery_mainline}"

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

recovery_source="$project_root/recovery/recovery_mainline.c"
tsm="$project_root/third_party/libtsm"
sources=(
	"$recovery_source"
	"$tsm/src/tsm/tsm-render.c"
	"$tsm/src/tsm/tsm-screen.c"
	"$tsm/src/tsm/tsm-selection.c"
	"$tsm/src/tsm/tsm-unicode.c"
	"$tsm/src/tsm/tsm-vte.c"
	"$tsm/src/tsm/tsm-vte-charsets.c"
	"$tsm/src/shared/shl-htable.c"
	"$tsm/src/shared/shl-ring.c"
	"$tsm/external/wcwidth/wcwidth.c"
)

for input in "${sources[@]}" "$tsm/src/tsm/libtsm.h" "$tsm/src/shared/shl-llog.h"; do
	test -f "$input" || { printf 'Missing input: %s\n' "$input" >&2; exit 1; }
done

mkdir -p "$(dirname "$output")"
"$cc" \
	-static -O2 -pipe -Wno-unused-result \
	-I"$tsm/src/tsm" \
	-I"$tsm/src/shared" \
	-I"$tsm/external" \
	-o "$output" \
	"${sources[@]}"

chmod 0755 "$output"
file "$output"
sha256sum "$output"
