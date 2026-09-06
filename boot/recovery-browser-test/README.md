# OP3 recovery + Wayland browser session (Issue #6)

This overlay restores the persistent `recovery_mainline` program as the
inittab foreground application on the pmOS MSM8996 Linux 6.12.1 line. The
recovery binary and the browser runners stay on sda15; only the small
`sbin/run_recovery.sh` selector is appended to the initramfs.

The owner-supplied `recovery_mainline.c` was ported into `recovery/` with the
matching libtsm sources under `third_party/libtsm/`. Its normal PTY shell gets
the `browser` command from the staged bundle:

```text
browser                  # default engine: chromium
browser chromium [URL]  # Chromium/Alpine Wayland session
browser cog [URL]       # Cog/WPE WebKit Wayland session
```

`browser` sets `/run/op3-browser.active` before starting Weston. While this
flag is live, recovery keeps its framebuffer mapping, PTY, and libtsm state but
does not consume input events or submit fb0 frames. The one-shot browser
runner waits for the selected browser to exit, stops Weston, removes stale
children, and then clears the flag. The original recovery shell prompt and
screen are therefore restored in the same boot.

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

## Scope and evidence

This change does not alter the kernel, DTS, GPU/DRM driver, Wi-Fi, or
Buildroot browser contents. It is not a device PASS until the owner records a
boot with recovery visible, one Cog or Chromium session, a normal browser
exit, a returned recovery prompt, and a second start/exit cycle with battery
or charging state.
