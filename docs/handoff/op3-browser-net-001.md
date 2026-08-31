# OP3-BROWSER-005 handoff — networked browser gate (cog over Wi-Fi)

```text
Task / GitHub Issue: Owner-authorized no-Issue task ("浏览器访问 baidu" test), OP3-BROWSER-005
Role: Implementation
Baseline commit: 31875200245e16643de1722ef12f823c910302cc (tip of agent/implementation/op3-wifi-port-001)
Working branch: agent/implementation/op3-browser-net-001
Changed files: boot/browser-test/opt/op3-browser/run.sh;
  boot/browser-net-test/{sbin/run_recovery.sh,README.md};
  buildroot/op3-browser.defconfig; docs (handoff, test-matrix)
Commit SHA: filled in per commit

Layer: 07 browser, network scope extension (one layer above the local-page browser gate)
Previous PASS milestone: OP3-BROWSER-004 (own-DTB browser, 2026-08-31) +
  OP3-WIFI-001 (WPA2-PSK association + DHCP, 2026-08-31)
Sole hypothesis: cog (WPE WebKit) can load and render a remote page
  (www.baidu.com) on the panel when the Wi-Fi link is brought up before the
  browser starts, using the existing sda15 browser bundle and the sda15 Wi-Fi
  CLI — no kernel/DTS change.
Only variable changed per test:
  1. Phase 1: the browser run.sh gains an OP3_BROWSER_URL/OP3_BROWSER_NET
     driven network bring-up (calls /newroot/opt/op3-wifi/wifi auto) and the
     launcher overlay passes http://www.baidu.com. No Buildroot rebuild.
  2. Phase 2 (separate owner build): BR2_PACKAGE_GLIB_NETWORKING=y +
     BR2_PACKAGE_CA_CERTIFICATES=y enable https. One build, one variable.

Build run by project owner: Phase 1 NOT needed (existing bundle). Phase 2:
  incremental Buildroot build (glib-networking, gnutls, ca-certificates).
Build result: NOT_RUN
Artifacts and SHA256: filled in after the owner's pack/test

Device test run by project owner: pending
Device result: NOT_RUN
Evidence links / log paths: /newroot/var/log/op3-browser-net.log,
  /run/op3-weston/cog.log, ACM console relay, owner panel confirmation

Conclusion: pending
Uncertainties:
  - TLS: the current bundle was built without glib-networking, so https://
    is expected to fail with "TLS support not available" until the phase-2
    rebuild. Phase 1 deliberately targets plain HTTP.
  - DNS: the Wi-Fi CLI writes /etc/resolv.conf (nameserver 223.5.5.5) in the
    initramfs root; WebKitNetworkProcess runs in that root and reads it.
  - TLS certificate validation also needs a sane clock; run.sh steps the
    clock with busybox ntpd (ntp.aliyun.com) before https attempts.
  - www.baidu.com must be reachable over plain HTTP without a forced
    redirect to https; if it 302-redirects, phase 2 becomes mandatory.
  - If the initramfs wifi_auto.sh already associated (OP3-WIFI-001 image),
    run.sh detects the existing IPv4 address and skips re-association.
Recommended next experiment: owner packs the browser-net initramfs
  (commands in boot/browser-net-test/README.md), fastboot boots, and reports
  the launcher log + panel observation. Then decide on the phase-2 https
  rebuild.
```

## Debug log

### Run 1 (2026-08-31): network PASS, hard reset at wlan0 association

Manual SSH run against the Wi-Fi-gate image (GPU firmware absent → GPU inert):
network bring-up fully PASSED — `wifi auto` found the already-associated
wlan0, IPv4 `192.168.1.6/24`, resolv.conf `223.5.5.5`, default route,
`ping 223.5.5.5 OK` — but weston died with `MESA: get_param failed -6
(ENODEV)` because that initramfs carries no A530 firmware. Expected; the
browser-net image does carry it.

The packed browser-net image run then hard-reset the device. Post-mortem from
the synced launcher log (`/newroot/var/log/op3-browser-net.log`):

```text
[ 8.7s] launcher: GPU runtime PM disabled: control=on, cur_freq=624000000
[11.3s] ath10k_pci probed, QCA6174 fw WLAN.RM.4.4.1-00309 loaded
[18.78s] wlan0: associated   (DHCP not yet run)
        < next 5 s periodic snapshot never happened: SoC hard reset >
```

This is the first configuration with the GPU forced on AND Wi-Fi active.
The wifi gate passed with the GPU inert (no firmware); the browser gates
passed with no Wi-Fi. Best-fit hypothesis: the ath10k TX power ramp at
association completion stacks on the always-on GPU behind the placeholder
DTB GPU regulators (docs/known-issues.md) → supply brownout → reset.

Countermeasure (one variable): move the GPU `control=on` from launcher time
to run.sh, after the Wi-Fi link is up and right before weston. The
browser-net launcher no longer touches GPU PM; run.sh does it idempotently
(the browser-test launcher duplicate is harmless).

### Next: rerun with sequenced GPU power-on

```sh
# repack (launcher change), then redeploy run.sh and fastboot boot
OVERLAY_SOURCE="$PWD/boot/browser-net-test" scripts/make-drm-test-initrd.sh \
  artifacts/initrd-op3-firmware-provenance-v2.cpio.gz artifacts/op3-drm-dumb \
  artifacts/initrd-op3-browser-net.cpio.gz
scp -O boot/browser-test/opt/op3-browser/run.sh root@172.16.42.1:/newroot/opt/op3-browser/run.sh
```

If the reset still lands at association time, the next isolation step is
associating Wi-Fi from init_mainline (wifi_auto overlay) BEFORE any GPU
activity, i.e. the exact wifi-gate power state plus a later GPU power-on.

## Design notes

- The Wi-Fi CLI (`/newroot/opt/op3-wifi/wifi auto`) already performs module
  loading (absolute insmod order), wpa association against the saved default
  profile, `udhcpc` and resolv.conf setup. Calling it from the sda15
  `run.sh` means the browser-net test needs **no initramfs wifi overlay and
  no wifi-specific repack** — only the browser-net `run_recovery.sh` overlay
  member on top of the OP3-BOOT-044-provenance base archive.
- The URL is resolved in order: `OP3_BROWSER_URL` (launcher env) >
  `/newroot/opt/op3-browser/op3-url` (one-line file on sda15, editable over
  SSH) > built-in local test page. The default behavior of the existing
  browser gate is unchanged when neither is set.
- Network bring-up failure is deterministic: `FATAL-NET` lines in the
  launcher log and exit 1 before weston starts, so a FAIL is diagnosable
  from the log alone.
