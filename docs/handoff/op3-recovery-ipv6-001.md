# OP3 recovery IPv6 opt-in — Issue #9

```text
Task / GitHub Issue: https://github.com/iamgreatwk/op3/issues/9
Role: Implementation
Baseline commit: 0d3be23d561ebe663c0cf46f4d748e8408ea1b06
Working branch: agent/implementation/recovery-browser-001
Changed files: boot/wifi/opt/op3-wifi/wifi;
  boot/wifi/initramfs/usr/bin/wifi_auto.sh; boot/wifi/README.md;
  boot/recovery-browser-test/README.md; docs/handoff/latest.md;
  docs/handoff/op3-recovery-ipv6-001.md
Commit SHA: pending implementation commit

Layer: 02 recovery network userspace / IPv6 policy
Previous PASS milestone: Issue #8 recovery initrd includes the validated
  Wi-Fi auto-start hook
Hypothesis: If the recovery Wi-Fi hook disables IPv6 before invoking the
  persistent `wifi auto` command, recovery will retain IPv4 auto-connect while
  IPv6 remains disabled by default; `wifi ipv6 on` will enable IPv6 on demand.
Only variable changed: recovery Wi-Fi IPv6 policy. The kernel, DTS, ath10k
  modules, firmware, IPv4 association/DHCP path, DRM/GPU, audio, input,
  browser, and credentials are unchanged.

Command interface:
  `wifi ipv6 off` disables IPv6 for current and future interfaces;
  `wifi ipv6 on` enables it; `wifi ipv6 status` reports the state.

Build run by project owner: NOT_RUN
Build result: Agent shell/static checks pending final checkpoint; owner boot
  image repack NOT_RUN
Artifacts and SHA256: The existing Issue #8 recovery initrd must be
  regenerated after this source change; SHA256 pending.

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: `/root/boot_mainline.log` including the
  `wifi_auto started` marker; `/root/dmesg_early.txt` or
  `/root/dmesg_rootfs.txt`; `wifi current`; `ip -4 addr show wlan0`;
  `ip -4 route`; and `wifi ipv6 status` before and after the explicit command.

Conclusion: INCONCLUSIVE
Uncertainties:
  - Enabling IPv6 only changes the kernel IPv6 sysctl state; a router
    advertisement may require a reconnect on networks that do not announce
    promptly after the interface is enabled.
  - A kernel built without IPv6 has no IPv6 sysctl tree; `wifi ipv6 off` is a
    successful no-op in that case, while `wifi ipv6 on` reports unavailable.
Recommended next experiment: regenerate the recovery initrd, repack the
  approved 6.12.1 recovery image, boot with the validated Wi-Fi bundle/profile
  on sda15, verify IPv4 plus `ipv6=off`, then run `wifi ipv6 on` and record the
  status and any IPv6 address/route without changing the profile.
```
