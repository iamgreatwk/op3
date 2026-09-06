# OP3 recovery haptics handoff

```text
Task / GitHub Issue: pending owner issue for recovery haptics
Role: Implementation
Baseline commit: 4c4dd4a
Working branch: agent/implementation/recovery-browser-001
Changed files: recovery/recovery_mainline.c
Commit SHA: pending

Layer: recovery userspace haptics backend
Hypothesis tested: Recovery can drive either a mainline EV_FF haptics input
device or the legacy timed-output/LED sysfs backend when the corresponding
kernel device exists.
Only variable changed: haptics backend selection and write format; no kernel,
DTS, audio, input-key routing, Wi-Fi, DRM, or browser changes.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: none

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: none

Conclusion: INCONCLUSIVE
Uncertainties:
  - The formal 6.12 DTS currently has no `pmi8994_haptics` node and the
    available build configuration may not enable a usable Qualcomm haptics
    device. The userspace change cannot create that kernel device.
  - If the kernel exposes a haptics input device, it must advertise both an
    haptic name and EV_FF; the selected effect is logged before the first
    recovery vibration.

Recommended next experiment: build/boot a kernel image whose assigned kernel
Issue registers the OnePlus 3 haptics device, then inspect `/tmp/fb.log` for
`vibration: input FF -> ...` and verify a short startup/touch vibration. If no
EV_FF device exists, inspect the same log for `vibration: no supported
backend`; do not treat that as a recovery failure until the kernel haptics
node/configuration is present.
```
