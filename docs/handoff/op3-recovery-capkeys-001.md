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

Build run by project owner: 2026-09-06
Build result: PASS
Artifacts and SHA256:
  - kernel config: c3da142eb257c5b0b501f24b91b74c0dba58a16ac6af252f054a4f14540baeb3
  - Image.gz: 389ed53b61cdaa44c301a228d126db84113b6d7626e8201d77d1a20647fc05f2
  - msm8996-oneplus3.dtb: bfb81b236c7e55c2022ae84e4fda6f3e7c0bb602fe91567a542e5e1e6a2fbb76
  - boot image: 9b7f25f549f69e2398516e46504c143eba7c3e2a3d62a2c9e9f0c16aa886e044
    (`artifacts/boot-oneplus3-pmos612-capkey.img`)

Device test run by project owner: 2026-09-06
Device result: PASS
Evidence links / log paths: owner SSH output; `/proc/bus/input/devices`
contains `op3-capkey-s1302` on `event1`, dmesg reports
`S1302 capacitive keys ready (irq=86)`, and `/tmp/fb.log` reports
`input: capacitive-keys -> /dev/input/event1 name=op3-capkey-s1302
codes=580,158,-1` plus `cap=7`. Physical testing then produced
`capacitive code=580 value=1/0` and `capacitive code=158 value=1/0`.

Conclusion: INCONCLUSIVE (device scope supported; Integration acceptance pending)
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
the resulting image, verify. If an earlier in-tree Kconfig run left
`$kernel/.config`, `include/config`, or `include/generated`, remove only those
generated paths first; do not run a broad clean on a shared kernel checkout.

  cat /proc/bus/input/devices
  dmesg | grep -iE 's1302|capkey|i2c|gpio|irq'
  find /dev/input -maxdepth 1 -type c -printf '%f\n'
  evtest /dev/input/eventN

Expected kernel evidence is an `op3-capkey-s1302` device and EV_KEY events
for codes 580 and 158. Kernel registration, recovery discovery, and physical
left/right press and release events are all observed. Integration must review
the evidence before promoting this result.
```
