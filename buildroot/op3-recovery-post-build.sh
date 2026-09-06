#!/usr/bin/env bash
set -euo pipefail

# Buildroot post-build hook for the default OP3 recovery profile. It installs
# the tracked Wi-Fi CLI and only the ath10k module dependency closure from the
# owner-built kernel modules root. Credentials and firmware stay outside the
# Buildroot target; the firmware-provenance initrd supplies the latter.

target_dir="${1:?Buildroot target directory is required}"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
wifi_dir="$script_dir/wifi"
modules_root="${OP3_WIFI_MODULES_ROOT:-}"

die() {
	printf 'OP3 recovery post-build failed: %s\n' "$*" >&2
	exit 1
}

test -d "$target_dir" || die "missing target directory: $target_dir"
test -d "$wifi_dir/opt/op3-wifi" || die "missing staged Wi-Fi sources: $wifi_dir"
test -n "$modules_root" || die 'set OP3_WIFI_MODULES_ROOT to owner-built modules_install output'
test -d "$modules_root/lib/modules" || die "missing modules root: $modules_root"

install -D -m 0755 "$wifi_dir/opt/op3-wifi/wifi" \
	"$target_dir/opt/op3-wifi/wifi"
install -D -m 0755 "$wifi_dir/opt/op3-wifi/wifi-start" \
	"$target_dir/opt/op3-wifi/wifi-start"
install -D -m 0755 "$wifi_dir/usr/bin/wifi" \
	"$target_dir/usr/bin/wifi"

for required in \
	"$target_dir/usr/sbin/wpa_supplicant" \
	"$target_dir/usr/sbin/wpa_cli" \
	"$target_dir/usr/sbin/iw"; do
	test -x "$required" || die "Buildroot did not install $required"
done

releases=("$modules_root"/lib/modules/*)
test "${#releases[@]}" = 1 || die "expected one kernel release under $modules_root/lib/modules"
release="$(basename "${releases[0]}")"
depfile="${releases[0]}/modules.dep"
test -f "$depfile" || die "missing modules.dep: $depfile"

required_modules=(cfg80211 rfkill mac80211 ath ath10k_core ath10k_pci)
declare -A selected=()

collect() {
	local path="$1" dependency
	[ -n "$path" ] || die 'empty module dependency path'
	[[ -n "${selected[$path]:-}" ]] && return
	selected[$path]=1
	while IFS= read -r dependency; do
		collect "$dependency"
	done < <(awk -v p="$path" '$1 == p ":" { for (i = 2; i <= NF; i++) print $i }' "$depfile")
}

for module in "${required_modules[@]}"; do
	path="$(awk -v m="/$module.ko" \
		'$1 ~ (m "(\\.zst|\\.xz|\\.gz)?(:)?$") { sub(/:$/, "", $1); print $1; exit }' \
		"$depfile")"
	[ -n "$path" ] || die "required module absent from $depfile: $module"
	collect "$path"
done

module_target="$target_dir/lib/modules/$release"
mkdir -p "$module_target"
for path in "${!selected[@]}"; do
	test -f "${releases[0]}/$path" || die "missing selected module: $path"
	install -D -m 0644 "${releases[0]}/$path" "$module_target/$path"
done

printf '%s\n' "${!selected[@]}" | LC_ALL=C sort | while IFS= read -r path; do
	deps="$(awk -v p="$path" \
		'$1 == p ":" { for (i = 2; i <= NF; i++) printf "%s%s", (i == 2 ? "" : " "), $i; print "" }' \
		"$depfile")"
	printf '%s: %s\n' "$path" "$deps"
done > "$module_target/modules.dep"

printf 'OP3 Wi-Fi integrated into Buildroot target: %s\n' "$target_dir"
printf 'Kernel module release: %s\n' "$release"
printf 'Selected modules: %s\n' "${#selected[@]}"
