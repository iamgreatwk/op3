# OP3-PMOS-CHROMIUM-001 — pmOS userland bundle with chromium + CDP

## Why

The target kernel is the pmOS MSM8996 6.12 tree, so the matching userland is
Alpine-based (pmOS = Alpine). Alpine ships `chromium` in its repos with a
complete dependency set — no manual .so sweep like the Buildroot WPE bundle.
The automation flow stays exactly as validated on the PC: the PC runs
Playwright, the device runs chromium and exposes CDP over the network.

Boot flow is UNCHANGED: the validated initramfs remains the live system
(Wi-Fi, RNDIS+ACM gadget, dropbear, DRM, firmware). The bundle is a chroot
userland at `/newroot/pmos` on sda15.

## Bundle

- Built by `scripts/stage-pmos-chromium-rootfs.sh`
  (Alpine minirootfs 3.22.5 aarch64 + `apk.static` cross-install, no qemu).
- chromium 142.0.7444.59-r0 + font-dejavu + tzdata + CA certs;
  703 MiB installed, tarball ~276 MB.
- `artifacts/op3-pmos-chromium-rootfs.tar.gz`
  SHA256 `aefa14bd8e4257605205ff041b449bc9171a16e2126668922fa5e20fadedb4ae`
  (repacked with `--owner=0 --group=0`; apk permission warnings under
  non-root staging are expected and normalized at pack time).
- Launcher inside the chroot: `/usr/local/bin/start-chromium.sh`
  - default: headless + SwiftShader + CDP `0.0.0.0:9222`
  - `OP3_CHROMIUM_HEADLESS=0`: `--ozone-platform=wayland` (panel output,
    needs the weston bundle running)
  - `OP3_CHROMIUM_URL` overrides the start page (default baidu.com)
- `/etc/resolv.conf`: 223.5.5.5 / 223.6.6.6 (device-validated resolvers).

## Deploy / run / verify

```sh
scripts/op3-pmos-chromium.sh deploy        # host default 172.16.42.1; wifi: deploy 192.168.1.6
scripts/op3-pmos-chromium.sh run           # headless CDP
scripts/op3-pmos-chromium.sh status        # CDP /json/version check
```

PC-side check and Playwright connection:

```python
from playwright.sync_api import sync_playwright
with sync_playwright() as p:
    b = p.chromium.connect_over_cdp("http://192.168.1.6:9222")  # or 172.16.42.1:9222
    print(b.contexts[0].pages[0].title())
```

## Device validation (OP3-PMOS-CHROMIUM-001) — 2026-09-03/04

Boot: `fastboot boot artifacts/boot-oneplus3-pmos612-own-dtb-browser-net.img`
(kernel `6.12.1-msm8996+`). Deploy 715 MB to `/newroot/pmos` → DEPLOY-OK;
chromium launched; Playwright `connect_over_cdp` loaded baidu.com with full
CJK rendering (screenshot PASS).

### Findings baked into the scripts

1. **lo must be UP**: the initramfs leaves `lo` down (`state noop`); the
   DevTools http server then fails (`Cannot start http server for devtools`)
   and the browser dies silently. `device_run` now runs `ip link set lo up`.
2. **DevTools binds loopback only**: `--remote-debugging-address=0.0.0.0` is
   ignored (and removed). PC connects via SSH tunnel
   (`scripts/op3-pmos-chromium.sh tunnel` → `ssh -L 9222:127.0.0.1:9222`).
3. **GPU**: SwANGLE/Vulkan unsupported on this kernel — GPU init failure
   kills the whole browser. Headless launcher now uses `--disable-gpu`.
4. **IPv6**: DNS returns AAAA but the device has no IPv6 internet; disable
   with `sysctl -w net.ipv6.conf.all.disable_ipv6=1` before launching.
5. **CJK font**: bundle ships no Chinese glyphs; `NotoSansCJK-Regular.ttc`
   is copied from the PC if present, else fetch `font-noto-cjk`. A browser
   restart is required after adding fonts.
6. **Device busybox tar has no `-z`**; deploy pipes through
   `gzip -dc | tar -x`. `scp` to dropbear needs `-O` (no SFTP).
7. A stale profile `SingletonLock` (crash leftovers) blocks restart; remove
   `/newroot/pmos/tmp/chromium-profile/Singleton*` before relaunching.

## FAIL / notes

- `--no-sandbox` is required (running as root inside the chroot).
- `--disable-dev-shm-usage` avoids needing a tmpfs at `/newroot/pmos/dev/shm`.
- GPU accel is not used by default (SwiftShader); if the a530 renders
  correctly (`OP3-EGL-001` passed), try `run-gui` with the Wayland backend
  and drop the SwiftShader flags as a follow-up gate.
- Space: needs >= 900 MB free on `/newroot` (deploy checks and aborts).
