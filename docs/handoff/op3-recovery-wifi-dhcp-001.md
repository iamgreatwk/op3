# OP3 recovery Wi-Fi DHCP retry — Issue #11

```text
Task / GitHub Issue: https://github.com/iamgreatwk/op3/issues/11
Role: Implementation
Baseline commit: 1fb9bdc
Working branch: agent/implementation/recovery-browser-001
Changed files: boot/wifi/opt/op3-wifi/wifi; boot/wifi/README.md;
  docs/handoff/latest.md; docs/handoff/op3-recovery-wifi-dhcp-001.md
Commit SHA: 8a25bbc; stale-lease cleanup correction: 835a926

Layer: 02 recovery network userspace / DHCP client lifecycle
Previous evidence: Issue #10 increased association wait; owner logs showed
  WPA association but no IPv4 lease. Historical 6.3.1 recovery used
  background `udhcpc -b -q`.
Hypothesis: Keeping `udhcpc` alive in background retry mode after WPA
  association, while observing the interface for 30 seconds, will obtain an
  IPv4 lease after transient AP/DHCP delays instead of exiting on the first
  unanswered exchange.
Only variable changed: DHCP client lifecycle. The ath10k modules, firmware,
  regulatory data, WPA profile format, association timeout, IPv6 policy,
  kernel, DTS, DRM/GPU, audio, input, browser, and credentials are unchanged.

Build run by project owner: NOT_RUN
Build result: Agent shell/static checks PASS; owner boot-image repack NOT_RUN
Artifacts and SHA256: `artifacts/op3-wifi-bundle-ipv6-dhcp-retry.tar.gz`
  `435b0a06826c1f9e6352698af6bbbcf2dcb90349271da8153c868d1f3d44d075`.
  The bundle contains the validated modules and the updated persistent CLI;
  no credentials are packaged. That bundle is superseded by
  `artifacts/op3-wifi-bundle-ipv6-assoc90-clean-retry.tar.gz`, SHA256
  `2d1bbe71a56363e2b7599936971d0d57a6e2d3d0fc9e1b203c5b512cb238b5a7`, which
  also includes the stale-lease cleanup.

Device test run by project owner: 2026-09-06 — the cleanup-bundle retest
  showed `wpa_state=SCANNING`, `NO-CARRIER`, no `wlan0` IPv4 address, and only
  the USB route. This is a real association failure, not a stale-lease result.
Device result: PASS candidate for the corrected automatic association and
  DHCP scope — the later 180-second cold-boot run reported `wpa_state=COMPLETED`,
  IPv4 `192.168.1.5/24`, and a default route via `192.168.1.1`. Integration
  acceptance remains pending.
Evidence links / log paths: `/root/boot_mainline.log`,
  `/root/dmesg_early.txt` or `/root/dmesg_rootfs.txt`, `wifi current`,
  `ip -4 addr show wlan0`, `ip -4 route`, `wifi ipv6 status`, and the
  observed DHCP client result.

Conclusion: INCONCLUSIVE
Uncertainties:
  - The phone currently sees the same SSID on two very weak BSSes and the
    association may still roam or drop; persistent DHCP retry cannot repair a
    link that remains disconnected.
  - The connected BSS is weak (approximately `-92 dBm`); stability under
    sustained browser traffic is not yet recorded.
  - The background client remains available after the 30-second observation
    window, so a delayed lease may appear after the command returns nonzero.
Recommended next experiment: deploy the cleanup bundle to sda15, boot the
  existing recovery image without manual `wifi connect`, wait through the
  90-second association and 30-second DHCP observation windows, then collect
  `iw dev wlan0 link`, IPv4 address/route, WPA state, IPv6 status, and filtered
  dmesg. A disconnected state must show no retained wlan0 IPv4 address.
```
