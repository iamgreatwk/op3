#!/usr/bin/env bash
set -euo pipefail

# Stage the Adreno 530 GPU firmware for the initramfs overlay.
#
# The kernel requests qcom/a530_pm4.fw and qcom/a530_pfp.fw while the GPU
# probes, which happens before any user-space root filesystem is available, so
# these two files must live in the initramfs and cannot be staged on sda15.
# Together they are about 35 KB, which is irrelevant to the OnePlus 3 boot.img
# size limit.
#
# Source: the host linux-firmware package. Ubuntu ships the files
# zstd-compressed; uncompressed copies are accepted too. Every file is verified
# against the pinned SHA256 below, so a package upgrade that changes the
# firmware fails here instead of silently shipping a different blob.
#
# Usage:
#   scripts/prepare-a530-firmware.sh [destdir]

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dest="${1:-$project_root/artifacts/a530-firmware}"

A530_PM4_SHA256=6419f35956ec7307af83723fedfba752520bacd8389eda0d0120e185e4cb1d3f
A530_PFP_SHA256=7ab3cd917e1f875f6a8387f8bc5efcf11ce9c88542ef2fc3cbda7d4b7b163286

source_dir=/lib/firmware/qcom

command -v zstd >/dev/null || {
  printf 'zstd is required: sudo apt install zstd\n' >&2
  exit 1
}

mkdir -p "$dest/lib/firmware/qcom"

fetch() {
  name="$1"
  want="$2"
  src="$source_dir/$name.zst"
  out="$dest/lib/firmware/qcom/$name"

  if [ -f "$src" ]; then
    zstd -d -c "$src" > "$out"
  elif [ -f "$source_dir/$name" ]; then
    cp "$source_dir/$name" "$out"
  else
    printf 'missing host GPU firmware: %s\n' "$src" >&2
    printf 'install it with: sudo apt install linux-firmware\n' >&2
    exit 1
  fi

  got="$(sha256sum "$out" | cut -d' ' -f1)"
  if [ "$got" != "$want" ]; then
    printf 'firmware checksum mismatch for %s\n' "$name" >&2
    printf '  expected %s\n  got      %s\n' "$want" "$got" >&2
    exit 1
  fi

  printf '%s  %s  %s bytes\n' "$got" "$name" "$(wc -c < "$out")"
}

fetch a530_pm4.fw "$A530_PM4_SHA256"
fetch a530_pfp.fw "$A530_PFP_SHA256"

printf 'staged under %s\n' "$dest"
