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
Commit SHA: e902c33, d18ffef, 194ae3f, 0e7ced3, 9f3c465, a458290, d4f9923,
 3234d5d, 5a73922

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
  6c833a1cc1408dfc9670e5bc258fcb3f77dd75834791f81afaaed3ffe7a519ac);
  package-only validation passed for the current source:
  artifacts/op3-recovery-browser-bundle.tar.gz
  (07a234bcc6a3e18b2e390d098ddfb96e6dab838abe9124e680d90803f14d6b5a);
  out/recovery/initrd-op3-recovery-browser.cpio.gz
  (4741472de06303c37cda67ea48e74c723dab2c9f6f0ff5e8ba144732a2f05de3).
  The packed corrected image is
  out/recovery/boot-oneplus3-pmos612-recovery-browser.img
  (b708e08fa1e6517ba0fcbe07eb5017b1c77fffd602d9c1bd07c62ede93932acf).
  These are local validation outputs, not owner-run device artifacts.

Device test run by project owner: 2026-09-06
Device result: FAIL for Cog rendering in the latest recovery image. Attempt 1
used the first recovery initrd and recovery returned normally after the
browser runner exited with rc=1. The corrected-initrd attempts loaded all
three A530 firmware files but hard-reset during the Cog startup path; the
latest attempt used the early GPU `control=on` workaround and still reset.
Evidence links / log paths: `/newroot/var/log/op3-recovery.log`,
  `/newroot/var/log/op3-browser-session.log`, `/run/op3-weston/weston.log`,
  and `dmesg` captured over SSH from `root@172.16.42.1`. The latest log has a
  persisted `session start` but no runner exit/cleanup. The latest dmesg reports
  successful loading of `a530_pm4.fw`, `a530_pfp.fw`, and `a530v3_gpmu.fw2`,
  `control=on`, and `runtime_status=active`; it has no kernel panic/fault
  record and pstore is empty. Battery state was 91%, charger online/full, so
  low battery is not supported as the cause of this reset.

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
    `a530v3_gpmu.fw2`; commit `d4f9923` adds them to the recovery initrd.
  - The corrected initrd loaded all three files, but GPU runtime resume during
    the later Cog launch still hard-reset the phone. Commit `3234d5d` moves
    `control=on` to the recovery initramfs entrypoint, before runtime suspend.
  - Before `5a73922`, recovery and the PTY shell could retain fb0 references
    while Weston started. Issue #7 commit `a58f166` replaces the recovery
    display path with direct `/dev/dri/card0` KMS and makes the browser handoff
    close the DRM framebuffer/card before Weston. This direct DRM gate has not
    yet been run on the phone.
  - Normal browser exit and cleanup are implemented for both runners; a
    SIGKILL that leaves a child compositor outside the supervisor remains an
    operational failure path to observe.
  - Chromium requires the existing Alpine/pmOS chroot at /newroot/pmos;
    Cog requires the existing Buildroot bundle at /newroot/opt/op3-browser.

Recommended next experiment: owner builds/repackages the current commit,
deploys the current recovery bundle, and boots the image containing `5a73922`
with the phone charged or on a charger. Before launching Cog, verify the early
recovery log says `GPU runtime PM disabled before recovery: control=on`. On
`browser cog`, verify the session log records
`recovery framebuffer released after ...` before any Weston log is inspected.
If stable, verify normal Cog exit returns to the same recovery prompt, then
repeat the cycle and test Chromium. Record battery/charging state and all
persistent logs; do not leave the GPU-on session unattended.
```
