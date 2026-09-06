# OP3 recovery Wi-Fi auto-connect — Issue #8

```text
Task / GitHub Issue: https://github.com/iamgreatwk/op3/issues/8
Role: Implementation
Baseline commit: 254483efaf039a15457a42c8555b87859645ee80
Working branch: agent/implementation/recovery-browser-001
Changed files: scripts/make-recovery-browser-initrd.sh;
  boot/recovery-browser-test/README.md
Commit SHA: pending implementation commit

Layer: 02 recovery boot userspace / initramfs integration
Previous PASS milestone: Issue #7 direct DRM recovery with visible restore
Hypothesis: Appending the already validated OP3-WIFI-001
  `/usr/bin/wifi_auto.sh` hook to the recovery initramfs will start the
  persistent `/newroot/opt/op3-wifi/wifi auto` flow after `/newroot` is
  mounted, so recovery starts with wlan0 associated and an IPv4 lease without
  manual setup.
Only variable changed: recovery initramfs network integration. The existing
  Wi-Fi CLI, ath10k modules, firmware, kernel, DTS, DRM/GPU, audio, input,
  browser, and credentials are unchanged.

Build run by project owner: NOT_RUN
Build result: Agent packaging check PASS; owner boot-image repack NOT_RUN
Artifacts and SHA256: `artifacts/initrd-op3-recovery-browser.cpio.gz`
  `8516cdf2e53e8926191cdd25f43f911abb433356aac005454256efa7c5e54bf1`.
  The appended cpio entries include `sbin/run_recovery.sh`,
  `usr/bin/wifi_auto.sh`, and the three A530 firmware files.

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: pending owner boot with the validated OP3-WIFI-001
  bundle and a device-local default profile.

Conclusion: INCONCLUSIVE
Uncertainties:
  - The current branch already contains the validated Wi-Fi scripts, but this
    recovery initrd previously did not contain `usr/bin/wifi_auto.sh`.
  - The hook depends on OP3-WIFI-001's matching modules, firmware, persistent
    CLI, and default profile on sda15. No credential is packaged here.
  - Integration must confirm that automatic Wi-Fi startup does not delay or
    regress the direct-DRM recovery GUI and existing RNDIS/ACM/SSH services.
Recommended next experiment: owner runs the recovery initrd generation,
  repacks the approved 6.12.1 recovery image, boots it without browser, and
  captures `/root/wifi_auto.log`, `wifi current`, `ip -4 addr show wlan0`,
  the default route, filtered ath10k/wlan dmesg, and recovery GUI/SSH status.
```
