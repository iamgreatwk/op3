# OP3 recovery + Wayland browser session (Issue #6)

This overlay restores the persistent `recovery_mainline` program as the
inittab foreground application on the pmOS MSM8996 Linux 6.12.1 line. The
recovery binary and the browser runners stay on sda15; the small
`sbin/run_recovery.sh` selector, the validated post-`/newroot` Wi-Fi
auto-start hook, and the three A530 GPU firmware files needed during early
kernel probe are appended to the initramfs.

The owner-supplied `recovery_mainline.c` was ported into `recovery/` with the
matching libtsm sources under `third_party/libtsm/`. Its normal PTY shell gets
the `browser` command from the staged bundle:

```text
browser                  # default engine: chromium
browser chromium [URL]  # Chromium/Alpine Wayland session
browser cog [URL]       # Cog/WPE WebKit Wayland session
```

`browser` sets `/run/op3-browser.active` before starting Weston. Recovery owns
`/dev/dri/card0` directly through its KMS backend, keeps its PTY/libtsm state,
closes its DRM framebuffer and card fd, then writes
`/run/op3-browser.recovery-ready`. The session supervisor waits for that marker
before starting Weston, so the browser gets an explicit DRM-master handoff.
The one-shot browser runner waits for the selected browser to exit, stops
Weston, removes stale children, and then clears the flag; recovery reacquires
card0, re-modesets, and redraws the same UI on the same boot.

The direct DRM recovery backend is tracked independently in Issue #7. This
browser session remains unvalidated until the DRM-only recovery gate passes.

The recovery initramfs launcher now includes the validated
`/usr/bin/wifi_auto.sh` hook from OP3-WIFI-001. After `/newroot` is mounted by
the established initramfs flow, it calls `/newroot/opt/op3-wifi/wifi auto`;
the matching modules/firmware, persistent CLI, and default profile must be
staged on sda15 as described in `boot/wifi/README.md`. No credentials are
included in this initrd.

The recovery initramfs launcher currently keeps the A530 GPU runtime active
from boot because this DTB exposes dummy `vdd`/`vddcx` regulators; allowing the
GPU to suspend before a later browser launch can make the runtime resume reset
the SoC. This is a power trade-off until the DTB regulator fix is available.

## Owner build and staging

The following commands prepare the persistent payload and derived initramfs;
the agent must not run the device build/flash/test:

```sh
scripts/build-recovery-mainline.sh
scripts/stage-recovery-rootfs.sh
scripts/make-recovery-browser-initrd.sh \
  artifacts/initrd-op3-firmware-provenance-v2.cpio.gz \
  artifacts/initrd-op3-recovery-browser.cpio.gz
```

Deploy the tarball to the already mounted `/newroot` using the device's
BusyBox-compatible extraction path, then pack the owner-approved boot image
with `artifacts/initrd-op3-recovery-browser.cpio.gz`. Existing sda15 browser
bundles must be present at `/newroot/opt/op3-browser` and, for Chromium, at
`/newroot/opt/pmos` or `/newroot/pmos` as used by the current Chromium runner.
The host checkout must also contain `artifacts/a530-firmware/`; the initrd
script stages `a530_pm4.fw`, `a530_pfp.fw`, and `a530v3_gpmu.fw2` under
`lib/firmware/qcom/`.

## Scope and evidence

Issue #8 only integrates the validated Wi-Fi auto-start hook into the recovery
initramfs; it does not alter the Wi-Fi CLI, modules, firmware, kernel, DTS,
GPU/DRM driver, audio, input, or Buildroot browser contents. It is not a
device PASS until the owner records automatic association/DHCP, a visible
recovery GUI, and the existing RNDIS/ACM/SSH services on the same boot. A real
Cog or Chromium session remains a separate Issue #6 test.
