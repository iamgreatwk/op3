#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
manifest="${OP3_RECOVERY_MANIFEST:-$project_root/manifests/op3-recovery-audio-full.env}"

usage() {
  cat <<'EOF'
Usage:
  scripts/restore-op3-recovery-kernel.sh <fresh-kernel-repo> \
    <new-worktree-path> [restore-branch]

The fresh repository must contain KERNEL_BASE_COMMIT from the recovery
manifest and have no local changes. The external ath10k files are supplied by
OP3_EXTERNAL_INPUTS (or the legacy OP3_ATH10K_EXTFW_SOURCE override).
EOF
}

if (($# < 2 || $# > 3)); then
  usage >&2
  exit 2
fi

fresh_kernel="$(readlink -f "$1")"
destination="$(readlink -m "$2")"
restore_branch="${3:-agent/restore/op3-recovery-audio-full-001}"

test -r "$manifest" || {
  printf 'Missing recovery manifest: %s\n' "$manifest" >&2
  exit 1
}
# shellcheck source=/dev/null
source "$manifest"

die() {
  printf 'restore check failed: %s\n' "$*" >&2
  exit 1
}

check_sha256() {
  local expected="$1"
  local file="$2"
  local actual

  test -f "$file" || die "missing file: $file"
  actual="$(sha256sum "$file" | awk '{print $1}')"
  [[ "$actual" == "$expected" ]] ||
    die "SHA256 mismatch: $file (expected $expected, got $actual)"
}

test -d "$fresh_kernel" || die "missing fresh kernel repository: $fresh_kernel"
git -C "$fresh_kernel" rev-parse --is-inside-work-tree >/dev/null 2>&1 ||
  die "not a kernel Git repository: $fresh_kernel"
[[ -z "$(git -C "$fresh_kernel" status --porcelain --untracked-files=all)" ]] ||
  die "fresh kernel repository is not clean: $fresh_kernel"

series="$project_root/$KERNEL_PATCH_SERIES_DIR"
config_source="$project_root/$KERNEL_CONFIG_SOURCE"
test -d "$series" || die "missing patch series: $series"
test -f "$config_source" || die "missing tracked config: $config_source"
check_sha256 "$KERNEL_CONFIG_SOURCE_SHA256" "$config_source"

mapfile -t patches < <(find "$series" -maxdepth 1 -type f -name '*.patch' -print | sort)
[[ "${#patches[@]}" -eq "$KERNEL_PATCH_COUNT" ]] ||
  die "patch count is ${#patches[@]}, expected $KERNEL_PATCH_COUNT"

git -C "$fresh_kernel" cat-file -e "$KERNEL_BASE_COMMIT^{commit}" 2>/dev/null ||
  die "fresh kernel repository does not contain baseline $KERNEL_BASE_COMMIT"
[[ ! -e "$destination" ]] || die "refusing to overwrite existing path: $destination"
git -C "$fresh_kernel" show-ref --verify --quiet "refs/heads/$restore_branch" &&
  die "restore branch already exists: $restore_branch"

external_root="${OP3_ATH10K_EXTFW_SOURCE:-${OP3_EXTERNAL_INPUTS:-}}"
test -n "$external_root" ||
  die 'set OP3_EXTERNAL_INPUTS to the external input directory'
firmware_source="$external_root/$ATH10K_FIRMWARE_REL"
board_source="$external_root/$ATH10K_BOARD_REL"
if [ ! -f "$firmware_source" ] && [[ "$ATH10K_FIRMWARE_REL" == extfw/* ]]; then
	# The external-input archive stores the same files without the kernel
	# source-tree prefix. They are installed below extfw/ in the restored tree.
	firmware_source="$external_root/${ATH10K_FIRMWARE_REL#extfw/}"
	board_source="$external_root/${ATH10K_BOARD_REL#extfw/}"
fi
check_sha256 "$ATH10K_FIRMWARE_SHA256" "$firmware_source"
check_sha256 "$ATH10K_BOARD_SHA256" "$board_source"

git -C "$fresh_kernel" worktree add -b "$restore_branch" \
  "$destination" "$KERNEL_BASE_COMMIT"

if ! git -C "$destination" am --3way "${patches[@]}"; then
  printf 'Patch application failed; worktree retained for inspection: %s\n' \
    "$destination" >&2
  printf 'After inspection, abort with: git -C %q am --abort\n' "$destination" >&2
  printf 'Then remove only this worktree with: git -C %q worktree remove %q\n' \
    "$fresh_kernel" "$destination" >&2
  exit 1
fi

actual_tree="$(git -C "$destination" rev-parse HEAD^{tree})"
[[ "$actual_tree" == "$KERNEL_TREE" ]] || {
  printf 'restore check failed: tree is %s, expected %s\n' \
    "$actual_tree" "$KERNEL_TREE" >&2
  printf 'Worktree retained for inspection: %s\n' "$destination" >&2
  exit 1
}

firmware_destination="$destination/$ATH10K_FIRMWARE_REL"
board_destination="$destination/$ATH10K_BOARD_REL"
mkdir -p "$(dirname "$firmware_destination")"
install -m 0644 "$firmware_source" "$firmware_destination"
install -m 0644 "$board_source" "$board_destination"
check_sha256 "$ATH10K_FIRMWARE_SHA256" "$firmware_destination"
check_sha256 "$ATH10K_BOARD_SHA256" "$board_destination"

printf 'Restored kernel worktree: %s\n' "$destination"
printf 'Restored branch: %s\n' "$restore_branch"
printf 'Restored tree: %s\n' "$actual_tree"
printf 'Tracked config: %s\n' "$config_source"
printf 'No kernel build was started.\n'
