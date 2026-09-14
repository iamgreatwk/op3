# OP3 autofocus G5 checkerboard sweep (2026-09-14)

## Scope

This was a userspace/device-only optical test after a clean `fastboot boot`.
The temporary boot image and complete camera module chain were unchanged from
the previous G5 tests. No kernel, DTS, initramfs, Buildroot, or flash image was
modified.

The test target is the tracked 1920x1080 pure black/white checkerboard:

- PNG: `test-patterns/op3-af-checkerboard.png`
- SVG: `test-patterns/op3-af-checkerboard.svg`
- PNG SHA256: `9378e47e409ee10048767bfa2fd2195e337c98d0111e1a1c96b0338c9ecd71c0`

The host copy used on the display was `/home/kai/桌面/OP3-AF-checkerboard.png`
with the same SHA256.

## Hypothesis and fixed conditions

Hypothesis: a high-contrast target will expose a repeatable sharpness maximum
if the BU63165GWL VCM position command produces useful optical lens travel.

The scene was held at approximately 0.5 m and the VCM position was the only
scan variable. Exposure was `893`, analogue gain was `240`, settle time was
`150 ms`, and one continuous RAW10 `1476x834` stream captured 200 frames. The
tested positions were:

```text
0, 128, 256, 384, 512, 768, 896, 1023
```

Three frames were retained at each position.

## Device result

The capture and VCM control paths passed:

```text
result frames_good=200 frames_error=0 focus_result=0 focus_position=-1 focus_cache=-1
G3 result=PASS capture-ready-and-focus-ioctl-returned
```

All eight VCM writes returned `ioctl_rc=0`. The focused CCI/VFE/CAMSS/SMMU
error scan was empty; there was no camera timeout, overflow, or reboot during
the run.

Tenengrad and Laplacian-variance scores below are averages of three RAW10
frames. They are intentionally reported as within-run comparisons, not as an
absolute focus scale:

| VCM position | Frames | Tenengrad all | Tenengrad centre | Laplacian all | Laplacian centre |
|---:|---:|---:|---:|---:|---:|
| 0    | 3 | 4337.170 | 4879.500 | 34048.344 | 38310.039 |
| 128  | 3 | 4342.074 | 4885.869 | 34086.648 | 38359.730 |
| 256  | 3 | 4347.486 | 4891.425 | 34131.266 | 38405.919 |
| 384  | 3 | 4350.113 | 4894.478 | 34152.171 | 38430.083 |
| 512  | 3 | 4352.762 | 4898.375 | 34174.392 | 38462.009 |
| 768  | 3 | 4351.189 | 4896.707 | 34161.357 | 38448.003 |
| 896  | 3 | 4351.641 | 4897.048 | 34165.573 | 38451.139 |
| 1023 | 3 | 4348.187 | 4893.455 | 34140.823 | 38425.224 |

Both full-frame and centre-region metrics peak at `512`, but the full-range
spread is only about `0.36%`, and the visual montage shows the checkerboard
occupying only part of the captured field. This is too small and too dependent
on the selected region to prove physical lens movement or a usable optical
focus peak.

## Conclusion

**CAPTURE-PASS / OPTICAL-FOCUS-INCONCLUSIVE.** The run confirms stable frame
capture and accepted VCM transactions under a high-contrast target. It does
not yet justify selecting position `512` as focus or implementing closed-loop
autofocus.

## Persistent evidence

Raw frames, before/after dmesg, the helper log, montage, and `SHA256SUMS` are
stored in:

`artifacts/op3-af-evidence/device-g5-checkerboard-sweep-20260914/`

The source-side plain device archive was
`/tmp/op3-af-checkerboard-20260914.tar`, SHA256
`4f24c82dffa183fddb662c2ab92479d89ce4bbc35031f339bb7b8333375ae4be`.

## Next step

Before changing the driver, repeat the same scan only after the checkerboard
fills the camera field and is centred in the optical axis. Use a fixed ROI
around the board and retain the same exposure, gain, settle time, and continuous
stream. If the peak remains flat, stop optical sweeps and audit the
BU63165GWL vendor initialization, position encoding, and calibration/park
protocol. Do not guess-write undocumented actuator registers.
