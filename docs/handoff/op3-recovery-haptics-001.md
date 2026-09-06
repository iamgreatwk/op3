# OP3 recovery haptics handoff

```text
Task / GitHub Issue: pending owner issue for recovery haptics
Role: Implementation
Baseline commit: 4c4dd4a
Working branch: agent/implementation/recovery-browser-001
Changed files: recovery/recovery_mainline.c
Commit SHA: d4c1e37

Layer: recovery userspace haptics backend
Hypothesis tested: Recovery can drive either a mainline EV_FF haptics input
device or the legacy timed-output/LED sysfs backend when the corresponding
kernel device exists.
Only variable changed: haptics backend selection and write format; no kernel,
DTS, audio, input-key routing, Wi-Fi, DRM, or browser changes.

Build run by project owner: NOT_RUN
Build result: PASS (agent-only recovery userspace compile; owner kernel build not run)
Artifacts and SHA256: `out/recovery/recovery_mainline`, SHA256
`7bb7eeb1b980e1b92b54e6b450922ebfcb7544778fbedc70a945d15db44cc724`

Device test run by project owner: 2026-09-06
Device result: FAIL for kernel haptics exposure. The device input inventory
contains only `pm8941_pwrkey` and the Synaptics touchscreen; no input device
advertising EV_FF is present, so the recovery backend cannot drive hardware
vibration.
Evidence links / log paths: owner SSH output from `/proc/bus/input/devices`
and `/tmp/fb.log`.

Conclusion: INCONCLUSIVE
Uncertainties:
  - The formal 6.12 DTS currently has no `pmi8994_haptics` node and the
    available build configuration may not enable a usable Qualcomm haptics
    device. The userspace change cannot create that kernel device.
  - If the kernel exposes a haptics input device, it must advertise both an
    haptic name and EV_FF; the selected effect is logged before the first
    recovery vibration.

Static verification: `git diff --check` passed and the recovery program built
as a statically linked AArch64 executable. The existing unrelated
`draw_statusbar()` format-truncation warning remains.

Recommended next experiment: restart the recovery process with the committed
binary and confirm `/tmp/fb.log` contains either the selected backend or the
explicit `vibration: no supported backend` line. Then build/boot a kernel image whose assigned kernel
Issue registers the OnePlus 3 haptics device, then inspect `/tmp/fb.log` for
`vibration: input FF -> ...` and verify a short startup/touch vibration. If no
EV_FF device exists, inspect the same log for `vibration: no supported
backend`; do not treat that as a recovery failure until the kernel haptics
node/configuration is present.
```
