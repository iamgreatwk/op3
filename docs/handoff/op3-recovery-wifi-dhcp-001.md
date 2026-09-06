# OP3 recovery Wi-Fi DHCP retry — Issue #11

```text
Task / GitHub Issue: https://github.com/iamgreatwk/op3/issues/11
Role: Implementation
Baseline commit: 1fb9bdc
Working branch: agent/implementation/recovery-browser-001
Changed files: boot/wifi/opt/op3-wifi/wifi; boot/wifi/README.md;
  docs/handoff/latest.md; docs/handoff/op3-recovery-wifi-dhcp-001.md
Commit SHA: pending implementation commit

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
Build result: Agent shell/static checks pending final checkpoint; owner
  boot-image repack NOT_RUN
Artifacts and SHA256: Replacement no-credential bundle pending generation.

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: `/root/boot_mainline.log`,
  `/root/dmesg_early.txt` or `/root/dmesg_rootfs.txt`, `wifi current`,
  `ip -4 addr show wlan0`, `ip -4 route`, `wifi ipv6 status`, and the
  observed DHCP client result.

Conclusion: INCONCLUSIVE
Uncertainties:
  - The phone currently sees the same SSID on two very weak BSSes and the
    association may still roam or drop; persistent DHCP retry cannot repair a
    link that remains disconnected.
  - The background client remains available after the 30-second observation
    window, so a delayed lease may appear after the command returns nonzero.
Recommended next experiment: generate and deploy the replacement bundle to
  sda15, boot the existing recovery image without manual `wifi connect`, wait
  through the 90-second association and 30-second DHCP observation windows,
  then collect IPv4, route, WPA state, IPv6 status, and filtered dmesg.
```
