# Latest handoff

```text
PROJECT MODE: CLEAN REBUILD
TARGET KERNEL: Linux 7.2 pristine upstream
LEGACY KERNEL: Linux 6.3.1 pmOS-derived
DO NOT BUILD LEGACY unless explicitly requested
HOST TARGET: Ubuntu 26.04 LTS x86_64
DEVICE: OnePlus 3 / MSM8996
```

## Current state (2026-08-29) — DTB IS THE KEY, 7.2 STILL BLOCKED

**All 6.x pmOS kernels boot with the v74 DTB (compiled from 6.3.1-v74full);
7.2 does not boot even with the v74 DTB.**

Test matrix (2026-08-29):
- OP3-BOOT-035: pmOS **v6.16.12 stable** + strict v74 + own DTB → FAIL
- OP3-BOOT-036: pmOS v6.16.12 stable + strict v74 + **v74 DTB** → **PASS**
- OP3-BOOT-037: pmOS **v6.19.5** + strict v74 (UFS=y) + **v74 DTB** → **PASS**
- OP3-BOOT-038: Linux **7.2** + strict v74 + **v74 DTB** → FAIL
- OP3-BOOT-039: Linux 7.2 + strict v74 + v74 DTB + no **LSUI** → FAIL

**Root cause narrowed:**
- The device boots **any 6.x kernel when given the 6.3.1-compiled v74 DTB**.
  6.12/6.16/6.19 all fail with their *own* DTB but boot with v74 DTB.
  This explains all earlier "6.12 boots, 6.16/7.2 fail" confusion: 6.12's
  "boot" image had been packed with the v74 DTB.
- 6.19.5's earlier fastboot failure was additionally caused by
  `CONFIG_SCSI_UFS_QCOM=m`; v74 full config forces `=y`.
- **7.2 fails even with v74 DTB** → 7.2 has an independent kernel-code/config
  issue. Disabling the only 7.2-unique ARMv9 feature `CONFIG_ARM64_LSUI`
  (unsupported on A53) did NOT fix it.

## Next action (7.2, unresolved)

1. Get early-boot log from 7.2 (USB ACM gadget or earlycon) to locate where it
   hangs — 7.2 is the only kernel failing with the v74 DTB.
2. Or diff msm8996-mainline 6.19.5 (boots) vs torvalds mainline 7.2
   (fails) early-boot code (head.S / clk-msm8996 / smem / pinctrl).
3. Parallel line: diff v74 DTB vs 6.16/6.19 own DTB to find the node that
   breaks their own DTB boot (candidate: new `qcom,rpm-proc` structure).

## DRM RGB gate PASS (2026-08-30, layer 04 parallel diagnostic)

The DRM dumb-buffer RGB gate passes on the device with the pmOS 6.12 control
kernel plus the v74 DTB:

```text
image:  artifacts/boot-oneplus3-pmos612-v74dtb-drm-test-60s.img
        f9693a7baf9e1fae5ff8ce27517ac6c246782576b8eb739093a84c690b7a3670
result: connector=33 crtc=106 mode=1080x1920@60, solid colour active, exit 0
owner:  red → green → blue → red, displayed in that order and correct
scope:  layer 04 DRM RGB only. Not EGL, GBM, Wayland, Weston, Cog, WPE, or GPU
        runtime PM, and not a Linux 7.2 acceptance result.
```

Two test-program defects were found and fixed on the way: `d43821a` (second-pass
array pointers in the enumeration ioctls) and `e261c4d` (the
`connector_status_connected` constant). Details are in
`docs/handoff/pmos612-drm-dumb-buffer.md`, rows `OP3-DRM-001` …
`OP3-DRM-005` in `docs/test-matrix.md`. Promoting the result to an accepted
milestone is the Integration role’s decision.

This does not change the Linux 7.2 line below.

## Key artifacts

- 6.16.12 bootable (v74 DTB): `artifacts/boot-oneplus3-pmos616-v74strict-v74dtb.img`
- 6.19.5 bootable (v74 DTB): `artifacts/boot-oneplus3-pmos6195-v74strict-v74dtb.img`
- 6.16.12 worktree: `source/linux-pmos-msm8996-6.16` (tag `v6.16.12-msm8996`)
- v74 DTB: `out/pmos-msm8996-6.3.1-v74full/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb` (73383 bytes, `463b2c72...`)
- Panel driver source: `source/linux-mainline-6.4/drivers/gpu/drm/panel/panel-samsung-s6e3fa5.c`
- 6.19.5 kernel: `source/linux-pmos-msm8996-v6.19.5`, build `out/pmos-msm8996-6.19.5-v74strict`
