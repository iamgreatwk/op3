#!/usr/bin/env bash
set -euo pipefail

# Stage an Alpine/pmos userland (aarch64) with chromium for the OP3 browser
# automation. pmOS is Alpine-based and the target kernel is the pmOS
# MSM8996 6.12 tree, so an Alpine rootfs is the matching userland.
#
# The bundle unpacks at /newroot/pmos on sda15 and is entered with chroot
# from the validated initramfs environment (wifi, RNDIS gadget, dropbear,
# DRM and firmware all stay in the initramfs — nothing in the boot flow
# changes). Chromium exposes CDP on 0.0.0.0:9222 so the PC Playwright
# driver can connect over wifi (192.168.1.x) or RNDIS (172.16.42.1).
#
# No qemu-user is required: apk.static (x86_64) installs aarch64 packages
# directly into the staging tree with --no-scripts (all selected packages
# are script-free data/musl binaries).
#
# Usage:
#   scripts/stage-pmos-chromium-rootfs.sh [output-tarball]

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
output="${1:-$project_root/artifacts/op3-pmos-chromium-rootfs.tar.gz}"

workdir="$project_root/out/pmos-rootfs-aarch64"
rootfs="$workdir/rootfs"
mirror="https://dl-cdn.alpinelinux.org/alpine"
alpine_ver="v3.22"
minirootfs="alpine-minirootfs-3.22.5-aarch64.tar.gz"
apk_static="apk-tools-static-2.14.10-r0.apk"

mkdir -p "$workdir" "$rootfs" "$(dirname "$output")"

# ---- 1. fetch inputs (cached) ---------------------------------------------
fetch() { # url dest
  if [ ! -f "$2" ]; then
    curl -fL --retry 3 -o "$2.part" "$1"
    mv "$2.part" "$2"
  fi
}

fetch "$mirror/$alpine_ver/releases/aarch64/$minirootfs" "$workdir/$minirootfs"
fetch "$mirror/$alpine_ver/main/x86_64/$apk_static" "$workdir/$apk_static"

# ---- 2. unpack the minirootfs ---------------------------------------------
if [ ! -e "$rootfs/etc/alpine-release" ]; then
  tar -xzf "$workdir/$minirootfs" -C "$rootfs"
fi

# ---- 3. extract apk.static (x86_64 host binary) ---------------------------
if [ ! -x "$workdir/apk.static" ]; then
  tar -xzf "$workdir/$apk_static" -C "$workdir" sbin/apk.static
  mv "$workdir/sbin/apk.static" "$workdir/apk.static"
fi

# ---- 4. install chromium + fonts (aarch64, no scripts) --------------------
cat > "$rootfs/etc/apk/repositories" <<EOF
$mirror/$alpine_ver/main
$mirror/$alpine_ver/community
EOF

"$workdir/apk.static" \
  --root "$rootfs" \
  --arch aarch64 \
  --initdb \
  --no-scripts \
  --update-cache \
  --repository "$mirror/$alpine_ver/main" \
  --repository "$mirror/$alpine_ver/community" \
  add chromium font-dejavu tzdata ca-certificates-bundle

# ---- 5. runtime configuration ---------------------------------------------
# DNS: same resolver the device already validated (OP3-BROWSER-005)
printf 'nameserver 223.5.5.5\nnameserver 223.6.6.6\n' > "$rootfs/etc/resolv.conf"

# chromium launcher: CDP for the PC Playwright driver
cat > "$rootfs/usr/local/bin/start-chromium.sh" <<'EOF'
#!/bin/sh
# Chromium launcher for the OP3 pmOS bundle.
# Headless + CDP on 127.0.0.1:9222 (default). Chromium 142+ binds DevTools
#   to loopback ONLY (--remote-debugging-address is ignored); the PC
#   connects through an SSH tunnel:
#     ssh -N -L 9222:127.0.0.1:9222 root@172.16.42.1
#   then Playwright: connect_over_cdp("http://localhost:9222")
# Set OP3_CHROMIUM_HEADLESS=0 to render on the panel instead (requires the
#   weston bundle running; chromium uses the Wayland backend).
# Set OP3_CHROMIUM_URL to the start page (default https://www.baidu.com).
# NOTE: the initramfs must have brought lo UP before launch
#   (`ip link set lo up`) — with lo down the DevTools http server cannot
#   bind ("Cannot start http server for devtools") and chromium dies.
export HOME=/tmp
export XDG_CACHE_HOME=/tmp/.cache
export XDG_CONFIG_HOME=/tmp/.config
mkdir -p "$XDG_CACHE_HOME" "$XDG_CONFIG_HOME" /tmp/chromium-profile

URL="${OP3_CHROMIUM_URL:-https://www.baidu.com}"
FLAGS="--no-sandbox --disable-dev-shm-usage --disable-crashpad \
  --disable-breakpad --user-data-dir=/tmp/chromium-profile \
  --remote-debugging-port=9222 \
  --lang=zh-CN --window-size=1080,1920"

if [ "${OP3_CHROMIUM_HEADLESS:-1}" = 1 ]; then
  # --disable-gpu: the kernel has no Vulkan/SwANGLE support; GPU init
  # failure otherwise takes the whole browser down.
  exec /usr/bin/chromium-browser $FLAGS \
    --headless=new --disable-gpu "$URL"
else
  exec /usr/bin/chromium-browser $FLAGS \
    --ozone-platform=wayland "$URL"
fi
EOF
chmod +x "$rootfs/usr/local/bin/start-chromium.sh"

# CJK font: the bundle has no Chinese glyphs by default (baidu renders
# tofu). Copy the PC's Noto CJK if available.
PC_NOTO="/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"
if [ -f "$PC_NOTO" ]; then
  mkdir -p "$rootfs/usr/share/fonts/noto"
  cp "$PC_NOTO" "$rootfs/usr/share/fonts/noto/"
fi

# ---- 6. sanity checks ------------------------------------------------------
required=(
  lib/ld-musl-aarch64.so.1
  usr/bin/chromium-browser
  usr/lib/libssl.so.3
  etc/ssl/certs/ca-certificates.crt
  usr/share/fonts/dejavu
)
missing=0
for f in "${required[@]}"; do
  ls "$rootfs/$f" >/dev/null 2>&1 || { printf 'Missing in rootfs: %s\n' "$f" >&2; missing=1; }
done
[ "$missing" = 0 ] || exit 1

# ---- 7. pack ---------------------------------------------------------------
tar -czf "$output" -C "$rootfs" .
sha256sum "$output"
du -h "$output"
