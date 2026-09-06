# OP3 recovery/browser integration handoff — Issue #6

```text
Task / GitHub Issue: https://github.com/iamgreatwk/op3/issues/6
Role: Implementation
Baseline commit: fa3c83e89876fa147c762d34bc2701400fa3bcff
Working branch: agent/implementation/recovery-browser-001
Changed files: recovery/recovery_mainline.c and bundled libtsm sources/assets;
  scripts/build-recovery-mainline.sh;
  boot/recovery-browser-test/{sbin/run_recovery.sh,README.md,
  opt/op3-recovery/browser-session.sh};
  boot/browser-test/opt/op3-browser/run.sh;
  boot/pmos-chromium-test/opt/op3-chromium/run.sh;
  scripts/{stage-recovery-rootfs.sh,make-recovery-browser-initrd.sh}
Commit SHA: e902c33, d18ffef, 194ae3f, 0e7ced3, 9f3c465, a458290, d4f9923

Layer: 07 browser, recovery lifecycle integration
Hypothesis tested: A recovery-managed foreground browser session can start
Cog/WPE or Chromium over the existing Weston/Wayland chain and return to the
same recovery PTY/framebuffer UI after browser exit.
Only variable changed: recovery startup and browser-session lifecycle; no
kernel, DTS, GPU/DRM driver, Wi-Fi, or Buildroot browser-content change.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: Agent-only static recovery compile passed:
  out/recovery/recovery_mainline (aarch64 static, SHA256
  93064c326cc95825b58aba592bead4e3712193d9466ee96a5e82beaf30adeb4a);
  package-only validation passed:
  out/recovery/op3-recovery-browser-bundle.tar.gz
  (1fcccae2244fb730f910dfec2618daa96f6e114a336b633072a3bfb762744b20);
  out/recovery/initrd-op3-recovery-browser.cpio.gz
  (01c1b34484e14764d0ef9c7459ece62a69b8e6f96d4bc831da8cce5384918e94).
  These are local validation outputs, not owner-run device artifacts.

Device test run by project owner: 2026-09-06
Device result: FAIL for Cog rendering with the first recovery initrd; recovery
  returned normally after the browser runner exited with rc=1.
Evidence links / log paths: `/newroot/var/log/op3-recovery.log`,
  `/newroot/var/log/op3-browser-session.log`, `/run/op3-weston/weston.log`,
  and `dmesg` captured over SSH from `root@172.16.42.1`. Weston reported
  `failed to initialize egl` / `fatal: failed to create compositor backend`;
  the kernel reported `Direct firmware load for qcom/a530_pm4.fw failed with
  error -2`. The tested initrd had `a530_zap.mbn` but not the three early A530
  GPU firmware files. Battery/charging state was not recorded in this run.

Static verification: `bash -n` passed for all recovery/browser shell entrypoints;
`git diff --check` passed; the recovery binary is statically linked for aarch64;
the initramfs overlay is reproducible across repeated generation;
the staged `browser` command is a regular executable so `init_mainline.sh` can
mirror it into the initramfs PATH (it intentionally skips symlinks). The compile
still reports the pre-existing `draw_statusbar()` `%d` truncation warning from
the imported reference source; no device test occurred.

Conclusion: INCONCLUSIVE
Uncertainties:
  - The first device initrd omitted `a530_pm4.fw`, `a530_pfp.fw`, and
    `a530v3_gpmu.fw2`; commit `d4f9923` adds them to the recovery initrd. The
    corrected initrd still needs a device run.
  - The recovery program and Weston both retain display/input descriptors;
    the new /run/op3-browser.active guard prevents recovery-side reads and
    fb0 commits, but the successful DRM/EGL handoff is not device-proven.
  - Normal browser exit and cleanup are implemented for both runners; a
    SIGKILL that leaves a child compositor outside the supervisor remains an
    operational failure path to observe.
  - Chromium requires the existing Alpine/pmOS chroot at /newroot/pmos;
    Cog requires the existing Buildroot bundle at /newroot/opt/op3-browser.

Recommended next experiment: owner regenerates the corrected initrd, packs the
  6.12.1 image, stages the recovery bundle on sda15, then validates that dmesg
  reports the three A530 firmware files loaded, followed by one Cog session
  and one Chromium session, normal exit back to the recovery prompt, and a
  second start/exit cycle. Record battery/charging state and all persistent
  logs.
```
