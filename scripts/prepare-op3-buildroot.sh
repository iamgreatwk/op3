#!/usr/bin/env bash
set -euo pipefail

# Restore the pinned Buildroot source and the project-owned defconfig/Cog
# patches. This prepares source only; it never invokes a Buildroot build.
#
# Usage:
#   scripts/prepare-op3-buildroot.sh [buildroot-source-dir] [recovery|browser]

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=/dev/null
source "$project_root/manifests/op3-recovery-audio-full.env"

buildroot_dir="${1:-$project_root/$BUILDROOT_SOURCE_DIR}"
buildroot_dir="$(readlink -m "$buildroot_dir")"
patch_dir="$project_root/$BUILDROOT_COG_PATCH_SOURCE_DIR"
profile="${2:-recovery}"

case "$profile" in
	recovery)
		config_name="$BUILDROOT_CONFIG_NAME"
		config_source="$project_root/$BUILDROOT_CONFIG_SOURCE"
		config_expected_hash="$BUILDROOT_CONFIG_SOURCE_SHA256"
		buildroot_output="$project_root/${BUILDROOT_RECOVERY_TARGET_DIR%/target}"
		install_browser_patches=0
		;;
	browser)
		config_name="$BUILDROOT_BROWSER_CONFIG_NAME"
		config_source="$project_root/$BUILDROOT_BROWSER_CONFIG_SOURCE"
		config_expected_hash="$BUILDROOT_BROWSER_CONFIG_SOURCE_SHA256"
		buildroot_output="$project_root/${BUILDROOT_BROWSER_TARGET_DIR%/target}"
		install_browser_patches=1
		;;
	*)
		printf 'usage: scripts/prepare-op3-buildroot.sh [buildroot-source-dir] [recovery|browser]\n' >&2
		exit 2
		;;
esac

die() {
	printf 'Buildroot preparation failed: %s\n' "$*" >&2
	exit 1
}

command -v git >/dev/null 2>&1 || die 'git is required'
command -v sha256sum >/dev/null 2>&1 || die 'sha256sum is required'
test -f "$config_source" || die "missing defconfig: $config_source"
test -d "$patch_dir" || die "missing Cog patch directory: $patch_dir"
config_hash="$(sha256sum "$config_source" | awk '{print $1}')"
[ "$config_hash" = "$config_expected_hash" ] || {
	die "defconfig SHA256 mismatch: expected $config_expected_hash, got $config_hash"
}

if [ "$profile" = recovery ]; then
	for symbol in \
		BR2_PACKAGE_MESA3D \
		BR2_PACKAGE_WESTON \
		BR2_PACKAGE_WPEWEBKIT \
		BR2_PACKAGE_WPEWEBKIT_WEBDRIVER \
		BR2_PACKAGE_COG; do
		if grep -Eq "^${symbol}=" "$config_source"; then
			die "$symbol must remain disabled in the recovery profile"
		fi
	done
fi

if [ ! -e "$buildroot_dir" ]; then
	mkdir -p "$(dirname "$buildroot_dir")"
	git clone "$BUILDROOT_REMOTE_URL" "$buildroot_dir"
fi

git -C "$buildroot_dir" rev-parse --is-inside-work-tree >/dev/null 2>&1 ||
	die "not a Buildroot Git repository: $buildroot_dir"

status="$(git -C "$buildroot_dir" status --porcelain --untracked-files=all)"
[ -z "$status" ] || die "Buildroot source is not clean; use a fresh checkout: $buildroot_dir"

if ! git -C "$buildroot_dir" cat-file -e "$BUILDROOT_COMMIT^{commit}" 2>/dev/null; then
	git -C "$buildroot_dir" fetch origin "$BUILDROOT_COMMIT"
fi
git -C "$buildroot_dir" checkout --detach "$BUILDROOT_COMMIT"

install -m 0644 "$config_source" \
	"$buildroot_dir/configs/$config_name"

if [ "$install_browser_patches" -eq 1 ]; then
	for patch in \
		0001-op3-default-window-1080x1920.patch \
		0002-op3-enable-automation-before-view-creation.patch \
		0003-op3-allow-cookie-jar-in-automation-mode.patch; do
		test -f "$patch_dir/$patch" || die "missing project patch: $patch_dir/$patch"
		install -m 0644 "$patch_dir/$patch" "$buildroot_dir/package/cog/$patch"
	done
fi

printf 'Buildroot source: %s\n' "$buildroot_dir"
printf 'Buildroot commit: %s\n' "$(git -C "$buildroot_dir" rev-parse HEAD)"
printf 'Profile: %s\n' "$profile"
printf 'Installed config: %s/configs/%s\n' "$buildroot_dir" "$config_name"
printf 'Config SHA256: %s\n' "$config_hash"
if [ "$install_browser_patches" -eq 1 ]; then
	printf 'Installed Cog patches:\n'
	printf '  %s\n' \
		"$buildroot_dir/package/cog/0001-op3-default-window-1080x1920.patch" \
		"$buildroot_dir/package/cog/0002-op3-enable-automation-before-view-creation.patch" \
		"$buildroot_dir/package/cog/0003-op3-allow-cookie-jar-in-automation-mode.patch"
else
	printf 'Browser stack: disabled in the recovery profile\n'
fi
printf '\nOwner build commands (not run by this script):\n'
printf '  make -C %q O=%q %s\n' \
	"$buildroot_dir" "$buildroot_output" "$config_name"
printf '  make -C %q O=%q BR2_JLEVEL=3\n' \
	"$buildroot_dir" "$buildroot_output"
