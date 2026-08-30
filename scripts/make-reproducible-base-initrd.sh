#!/usr/bin/env bash
set -euo pipefail

# Re-serialize the pinned OP3 reference initramfs deterministically.
#
# This is an initramfs-only control: it neither reads a kernel/DTB nor packs a
# boot image. fakeroot preserves the reference archive's special /dev/console
# entry and ownership without requiring a host-root build.

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
reference="${1:-$project_root/artifacts/reference-initrd.img}"
output="${2:-$project_root/artifacts/initrd-op3-reproducible-base.cpio.gz}"
expected_file="$project_root/boot/base-initramfs/reference-initrd.sha256"

usage() {
	printf '%s\n' "Usage: $0 [reference-initrd.img] [output-initrd.cpio.gz]" >&2
}

if [ "$#" -gt 2 ]; then
	usage
	exit 2
fi

for command in awk cmp cpio fakeroot find gzip sha256sum sort wc xargs; do
	command -v "$command" >/dev/null || {
		printf 'Missing required command: %s\n' "$command" >&2
		exit 1
	}
done

test -f "$reference" || { printf 'Missing reference: %s\n' "$reference" >&2; exit 1; }
test -f "$expected_file" || { printf 'Missing checksum manifest: %s\n' "$expected_file" >&2; exit 1; }
test ! -e "$output" || { printf 'Refusing to overwrite output: %s\n' "$output" >&2; exit 1; }
test ! -e "$output.manifest" || { printf 'Refusing to overwrite manifest: %s\n' "$output.manifest" >&2; exit 1; }

expected_hash="$(awk 'NR == 1 { print $1 }' "$expected_file")"
actual_hash="$(sha256sum "$reference" | awk '{ print $1 }')"
test "$actual_hash" = "$expected_hash" || {
	printf 'Reference SHA256 mismatch: expected %s, got %s\n' "$expected_hash" "$actual_hash" >&2
	exit 1
}

# Keep extraction and creation inside a single fakeroot session: otherwise the
# fake character-device and ownership metadata would be lost between commands.
if [ -z "${FAKEROOTKEY:-}" ]; then
	exec fakeroot -- "$0" "$reference" "$output"
fi

gzip -t "$reference"

workdir="$(mktemp -d "${TMPDIR:-/tmp}/op3-reproducible-initrd.XXXXXX")"
trap 'rm -rf "$workdir"' EXIT
stage="$workdir/rootfs"
mkdir "$stage"

gzip -cd "$reference" | (cd "$stage" && cpio -idm --no-absolute-filenames --quiet)

gzip -cd "$reference" | cpio -it --quiet | LC_ALL=C sort > "$workdir/source.paths"
(cd "$stage" && LC_ALL=C find . -print0 | LC_ALL=C sort -z |
	cpio --null -o -H newc --reproducible --quiet) > "$workdir/base.cpio"
gzip -9 -n -c "$workdir/base.cpio" > "$output"

gzip -t "$output"
gzip -cd "$output" | cpio -it --quiet | LC_ALL=C sort > "$workdir/output.paths"
cmp "$workdir/source.paths" "$workdir/output.paths"

{
	printf 'reference_sha256 %s\n' "$actual_hash"
	printf 'generated_sha256 %s\n' "$(sha256sum "$output" | awk '{ print $1 }')"
	printf 'entries %s\n' "$(wc -l < "$workdir/output.paths")"
	(cd "$stage" && LC_ALL=C find . -printf '%y\t%m\t%U\t%G\t%s\t%p\t%l\n' | LC_ALL=C sort)
	(cd "$stage" && LC_ALL=C find . -type f -print0 | LC_ALL=C sort -z |
		xargs -0 -r sha256sum)
} > "$output.manifest"

printf 'reference=%s\n' "$reference"
printf 'output=%s\n' "$output"
printf 'manifest=%s.manifest\n' "$output"
sha256sum "$reference" "$output"
