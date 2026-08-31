# OP3-BROWSER-005 — networked browser gate (cog + WPE over Wi-Fi)

Derived initramfs overlay on top of the validated browser gate
(`boot/browser-test`) and the validated Wi-Fi integration (OP3-WIFI-001,
`docs/handoff/op3-wifi-001.md`). The only difference from the browser gate
overlay is `sbin/run_recovery.sh`, which exports:

```text
OP3_BROWSER_NET=1
OP3_BROWSER_URL=${OP3_BROWSER_URL:-http://www.baidu.com}
```

Everything else (Wi-Fi association, DHCP, DNS, clock step, the URL itself)
lives in `boot/browser-test/opt/op3-browser/run.sh`, which is deployed on
sda15 — iteration happens over SSH without repacking the boot image.

## Phase 1 (no Buildroot rebuild): http://neverssl.com

www.baidu.com 302-redirects to https (owner-confirmed 2026-08-31) and the
current bundle has no TLS backend, so phase 1 uses the http-only
neverssl.com to validate the full remote chain. The launcher defaults to it;
override per run with `OP3_BROWSER_URL=http://www.baidu.com` to capture the
on-device TLS error page as evidence.

Requires on sda15:

1. The existing browser bundle `/newroot/opt/op3-browser` (unchanged bundle;
   the updated `run.sh` must be redeployed, see below).
2. The Wi-Fi bundle from OP3-WIFI-001 staged at `/newroot` (modules +
   `/newroot/opt/op3-wifi/wifi` + `/newroot/usr/bin/wifi`). If the default
   profile was already associated by the initramfs `wifi_auto.sh`, run.sh
   detects the existing IPv4 address and skips re-association.

Pack the initramfs overlay on top of the Wi-Fi-provenance base archive:

```sh
OVERLAY_SOURCE="$PWD/boot/browser-net-test" scripts/make-drm-test-initrd.sh \
  artifacts/initrd-op3-firmware-provenance-v2.cpio.gz \
  artifacts/op3-drm-dumb \
  artifacts/initrd-op3-browser-net.cpio.gz
scripts/pack-boot.sh \
  <OP3-BOOT-044 Image.gz> <own DTB> artifacts/initrd-op3-browser-net.cpio.gz \
  artifacts/boot-oneplus3-pmos612-own-dtb-browser-net.img
```

Deploy the updated `run.sh` over SSH (sda15):

```sh
scp boot/browser-test/opt/op3-browser/run.sh root@172.16.42.1:/newroot/opt/op3-browser/run.sh
```

PASS criteria: launcher log (`/newroot/var/log/op3-browser-net.log`) shows an
IPv4 address on `wlan0`, a working default route and `ping 223.5.5.5 OK`;
GPU power-on logged AFTER the network is up; cog loads the remote page
("Loaded successfully"); the owner sees the neverssl.com page rendered on
the panel.
FAIL: FATAL-NET lines (no association / no DHCP / no route), a hard reset
around association time (GPU/wifi power interaction — see the handoff debug
log), TLS or DNS errors in cog.log, or a blank panel.

## Phase 2 (owner Buildroot rebuild): https://www.baidu.com

`buildroot/op3-browser.defconfig` now selects `BR2_PACKAGE_GLIB_NETWORKING=y`
and `BR2_PACKAGE_CA_CERTIFICATES=y` (GLib TLS backend via GIO module —
libsoup3/WebKit picks it up at runtime, no WebKit rebuild expected, but the
incremental build still runs as owner). Without these, https targets fail
with "TLS support not available". After the rebuild, restage the browser
bundle and set the URL to `https://www.baidu.com` (edit
`/newroot/opt/op3-browser/op3-url` on sda15, or re-export in the launcher).
