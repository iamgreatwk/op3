#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
manifest="${OP3_RECOVERY_MANIFEST:-$project_root/manifests/op3-recovery-audio-full.env}"
mode=source

usage() {
  cat <<'EOF'
Usage:
  scripts/verify-op3-recovery-manifest.sh [--source|--artifacts]

--source     Verify the assigned project/kernel branches and commits.
--artifacts  Also verify the locked kernel, initrd, and boot-image SHA256.
EOF
}

while (($#)); do
  case "$1" in
    --source|--artifacts)
      mode="${1#--}"
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      usage >&2
      exit 2
      ;;
  esac
  shift
done

test -r "$manifest" || {
  printf 'Missing recovery manifest: %s\n' "$manifest" >&2
  exit 1
}
# shellcheck source=/dev/null
source "$manifest"

die() {
  printf 'manifest check failed: %s\n' "$*" >&2
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
  printf 'PASS sha256 %s %s\n' "$actual" "$file"
}

project_branch="$(git -C "$project_root" symbolic-ref --quiet --short HEAD || true)"
[[ "$project_branch" == "$PROJECT_BRANCH" ]] ||
  die "top-level branch is '$project_branch', expected '$PROJECT_BRANCH'"

git -C "$project_root" merge-base --is-ancestor \
  "$PROJECT_SOURCE_COMMIT" HEAD ||
  die "top-level HEAD does not contain $PROJECT_SOURCE_COMMIT"

kernel_root="$project_root/$KERNEL_SOURCE_DIR"
test -d "$kernel_root" || die "missing kernel worktree: $kernel_root"
git -C "$kernel_root" rev-parse --is-inside-work-tree >/dev/null 2>&1 ||
  die "missing kernel Git repository: $kernel_root"
kernel_branch="$(git -C "$kernel_root" symbolic-ref --quiet --short HEAD || true)"
[[ "$kernel_branch" == "$KERNEL_BRANCH" ]] ||
  die "kernel branch is '$kernel_branch', expected '$KERNEL_BRANCH'"

kernel_commit="$(git -C "$kernel_root" rev-parse HEAD)"
[[ "$kernel_commit" == "$KERNEL_COMMIT" ]] ||
  die "kernel HEAD is '$kernel_commit', expected '$KERNEL_COMMIT'"

git -C "$kernel_root" merge-base --is-ancestor \
  "$KERNEL_BASE_COMMIT" "$KERNEL_COMMIT" ||
  die "kernel commit is not based on $KERNEL_BASE_COMMIT"

test -f "$project_root/$BOOT_PROFILE" ||
  die "missing boot profile: $project_root/$BOOT_PROFILE"
test -x "$project_root/scripts/pack-boot.sh" ||
  die "missing executable boot packer: $project_root/scripts/pack-boot.sh"
test -f "$project_root/$RECOVERY_INITRD" ||
  die "missing recovery initrd: $project_root/$RECOVERY_INITRD"

printf 'PASS source project=%s branch=%s\n' \
  "$(git -C "$project_root" rev-parse --short HEAD)" "$project_branch"
printf 'PASS source kernel=%s branch=%s\n' \
  "${kernel_commit:0:12}" "$kernel_branch"
printf 'PASS source baseline=%s\n' "$KERNEL_BASE_COMMIT"

if [[ "$mode" == artifacts ]]; then
  check_sha256 "$CONFIG_SHA256" \
    "$project_root/$KERNEL_OUTPUT_DIR/$KERNEL_CONFIG_REL"
  check_sha256 "$KERNEL_IMAGE_SHA256" \
    "$project_root/$KERNEL_OUTPUT_DIR/$KERNEL_IMAGE_REL"
  check_sha256 "$KERNEL_DTB_SHA256" \
    "$project_root/$KERNEL_OUTPUT_DIR/$KERNEL_DTB_REL"
  check_sha256 "$RECOVERY_INITRD_SHA256" \
    "$project_root/$RECOVERY_INITRD"
  check_sha256 "$BOOT_IMAGE_SHA256" \
    "$project_root/$BOOT_IMAGE"
fi

printf 'Recovery manifest verification: PASS (%s)\n' "$mode"
