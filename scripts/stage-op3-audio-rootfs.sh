#!/usr/bin/env bash
set -euo pipefail

# Stage only OP3-AUDIO-MIC-001's device-local diagnostics into an already
# built Buildroot target tree.  This script does not invoke Buildroot.

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
target_dir=${1:?usage: stage-op3-audio-rootfs.sh /absolute/path/to/Buildroot/target}
source_dir="$project_root/boot/audio-test/opt/op3-audio"
destination="$target_dir/opt/op3-audio"

case "$target_dir" in
	/*) ;;
	*) printf 'Target path must be absolute: %s\n' "$target_dir" >&2; exit 2 ;;
esac

test -d "$target_dir"
for program in tinymix tinyplay tinycap; do
	if [ ! -x "$target_dir/usr/bin/$program" ] && [ ! -x "$target_dir/bin/$program" ]; then
		printf 'Missing %s in target; enable BR2_PACKAGE_TINYALSA and BR2_PACKAGE_TINYALSA_TOOLS.\n' \
		"$program" >&2
		exit 1
	fi
done

install -d -m 0755 "$destination"
install -m 0755 "$source_dir/route.sh" "$destination/route.sh"
printf 'Staged %s\n' "$destination/route.sh"
