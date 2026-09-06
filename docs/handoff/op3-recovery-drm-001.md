# OP3 recovery direct DRM handoff — Issue #7

```text
Task / GitHub Issue: https://github.com/iamgreatwk/op3/issues/7
Role: Implementation
Baseline commit: fa3c83e89876fa147c762d34bc2701400fa3bcff
Working branch: agent/implementation/recovery-browser-001
Changed files: recovery/recovery_mainline.c; recovery/recovery_drm.c;
  recovery/recovery_drm.h; scripts/build-recovery-mainline.sh
Commit SHA: a58f166, 398ec3a, 071cc75, eeba948

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
  6ab82320aa87fd6255a203d94077a3e9afd1e7e147d957822b97127545414ac0
  artifacts/op3-recovery-browser-bundle.tar.gz
  484d7db7d4a977bb0b451cb148b50532c83a7eddedd8938817e340c7cb83cb3a

Device test run by project owner: 2026-09-06
Device result: PASS (DRM-only smoke test and visible restore)
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
  both session markers were cleared and PID 388 remained alive. After the
  panel-preserving handoff fix in `eeba948`, the owner confirmed that the
  recovery GUI was visible again after the fake session exited. No browser was
  started.

Conclusion: INCONCLUSIVE
Uncertainties:
  - This DRM-only retest did not start a real compositor/browser and did not
    include a post-handoff filtered dmesg excerpt. Browser ownership and the
    browser return path remain outside this issue.
  - The existing GPU runtime-PM workaround remains unchanged; this issue does
    not repair the DTB's dummy GPU regulators.
Recommended next experiment: Integration records the visible DRM-only result
  and keeps Issue #6 browser testing separate. Before starting Cog/Chrome,
  complete the recovery foundation issues for network, audio, and physical
  input as planned.
```
