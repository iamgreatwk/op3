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
