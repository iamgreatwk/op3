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
Commit SHA: e902c33, d18ffef, 194ae3f, 0e7ced3, 9f3c465, a458290

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
  (af2445f01f7fe814c7e0955331dbe494af90c0e85f64e44aeb6f0fba95f931a0).
  These are local validation outputs, not owner-run device artifacts.

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: No device test occurred. Owner must record
  battery/charging state and boot/recovery/browser/post-exit logs.

Static verification: `bash -n` passed for all recovery/browser shell entrypoints;
`git diff --check` passed; the recovery binary is statically linked for aarch64;
the initramfs overlay is reproducible across repeated generation;
the staged `browser` command is a regular executable so `init_mainline.sh` can
mirror it into the initramfs PATH (it intentionally skips symlinks). The compile
still reports the pre-existing `draw_statusbar()` `%d` truncation warning from
the imported reference source; no device test occurred.

Conclusion: INCONCLUSIVE
Uncertainties:
  - The recovery program and Weston both retain display/input descriptors;
    the new /run/op3-browser.active guard prevents recovery-side reads and
    fb0 commits, but the DRM handoff is not device-proven.
  - Normal browser exit and cleanup are implemented for both runners; a
    SIGKILL that leaves a child compositor outside the supervisor remains an
    operational failure path to observe.
  - Chromium requires the existing Alpine/pmOS chroot at /newroot/pmos;
    Cog requires the existing Buildroot bundle at /newroot/opt/op3-browser.

Recommended next experiment: owner builds/packs the 6.12.1 image with
  out/recovery/initrd-op3-recovery-browser.cpio.gz, stages the recovery
  bundle on sda15, then validates recovery startup, one Cog session and one
  Chromium session, normal exit back to the recovery prompt, and a second
  start/exit cycle. Record battery/charging state and all persistent logs.
```
