# OP3 recovery Wi-Fi cold-start association — Issue #10

```text
Task / GitHub Issue: https://github.com/iamgreatwk/op3/issues/10
Role: Implementation
Baseline commit: 8373e11
Working branch: agent/implementation/recovery-browser-001
Changed files: boot/wifi/opt/op3-wifi/wifi; boot/wifi/README.md;
  docs/handoff/latest.md; docs/handoff/op3-recovery-wifi-timeout-001.md
Commit SHA: 604fad0

Layer: 02 recovery network userspace / association timeout
Previous PASS milestone: Issue #9 manual IPv4 connection and IPv6-off policy
  pass after the replacement Wi-Fi bundle was deployed
Hypothesis: Increasing the existing association wait from 30 to 90 seconds
  will cover the observed QCA6174 cold-boot scan/association delay, allowing
  `wifi auto` to reach the existing DHCP step.
Only variable changed: association wait timeout. The ath10k modules,
  firmware, regulatory data, WPA profile format, DHCP implementation, IPv6
  policy, kernel, DTS, DRM/GPU, audio, input, browser, and credentials are
  unchanged.

Build run by project owner: NOT_RUN
Build result: Agent shell/static checks PASS; owner boot-image repack NOT_RUN
Artifacts and SHA256: `artifacts/op3-wifi-bundle-ipv6-assoc90.tar.gz`
  `0e4f74b4f575c3339b13681da4c578256aac999d5c42212d67557909d5904c52`.
  The bundle contains the validated modules and the updated persistent CLI;
  no credentials are packaged.

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: `/root/boot_mainline.log`,
  `/root/dmesg_early.txt` or `/root/dmesg_rootfs.txt`, `wifi current`,
  `ip -4 addr show wlan0`, `ip -4 route`, `wifi ipv6 status`, and filtered
  ath10k/wlan dmesg. The triggering evidence is the owner-provided log where
  driver initialization occurred near 9 seconds and successful association
  occurred near 46 seconds.

Conclusion: INCONCLUSIVE
Uncertainties:
  - The 90-second wait is intended to cover the observed cold-start delay;
    repeated roaming between the two weak same-SSID BSSes may still require a
    separate policy change.
  - DHCP will only be attempted after the association wait succeeds.
Recommended next experiment: generate and deploy the replacement bundle to
  sda15 without changing the saved profile, boot the existing recovery image,
  and verify automatic `wpa_state=COMPLETED`, IPv4 address/default route, and
  `ipv6=off` without manually running `wifi connect`.
```
