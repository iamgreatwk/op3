# OP3 recovery direct DRM handoff — Issue #7

```text
Task / GitHub Issue: https://github.com/iamgreatwk/op3/issues/7
Role: Implementation
Baseline commit: fa3c83e89876fa147c762d34bc2701400fa3bcff
Working branch: agent/implementation/recovery-browser-001
Changed files: recovery/recovery_mainline.c; recovery/recovery_drm.c;
  recovery/recovery_drm.h; scripts/build-recovery-mainline.sh
Commit SHA: a58f166, 398ec3a

Layer: 04 DRM, recovery display backend
Hypothesis: Direct `/dev/dri/card0` KMS with an XRGB8888 dumb buffer can
  render the existing recovery UI and provide an explicit DRM release/reopen
  boundary for a later compositor session.
Only variable changed: recovery userspace display backend. Kernel, DTS,
  GPU/DRM driver, input, audio, Wi-Fi, and browser contents are unchanged.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: Agent static aarch64 recovery compile passed:
  out/recovery/recovery_mainline
  242721ba77e89ccc929b094d7e486639063b7ba064810be82f980dcea321b85d
  artifacts/op3-recovery-browser-bundle.tar.gz
  c9d4ffe48f409d111679b76a9bafff7e5065ff5f2981bd4fbe0632fa18b5dcd1

Device test run by project owner: 2026-09-06
Device result: PASS (DRM-only smoke test)
Evidence links / log paths: owner-provided OP3 DRM-only boot capture
  (2026-09-06): `/tmp/fb.log`, `/newroot/var/log/op3-recovery.log`, and
  filtered `dmesg`. Recovery logged `DRM display opened fd=3 connector=33
  crtc=106 1080x1920 pitch=4352`; `/proc/388/fd` showed fd 3 pointing to
  `/dev/dri/card0` and no `/dev/fb0` descriptor. A530 PM4/PFP/GPMU firmware
  loaded. The `pp done time out, lm=2` line was at dmesg 2.097s, before the
  recovery launcher marker at 9.809s, so it is pre-recovery boot evidence and
  does not identify a direct-DRM recovery failure. The controlled handoff
  then logged `DRM display released for browser handoff`, `browser handoff
  ready`, `DRM display opened ...`, and `DRM display restored after browser`;
  both session markers were cleared and PID 388 remained alive. No browser
  was started.

Conclusion: INCONCLUSIVE
Uncertainties:
  - The first smoke test proved the userspace DRM lifecycle but the owner
    reported that the restored UI was not visible. The direct DRM close
    disables the panel/backlight, while the old restore path did not restore
    the saved brightness even though `screen_on` remained true.
  - Commit `398ec3a` saves the active brightness before handoff and explicitly
    restores it after the DRM modeset. This fix has not yet been device tested.
  - The recovery display now owns and closes `/dev/dri/card0`; browser
    handoff behavior is deliberately not part of this gate.
  - The existing GPU runtime-PM workaround remains unchanged; this issue does
    not repair the DTB's dummy GPU regulators.
Recommended next experiment: deploy the bundle from commit `398ec3a` and
  rerun the DRM-only handoff. Confirm `/tmp/fb.log` contains
  `backlight saved before DRM handoff=...` and
  `backlight restored after DRM handoff=...`, then confirm the recovery UI is
  visible and the physical power key still toggles it. Keep browser testing
  out of this gate; it belongs to Issue #6 after the recovery foundation
  issues are complete.
```
