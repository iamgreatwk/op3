# OP3 recovery physical-input handoff

```text
Task / GitHub Issue: pending owner issue for recovery physical input
Role: Implementation
Baseline commit: fced25f
Working branch: agent/implementation/recovery-browser-001
Changed files: recovery/recovery_mainline.c
Commit SHA: pending

Layer: recovery userspace input routing
Hypothesis tested: Recovery can reliably attach to the physical input devices
when it discovers them by capabilities instead of event number or a single
device-name spelling.
Only variable changed: recovery input-device discovery and event diagnostics;
no kernel, DTS, audio, haptics, Wi-Fi, DRM, or browser changes.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: none

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: none

Conclusion: INCONCLUSIVE
Uncertainties:
  - The formal 6.12 OnePlus 3 DTS currently exposes the PM8941 power key,
    but does not register the board's volume GPIOs as gpio-keys.
  - Historical OnePlus 3 DTS evidence maps PMIC GPIO3 to volume-up and GPIO2
    to volume-down. The kernel-side DTS change must be made and tested on its
    assigned kernel branch; it was not changed here because that checkout is
    another agent's branch with existing work.
  - The historical tri-state and S1302 capacitive-key devices are downstream
    drivers and are not present in the formal 6.12 source. This recovery change
    only consumes their standard EV_KEY events if a kernel driver provides them.

Recommended next experiment: owner/integration assigns a kernel DTS issue to
register the OnePlus 3 volume GPIOs as a standard gpio-keys device, builds the
resulting 6.12 kernel/DTB, and boots it with this recovery binary. Verify
`/tmp/fb.log` reports `input: volume -> ...` and then verify both keys produce
`input: volume code=115/114 value=1` while the recovery console is visible.
Record `/proc/bus/input/devices`, the exact event device names, and the kernel
DTB commit before addressing tri-state, capacitive keys, audio, or vibration.
```
