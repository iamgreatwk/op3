# OP3 recovery Wi-Fi cold-start association — Issue #10

```text
Task / GitHub Issue: https://github.com/iamgreatwk/op3/issues/10
Role: Implementation
Baseline commit: 8373e11
Working branch: agent/implementation/recovery-browser-001
Changed files: boot/wifi/opt/op3-wifi/wifi; boot/wifi/README.md;
  docs/handoff/latest.md; docs/handoff/op3-recovery-wifi-timeout-001.md
Commit SHA: 604fad0; timeout-guard correction: b617476; wait extension: 1a166c0

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
  That bundle is superseded by
  `artifacts/op3-wifi-bundle-ipv6-assoc90-fixed.tar.gz`, SHA256
  `143714f00fb17fe5c63f3cb00821ea77e3e0c8616504d8e497b89e9d65f200f7`.
  Both bundles contain the validated modules and no credentials; deploy only
  the replacement bundle for the corrected guard. The 90-second bundle is
  superseded by
  `artifacts/op3-wifi-bundle-ipv6-assoc180-clean-retry.tar.gz`, SHA256
  `3b68515b71f2226d5e82cc55a6262b64b0dfa4fd4f323456aea29ccde6247938`.

Device test run by project owner: 2026-09-06 — direct `wifi auto` with
  `OP3_WIFI_ASSOC_TIMEOUT=120` returned 0, associated to SSID 1106, and
  obtained IPv4 `192.168.1.5` with a default route. Kernel timestamps show
  association at approximately 171 seconds after boot.
Device result: PARTIAL — the controlled auto path passed with a 120-second
  override; cold-boot validation of the new 180-second default is pending.
Evidence links / log paths: `/root/boot_mainline.log`,
  `/root/dmesg_early.txt` or `/root/dmesg_rootfs.txt`, `wifi current`,
  `ip -4 addr show wlan0`, `ip -4 route`, `wifi ipv6 status`, and filtered
  ath10k/wlan dmesg. The triggering evidence is the owner-provided log where
  driver initialization occurred near 9 seconds and successful association
  occurred near 46 seconds.

Conclusion: INCONCLUSIVE
Uncertainties:
  - The controlled auto run passed with a 120-second override, but the
    default-180-second cold-boot run has not yet been recorded.
  - Repeated roaming between the two weak same-SSID BSSes may still require a
    separate policy change.
  - DHCP will only be attempted after the association wait succeeds.
Recommended next evidence capture: deploy the 180-second bundle, cold boot
  without running `wifi connect`, wait at least 180 seconds, and collect
  `wifi current`, `iw dev wlan0 link`, `ip -4 addr show wlan0`, `ip -4 route`,
  `wifi ipv6 status`, and one harmless network request. Then record the exact
  boot image and bundle SHA256 used for the run.
```
