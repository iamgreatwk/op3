# OP3 autofocus G5 high-end clean sweep (2026-09-14)

## Task / GitHub Issue

Continue the IMX298 autofocus investigation after the G5 full-range sweep.

## Role and baseline

- Role: camera userspace/device-test handoff
- Project branch: `agent/implementation/recovery-browser-001`
- Camera kernel source used by the temporary image: nested branch
  `agent/implementation/op3-camera-imx298-001`
- Temporary boot image: `boot-oneplus3-pmos612-recovery-imx298-afpwdm-20260909.img`
- Boot image SHA256: `64ccf213644f517f8c8b8c512869f396cb75c90920f89163fb8385a244dce4e2`

No source, kernel, DTS, initramfs, Buildroot, or flash image was changed for
this experiment.

## Hypothesis and only variable

Hypothesis: the high end of the BU63165GWL VCM range contains the optical focus
position for the current fixed scene.

Only variable changed: VCM position. The scene, exposure (`893`), analogue gain
(`240`), RAW10 mode (`1476x834`), continuous stream, settle delay (`150 ms`),
and frame count (`200`) were held constant.

## Procedure

The phone was freshly booted with `fastboot boot`. The complete camera module
chain was loaded in dependency order using
`scripts/op3-camera-load-modules.sh`. The helper kept one continuous
`STREAMON` active and tested these eight positions:

```text
768, 800, 832, 864, 896, 928, 960, 1023
```

Three RAW10 frames were retained per position after the settle delay. The
device-side plain evidence archive was
`op3-af-g5-high-clean-20260914.tar`, SHA256
`c65864ea8aadd54e564f16ec57080298b7f88dd60e7d62e6eda1736c78b34730`.

## Results

The control path and capture path passed:

```text
result frames_good=200 frames_error=0 focus_result=0 focus_position=-1 focus_cache=-1
G3 result=PASS capture-ready-and-focus-ioctl-returned
```

Every VCM ioctl returned `0`; there were no focused CCI timeout/error
signatures, VFE overflow, CAMSS failure, SMMU fault, or reboot in the
post-test dmesg.

Tenengrad scores were calculated from all pixels, the left-side scene region,
and a central text region. Values are averages of the three retained frames:

| VCM position | Frames | All pixels | Left region | Text region |
|---:|---:|---:|---:|---:|
| 768  | 3 | 21333.377 | 21121.793 | 21792.863 |
| 800  | 3 | 21369.226 | 21097.432 | 21795.725 |
| 832  | 3 | 21344.990 | 21034.104 | 21758.032 |
| 864  | 3 | 21349.207 | 21037.115 | 21767.654 |
| 896  | 3 | 21351.637 | 21040.484 | 21777.068 |
| 928  | 3 | 21330.648 | 20978.454 | 21735.590 |
| 960  | 3 | 21327.973 | 20948.863 | 21726.733 |
| 1023 | 3 | 21284.617 | 20845.129 | 21613.398 |

Position `800` is the numerical maximum for the all-pixel and text-region
metrics, while position `768` is the maximum for the left region. The spread
is small and inconsistent across regions; the recovered display text remains
visibly blurred in the montage. This is not a repeatable optical focus peak.

## Conclusion

**CAPTURE-PASS / OPTICAL-FOCUS-INCONCLUSIVE.** The BU63165GWL position write is
reliably accepted during an active stream, but this experiment does not prove
that the lens moves through a useful optical range or that the driver position
code maps monotonically to lens travel. The earlier same-boot high-end retry
that timed out and produced VFE overflow remains invalid optical evidence.

## Evidence

Persistent raw frames, logs, dmesg snapshots, checksums, and the visual montage
are in:

`artifacts/op3-af-evidence/device-g5-high-clean-sweep-20260914/`

The primary files are:

- `op3-af-g5-high-clean-20260914.log`
- `op3-af-g5-high-clean-20260914-before.dmesg`
- `op3-af-g5-high-clean-20260914-after.dmesg`
- `op3-af-g5-high-clean-20260914-pos-*-frame-*.raw`
- `op3-af-g5-high-clean-sheet.png`
- `SHA256SUMS`

## Recommended next experiment

Do not integrate closed-loop autofocus or claim a calibrated position from this
run. The next isolated step should establish lens motion or actuator mapping:

1. Repeat a clean-boot sweep with a near/far target that contains fine detail,
   retaining the same fixed exposure and gain.
2. Compare the same ROI with a second independent sharpness metric and inspect
   frame-to-frame stability at each position.
3. If the score remains flat, audit the BU63165GWL vendor initialization,
   position encoding, and calibration/park protocol from the downstream source
   or a hardware trace; do not guess-write undocumented registers.
