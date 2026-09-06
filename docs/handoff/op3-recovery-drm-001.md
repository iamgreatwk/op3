# OP3 recovery direct DRM handoff — Issue #7

```text
Task / GitHub Issue: https://github.com/iamgreatwk/op3/issues/7
Role: Implementation
Baseline commit: fa3c83e89876fa147c762d34bc2701400fa3bcff
Working branch: agent/implementation/recovery-browser-001
Changed files: recovery/recovery_mainline.c; recovery/recovery_drm.c;
  recovery/recovery_drm.h; scripts/build-recovery-mainline.sh
Commit SHA: a58f166

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
  891b025bf6dbe810a8b5dba797c7592e440a66800ac77c93ad493fdbe3c28fdd
  artifacts/op3-recovery-browser-bundle.tar.gz
  3c851b7bf9a3bce275d8ecdbb8c28c97238ec974bb616348d700a8c5f69ce8fa

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
  - The owner capture does not include a post-handoff filtered dmesg excerpt
    or a visual report of the restored UI; the smoke-test log proves the
    userspace lifecycle and ioctl path, but not the final Integration verdict.
  - The recovery display now owns and closes `/dev/dri/card0`; browser
    handoff behavior is deliberately not part of this gate.
  - The existing GPU runtime-PM workaround remains unchanged; this issue does
    not repair the DTB's dummy GPU regulators.
Recommended next experiment: Integration reviews this DRM-only evidence and
  promotes Issue #7 only after confirming the restored UI and post-handoff
  dmesg. Keep browser testing out of this gate; it belongs to Issue #6 after
  the recovery foundation issues are complete.
```
