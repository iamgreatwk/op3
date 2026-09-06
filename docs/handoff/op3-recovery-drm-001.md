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

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: pending owner DRM-only boot test.

Conclusion: INCONCLUSIVE
Uncertainties:
  - The already validated raw KMS probe is reused, but recovery's dynamic
    DIRTYFB update path and repeated display reinitialization need device
    evidence on the DSI command-mode panel.
  - The recovery display now owns and closes `/dev/dri/card0`; browser
    handoff behavior is deliberately not part of this gate.
  - The existing GPU runtime-PM workaround remains unchanged; this issue does
    not repair the DTB's dummy GPU regulators.
Recommended next experiment: owner deploys the current recovery bundle,
  builds/repackages the current recovery image with the existing approved
  6.12.1 kernel/DTB/initrd, and runs recovery without starting a browser.
  Capture the recovery display log, `/dev/dri` state, connector/CRTC/mode,
  dmesg DRM/MDP lines, and battery/charging state. PASS requires a stable
  recovery UI through direct KMS with no recovery open of `/dev/fb0`, followed
  by a controlled recovery DRM release/reopen smoke test on the same boot.
```
