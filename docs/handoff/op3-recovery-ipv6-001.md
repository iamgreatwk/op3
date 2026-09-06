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
Commit SHA: cf6675c

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
Build result: Agent shell/static checks PASS; owner boot-image repack NOT_RUN
Artifacts and SHA256: `artifacts/initrd-op3-recovery-browser.cpio.gz`
  `e9f7ffd3555c7d0796cee002f6ec58e83546dc8871c91a72d0c4d8d49ee7ea2b`.
  The appended entries include `usr/bin/wifi_auto.sh` with the IPv6-off
  preamble, the recovery selector, and the three A530 firmware files.

Device test run by project owner: 2026-09-06
Device result: First boot attempt failed before the Wi-Fi driver stage because
  the deployed sda15 CLI was the earlier bundle and lacked `wifi ipv6`. After
  deploying the replacement bundle, the owner manually connected the saved
  profile successfully: `wpa_state=COMPLETED`, SSID `1106`, IPv4
  `192.168.1.5/24`, default route via `192.168.1.1`, and
  `ipv6=off all=1 default=1 wlan0=1`. Automatic post-reboot validation is
  still pending.
Evidence links / log paths: owner-provided `/root/boot_mainline.log`,
  `wifi current`, `ip -4 addr show wlan0`, `ip -4 route`, filtered dmesg, and
  `/tmp/fb.log`; the relevant evidence is also recorded in the Issue #9
  comment. Replacement bundle:
  `artifacts/op3-wifi-bundle-ipv6.tar.gz`
  `eeabbb20f0f8331fb220252c77acf52f1b0fabe2919dc5197c45690421994654`.

Conclusion: INCONCLUSIVE
Uncertainties:
  - The first device run used a stale persistent CLI; the replacement bundle
    must be deployed before judging the IPv6 policy or IPv4 auto-connect.
  - Enabling IPv6 only changes the kernel IPv6 sysctl state; a router
    advertisement may require a reconnect on networks that do not announce
    promptly after the interface is enabled.
  - A kernel built without IPv6 has no IPv6 sysctl tree; `wifi ipv6 off` is a
    successful no-op in that case, while `wifi ipv6 on` reports unavailable.
Recommended next experiment: reboot the existing recovery image with the
  replacement bundle and saved default profile on sda15. Verify that
  `/root/boot_mainline.log` records `wifi_auto started`, then collect
  `wifi current`, IPv4 address/route, and `wifi ipv6 status` without manually
  running `wifi connect`. Finally run `wifi ipv6 on` and record the status and
  any IPv6 address/route without changing the profile.
```
