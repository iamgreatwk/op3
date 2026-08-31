#!/usr/bin/env bash
set -euo pipefail

# Rebuild the validated base initramfs while replacing exactly the declared
# MSM8996 firmware files with the independently staged, hash-verified copies.

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
reference="${1:-$project_root/artifacts/reference-initrd.img}"
firmware_stage="${2:-$project_root/artifacts/msm8996-oneplus3-firmware-verified}"
output="${3:-$project_root/artifacts/initrd-op3-firmware-provenance.cpio.gz}"
manifest="$project_root/boot/base-initramfs/msm8996-oneplus3-firmware.tsv"
expected_file="$project_root/boot/base-initramfs/reference-initrd.sha256"
output="$(realpath -m "$output")"

usage() {
	printf '%s\n' "Usage: $0 [reference-initrd.img] [firmware-stage-dir] [output-initrd.cpio.gz]" >&2
}

if [ "$#" -gt 3 ]; then
	usage
	exit 2
fi

for command in awk cmp cpio fakeroot find gzip install realpath sha256sum sort wc xargs; do
	command -v "$command" >/dev/null || {
		printf 'Missing required command: %s\n' "$command" >&2
		exit 1
	}
done

for input in "$reference" "$manifest" "$expected_file"; do
	test -f "$input" || { printf 'Missing input: %s\n' "$input" >&2; exit 1; }
done
test -d "$firmware_stage" || { printf 'Missing firmware stage: %s\n' "$firmware_stage" >&2; exit 1; }
test ! -e "$output" || { printf 'Refusing to overwrite output: %s\n' "$output" >&2; exit 1; }
test ! -e "$output.manifest" || { printf 'Refusing to overwrite manifest: %s\n' "$output.manifest" >&2; exit 1; }

expected_reference_hash="$(awk 'NR == 1 { print $1 }' "$expected_file")"
actual_reference_hash="$(sha256sum "$reference" | awk '{ print $1 }')"
test "$actual_reference_hash" = "$expected_reference_hash" || {
	printf 'Reference SHA256 mismatch: expected %s, got %s\n' "$expected_reference_hash" "$actual_reference_hash" >&2
	exit 1
}

if [ -z "${FAKEROOTKEY:-}" ]; then
	exec fakeroot -- "$0" "$reference" "$firmware_stage" "$output"
fi

workdir="$(mktemp -d "${TMPDIR:-/tmp}/op3-firmware-provenance-initrd.XXXXXX")"
trap 'rm -rf "$workdir"' EXIT
stage="$workdir/rootfs"
mkdir "$stage"

gzip -t "$reference"
gzip -cd "$reference" | (cd "$stage" && cpio -idm --no-absolute-filenames --quiet)

while IFS=$'\t' read -r path size hash source early; do
	case "$path" in ''|'#'*) continue ;; esac
	control="$stage/$path"
	replacement="$firmware_stage/$path"
	for file in "$control" "$replacement"; do
		test -f "$file" || { printf 'Missing declared firmware: %s\n' "$file" >&2; exit 1; }
	done
	test "$(wc -c < "$control")" = "$size"
	test "$(sha256sum "$control" | awk '{ print $1 }')" = "$hash"
	test "$(wc -c < "$replacement")" = "$size"
	test "$(sha256sum "$replacement" | awk '{ print $1 }')" = "$hash"
	install -m 0644 "$replacement" "$control"
done < "$manifest"

gzip -cd "$reference" | cpio -it --quiet | LC_ALL=C sort > "$workdir/source.paths"
(cd "$stage" && LC_ALL=C find . -print0 | LC_ALL=C sort -z |
	cpio --null -o -H newc --reproducible --quiet) > "$workdir/output.cpio"
gzip -9 -n -c "$workdir/output.cpio" > "$output"

gzip -t "$output"
gzip -cd "$output" | cpio -it --quiet | LC_ALL=C sort > "$workdir/output.paths"
cmp "$workdir/source.paths" "$workdir/output.paths"

{
	printf 'reference_sha256 %s\n' "$actual_reference_hash"
	printf 'generated_sha256 %s\n' "$(sha256sum "$output" | awk '{ print $1 }')"
	printf 'entries %s\n' "$(wc -l < "$workdir/output.paths")"
	printf 'replaced_firmware_manifest %s\n' "$(sha256sum "$manifest" | awk '{ print $1 }')"
	(cd "$stage" && LC_ALL=C find . -printf '%y\t%m\t%U\t%G\t%s\t%p\t%l\n' | LC_ALL=C sort)
	(cd "$stage" && LC_ALL=C find . -type f -print0 | LC_ALL=C sort -z |
		xargs -0 -r sha256sum)
} > "$output.manifest"

printf 'reference=%s\n' "$reference"
printf 'firmware_stage=%s\n' "$firmware_stage"
printf 'output=%s\n' "$output"
printf 'manifest=%s.manifest\n' "$output"
sha256sum "$reference" "$output"
