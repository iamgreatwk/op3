#!/usr/bin/env bash
set -euo pipefail

# Assemble the persistent sda15 payload for Issue #6.  The initramfs only
# carries the tiny run_recovery.sh selector; the recovery binary, browser
# session supervisor, and the two existing browser runners live on /newroot.
#
# Usage:
#   scripts/stage-recovery-rootfs.sh [output-tarball] [recovery-binary]

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
output="${1:-$project_root/artifacts/op3-recovery-browser-bundle.tar.gz}"
binary="${2:-$project_root/out/recovery/recovery_mainline}"

if [ ! -x "$binary" ]; then
	"$project_root/scripts/build-recovery-mainline.sh" "$binary"
fi

for input in \
	"$binary" \
	"$project_root/boot/recovery-browser-test/opt/op3-recovery/browser-session.sh" \
	"$project_root/boot/browser-test/opt/op3-browser/run.sh" \
	"$project_root/boot/pmos-chromium-test/opt/op3-chromium/run.sh"; do
	test -f "$input" || { printf 'Missing input: %s\n' "$input" >&2; exit 1; }
done

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT
stage="$tmpdir/root"
mkdir -p "$stage/sbin" "$stage/usr/bin" "$stage/opt/op3-recovery"

install -m 0755 "$binary" "$stage/sbin/recovery_mainline"
install -m 0755 \
	"$project_root/boot/recovery-browser-test/opt/op3-recovery/browser-session.sh" \
	"$stage/usr/bin/op3-browser-session"
# Keep this as a regular executable: init_mainline.sh mirrors regular files
# from /newroot/usr/bin into the initramfs PATH, but intentionally skips
# symlinks.  The recovery shell must therefore see `browser` after boot.
install -m 0755 \
	"$project_root/boot/recovery-browser-test/opt/op3-recovery/browser-session.sh" \
	"$stage/usr/bin/browser"

# The Cog runner is copied into the recovery payload so its one-shot mode can
# be updated independently of the large Buildroot bundle. OP3_BROWSER_BASE
# points it back to /newroot/opt/op3-browser at runtime.
install -m 0755 \
	"$project_root/boot/browser-test/opt/op3-browser/run.sh" \
	"$stage/opt/op3-recovery/cog-run.sh"
install -m 0755 \
	"$project_root/boot/pmos-chromium-test/opt/op3-chromium/run.sh" \
	"$stage/opt/op3-recovery/chromium-run.sh"

mkdir -p "$(dirname "$output")"
epoch="${SOURCE_DATE_EPOCH:-0}"
tar --sort=name --mtime="@$epoch" --owner=0 --group=0 --numeric-owner \
	-czf "$output" -C "$stage" .

printf 'output=%s\n' "$output"
tar -tzf "$output" | LC_ALL=C sort
sha256sum "$output"
printf '\nDeploy on the device (sda15 is /newroot):\n'
printf '  gzip -dc op3-recovery-browser-bundle.tar.gz | tar -x -C /newroot\n'
printf '  # select the integration initramfs before rebooting\n'
