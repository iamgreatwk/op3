# OP3 recovery chin capacitive-key handoff

```text
Task / GitHub Issue: pending owner issue for OP3 recovery chin capacitive keys
Role: Implementation
Baseline commit: 67b0bbc3cbf46bae712a2606a43361756fcbd829
Working branch: agent/implementation/recovery-browser-001
Changed files: patches/pmos612-op3-capkeys/*, kernel/configs/oneplus3-s1302-capkey.fragment
Commit SHA: d1c525f

Layer: formal pmOS 6.12 kernel input (S1302 driver, DTS registration, config fragment)
Hypothesis tested: Registering the separate OnePlus 3 S1302 controller on the
correct BLSP2 I2C bus with its GPIO interrupt/reset and legacy page-2 key
protocol will expose the chin keys as standard EV_KEY devices for recovery.
Only variable changed: S1302 capkey kernel exposure; recovery userspace,
volume GPIOs, tri-state, audio, haptics, Wi-Fi, DRM, and browser are unchanged.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: none

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: static patch checks only; no device evidence yet

Conclusion: INCONCLUSIVE
Uncertainties:
  - The driver/protocol and GPIO wiring are based on historical OP3 evidence
    and must be validated on the formal 6.12 kernel and the physical device.
  - This patch intentionally reports the two chin keys as KEY_APPSELECT (580,
    left/recent) and KEY_BACK (158, right/back). The center fingerprint/home
    button is a separate device and is not claimed here.
  - Recovery already discovers capability-based code 580/158, so no recovery
    binary change is required if the kernel registers this input device.

Recommended next experiment: in a private worktree of the assigned formal
6.12 kernel, apply
`patches/pmos612-op3-capkeys/0001-*.patch` and
`patches/pmos612-op3-capkeys/0002-*.patch`. Do not create an in-tree `.config`
from the host `/boot/config`; copy the validated OP3 base config from
`out/pmos-msm8996-6.12-rpm-glink-own-dtb/.config` to a new external output
directory, merge `kernel/configs/oneplus3-s1302-capkey.fragment` with
`merge_config.sh -O`, and run the owner-authorized kernel build. After booting
the resulting image, verify:

  cat /proc/bus/input/devices
  dmesg | grep -iE 's1302|capkey|i2c|gpio|irq'
  find /dev/input -maxdepth 1 -type c -printf '%f\n'
  evtest /dev/input/eventN

Expected kernel evidence is an `op3-capkey-s1302` device and EV_KEY events
for codes 580 and 158. Recovery should then log
`input: capacitive-keys -> ...`; the owner must report the physical key test
before this can be promoted beyond INCONCLUSIVE.
```
