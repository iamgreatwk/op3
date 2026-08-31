# Decisions

## 2026-08-26 — Adopt Linux 7.2 clean-rebuild baseline

The default target is Linux v7.2 pristine upstream. The pmOS MSM8996 6.3.1
tree is retained solely as legacy evidence and may not be built or patched
unless the task explicitly permits legacy work.

## 2026-08-26 — Compilation boundary

The project owner runs every kernel and large userspace compilation. Agent work
may prepare and review source, configuration, scripts, commands, and reports,
but may not start those builds.

## 2026-08-30 — boot.img size limit: keep the initramfs minimal

The OnePlus 3 bootloader imposes a size limit on `boot.img`, so the initramfs
must stay small. Large test payloads belong on the persistent sda15 root
filesystem, not inside the boot image.

Rules that follow:

- The appended initramfs overlay carries only the launcher and files the kernel
  needs before userspace is usable, for example GPU firmware. Nothing bulky goes
  there.
- Test binaries and libraries (Mesa/EGL/GBM/libdrm, Wayland, WPE, and any test
  program) are staged on sda15, for example under `/newroot/opt/…`, and run from
  there with the needed `LD_LIBRARY_PATH`.
- Logs are written to `/newroot/var/log/…` so they survive reboots; the
  initramfs copy is session-local scratch.
- A test that needs a large userspace stack must not be packaged into the boot
  image; stage it on sda15 first and change only the launcher.

For MSM8996 firmware specifically, keep only firmware requested before
`/newroot` is usable in the initramfs: ADSP, SLPI, and the small Adreno zap
and GPU microcode files.  Keep deferred module firmware on sda15/rootfs:
modem plus MBA, Venus, and ath10k Wi-Fi firmware.  A placement change must
also prove that the relevant module is first loaded after the root filesystem
and its firmware path are available; moving files alone is not a valid test.

## 2026-08-30 — Adopt pmOS MSM8996 Linux 6.12.1 LTS product baseline

The project owner selected pmOS MSM8996 Linux v6.12.1 LTS, commit
`67b0bbc3cbf46bae712a2606a43361756fcbd829`, as the formal OnePlus 3 product
baseline. It has device evidence through DRM RGB, A530 EGL, Weston Wayland,
and Cog/WPE browser rendering (OP3-BROWSER-003).

Linux 7.x is shelved, not a default alternative baseline. Its early-boot
failure remains historical evidence and may be resumed only in an explicitly
authorised `SHELVED-7X` task with physical MSM8996 UART evidence.
