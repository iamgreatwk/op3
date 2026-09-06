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
recovery_postbuild="$project_root/buildroot/op3-recovery-post-build.sh"
recovery_package="$project_root/buildroot/package-patches/op3-recovery"
initramfs_package="$project_root/buildroot/package-patches/op3-initramfs"
initramfs_source="$project_root/boot/initramfs"
wifi_source="$project_root/boot/wifi"
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
	test -f "$recovery_postbuild" || die "missing recovery post-build hook: $recovery_postbuild"
	test -d "$wifi_source" || die "missing Wi-Fi sources: $wifi_source"
	for input in \
		"$recovery_package/Config.in" \
		"$recovery_package/op3-recovery.mk" \
		"$project_root/recovery/recovery_mainline.c" \
		"$project_root/recovery/recovery_drm.c" \
		"$project_root/third_party/libtsm" \
		"$initramfs_package/Config.in" \
		"$initramfs_package/op3-initramfs.mk" \
		"$initramfs_source/init" \
		"$initramfs_source/etc/inittab" \
		"$initramfs_source/sbin/init_mainline.sh" \
		"$initramfs_source/sbin/run_recovery.sh" \
		"$initramfs_source/usr/bin/init_audio_mainline.sh" \
		"$initramfs_source/usr/bin/feed_entropy.c" \
		"$initramfs_source/usr/bin/netcat.c" \
		"$project_root/boot/audio-test/opt/op3-audio/route.sh" \
		"$wifi_source/initramfs/usr/bin/wifi_auto.sh" \
		"$project_root/boot/recovery-browser-test/opt/op3-recovery/browser-session.sh" \
		"$project_root/boot/browser-test/opt/op3-browser/run.sh" \
		"$project_root/boot/pmos-chromium-test/opt/op3-chromium/run.sh"; do
		test -e "$input" || die "missing recovery package input: $input"
	done
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

if [ "$profile" = recovery ]; then
	recovery_board="$buildroot_dir/board/oneplus3/recovery"
	install -D -m 0755 "$recovery_postbuild" \
		"$recovery_board/post-build.sh"
	mkdir -p "$recovery_board/wifi"
	cp -a "$wifi_source/." "$recovery_board/wifi/"
	recovery_package_dir="$buildroot_dir/package/op3-recovery"
	install -D -m 0644 "$recovery_package/Config.in" \
		"$recovery_package_dir/Config.in"
	install -D -m 0644 "$recovery_package/op3-recovery.mk" \
		"$recovery_package_dir/op3-recovery.mk"
	mkdir -p "$recovery_package_dir/source/recovery" \
		"$recovery_package_dir/source/libtsm" \
		"$recovery_package_dir/source/runners"
	cp -a "$project_root/recovery/." "$recovery_package_dir/source/recovery/"
	cp -a "$project_root/third_party/libtsm/." \
		"$recovery_package_dir/source/libtsm/"
	install -m 0755 \
		"$project_root/boot/recovery-browser-test/opt/op3-recovery/browser-session.sh" \
		"$recovery_package_dir/source/runners/browser-session.sh"
	install -m 0755 \
		"$project_root/boot/browser-test/opt/op3-browser/run.sh" \
		"$recovery_package_dir/source/runners/cog-run.sh"
	install -m 0755 \
		"$project_root/boot/pmos-chromium-test/opt/op3-chromium/run.sh" \
		"$recovery_package_dir/source/runners/chromium-run.sh"
	package_config="$buildroot_dir/package/Config.in"
	if grep -Fq 'source "package/op3-recovery/Config.in"' "$package_config"; then
		die "Buildroot already contains the op3-recovery package registration"
	fi
	sed -i '/^[[:space:]]*source "package\/strace\/Config.in"$/a\	source "package/op3-recovery/Config.in"' \
		"$package_config"
	initramfs_package_dir="$buildroot_dir/package/op3-initramfs"
	install -D -m 0644 "$initramfs_package/Config.in" \
		"$initramfs_package_dir/Config.in"
	install -D -m 0644 "$initramfs_package/op3-initramfs.mk" \
		"$initramfs_package_dir/op3-initramfs.mk"
	mkdir -p "$initramfs_package_dir/source"
	install -m 0755 "$initramfs_source/init" \
		"$initramfs_package_dir/source/init"
	install -m 0644 "$initramfs_source/etc/inittab" \
		"$initramfs_package_dir/source/inittab"
	install -m 0755 "$initramfs_source/sbin/init_mainline.sh" \
		"$initramfs_package_dir/source/init_mainline.sh"
	install -m 0755 "$initramfs_source/sbin/run_recovery.sh" \
		"$initramfs_package_dir/source/run_recovery.sh"
	install -m 0755 "$initramfs_source/usr/bin/init_audio_mainline.sh" \
		"$initramfs_package_dir/source/init_audio_mainline.sh"
	install -m 0644 "$initramfs_source/usr/bin/feed_entropy.c" \
		"$initramfs_package_dir/source/feed_entropy.c"
	install -m 0644 "$initramfs_source/usr/bin/netcat.c" \
		"$initramfs_package_dir/source/netcat.c"
	install -m 0755 "$project_root/boot/audio-test/opt/op3-audio/route.sh" \
		"$initramfs_package_dir/source/route.sh"
	install -m 0755 "$wifi_source/initramfs/usr/bin/wifi_auto.sh" \
		"$initramfs_package_dir/source/wifi_auto.sh"
	if grep -Fq 'source "package/op3-initramfs/Config.in"' "$package_config"; then
		die "Buildroot already contains the op3-initramfs package registration"
	fi
	sed -i '/^[[:space:]]*source "package\/op3-recovery\/Config.in"$/a\	source "package/op3-initramfs/Config.in"' \
		"$package_config"
fi

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
if [ "$profile" = recovery ]; then
	printf '  OP3_WIFI_MODULES_ROOT=%q make -C %q O=%q %s\n' \
		"$project_root/$BUILDROOT_WIFI_MODULES_ROOT" \
		"$buildroot_dir" "$buildroot_output" "$config_name"
	printf '  OP3_INITRAMFS_FIRMWARE_ROOT=%q OP3_WIFI_MODULES_ROOT=%q make -C %q O=%q BR2_JLEVEL=3\n' \
		"$project_root/artifacts/op3-initramfs-firmware" \
		"$project_root/$BUILDROOT_WIFI_MODULES_ROOT" \
		"$buildroot_dir" "$buildroot_output"
else
	printf '  make -C %q O=%q %s\n' \
		"$buildroot_dir" "$buildroot_output" "$config_name"
	printf '  make -C %q O=%q BR2_JLEVEL=3\n' \
		"$buildroot_dir" "$buildroot_output"
fi
