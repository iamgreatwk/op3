#!/usr/bin/env bash
set -euo pipefail

# Create a persistent sda15 bundle containing the exact ath10k dependency
# closure for one owner-built kernel. This script never builds a kernel.

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
modules_root="${1:-$project_root/artifacts/op3-wifi-modules-root}"
output="${2:-$project_root/artifacts/op3-wifi-bundle.tar.gz}"

test -d "$modules_root/lib/modules" || { printf 'Missing modules root: %s\n' "$modules_root" >&2; exit 1; }
test ! -e "$output" || { printf 'Refusing to overwrite output: %s\n' "$output" >&2; exit 1; }

releases=("$modules_root"/lib/modules/*)
test "${#releases[@]}" = 1 || { printf 'Expected exactly one kernel release under %s/lib/modules\n' "$modules_root" >&2; exit 1; }
release="$(basename "${releases[0]}")"
depfile="${releases[0]}/modules.dep"
test -f "$depfile" || { printf 'Missing modules.dep; run modules_install after the owner build\n' >&2; exit 1; }

required=(cfg80211 rfkill mac80211 ath ath10k_core ath10k_pci)
for module in "${required[@]}"; do
	rg -q "/${module}\.ko(\.zst|\.xz)?(:| )" "$depfile" || {
		printf 'Required module absent from %s: %s\n' "$depfile" "$module" >&2
		exit 1
	}
done

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT
bundle="$tmpdir/root"
mkdir -p "$bundle/opt/op3-wifi" "$bundle/usr/bin" "$bundle/lib/modules/$release"
cp -a "$project_root/boot/wifi/opt/op3-wifi/." "$bundle/opt/op3-wifi/"
cp -a "$project_root/boot/wifi/usr/bin/wifi" "$bundle/usr/bin/wifi"

declare -A selected=()
collect() {
	local path="$1" dependency
	[[ -n "${selected[$path]:-}" ]] && return
	selected[$path]=1
	while IFS= read -r dependency; do
		collect "$dependency"
	done < <(awk -v p="$path" '$1 == p ":" { for (i = 2; i <= NF; i++) print $i }' "$depfile")
}
for module in "${required[@]}"; do
	path="$(awk -v m="/$module.ko" '$1 ~ (m "(\\.zst|\\.xz)?:$") { sub(/:$/, "", $1); print $1; exit }' "$depfile")"
	collect "$path"
done

for path in "${!selected[@]}"; do
	test -n "$path" || { printf 'Unable to resolve module dependency\n' >&2; exit 1; }
	install -D -m 0644 "${releases[0]}/$path" "$bundle/lib/modules/$release/$path"
done
printf '%s\n' "${!selected[@]}" | LC_ALL=C sort | while IFS= read -r path; do
	deps=$(awk -v p="$path" '$1 == p ":" { for (i = 2; i <= NF; i++) printf "%s%s", (i == 2 ? "" : " "), $i; print "" }' "$depfile")
	printf '%s: %s\n' "$path" "$deps"
done > "$bundle/lib/modules/$release/modules.dep"

tar -czf "$output" -C "$bundle" .
printf 'output=%s\n' "$output"
sha256sum "$output"
