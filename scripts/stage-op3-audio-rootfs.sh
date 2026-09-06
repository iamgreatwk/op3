#!/usr/bin/env bash
set -euo pipefail

# Stage the Buildroot target as the persistent OP3 audio payload. The target
# must already have been built by the project owner with TinyALSA tools. This
# script does not invoke Buildroot and does not create a boot image.
#
# Usage:
#   scripts/stage-op3-audio-rootfs.sh [buildroot-target] [output-tarball]

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
target="${1:-$project_root/out/buildroot-op3-egl/target}"
output="${2:-$project_root/artifacts/op3-audio-rootfs.tar.gz}"
route="$project_root/boot/audio-test/opt/op3-audio/route.sh"

for input in "$target" "$route"; do
	test -e "$input" || { printf 'Missing input: %s\n' "$input" >&2; exit 1; }
done
test ! -e "$output" || { printf 'Refusing to overwrite: %s\n' "$output" >&2; exit 1; }

for program in tinycap tinymix tinyplay; do
	test -x "$target/usr/bin/$program" || {
		printf 'Missing %s; enable BR2_PACKAGE_TINYALSA and BR2_PACKAGE_TINYALSA_TOOLS.\n' \
			"$target/usr/bin/$program" >&2
		exit 1
	}
done
ls "$target"/usr/lib/libtinyalsa.so.* >/dev/null 2>&1 || {
	printf 'Missing libtinyalsa in %s/usr/lib\n' "$target" >&2
	exit 1
}

tmpdir="$(mktemp -d "${TMPDIR:-/tmp}/op3-audio-rootfs.XXXXXX")"
trap 'rm -rf "$tmpdir"' EXIT
stage="$tmpdir/root"
mkdir -p "$stage"
cp -a "$target/." "$stage/"
install -D -m 0755 "$route" "$stage/opt/op3-audio/route.sh"

mkdir -p "$(dirname "$output")"
epoch="${SOURCE_DATE_EPOCH:-0}"
tar --sort=name --mtime="@$epoch" --owner=0 --group=0 --numeric-owner \
	-czf "$output" -C "$stage" .

printf 'output=%s\n' "$output"
tar -tzf "$output" | grep -E '(^\./)?(usr/bin/(tinycap|tinymix|tinyplay)|opt/op3-audio/route.sh)$'
sha256sum "$output"
printf '\nDeploy as the persistent /newroot payload:\n'
printf '  tar -xzf op3-audio-rootfs.tar.gz -C /newroot\n'
