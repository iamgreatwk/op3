# Latest handoff

```text
PROJECT MODE: PRODUCT BASELINE
TARGET KERNEL: pmOS MSM8996 Linux 6.12.1 LTS
TARGET COMMIT: 67b0bbc3cbf46bae712a2606a43361756fcbd829
SHELVED RESEARCH: Linux 7.x (physical UART required to resume)
LEGACY KERNEL: Linux 6.3.1 pmOS-derived
DO NOT BUILD LEGACY OR SHELVED 7.x UNLESS EXPLICITLY REQUESTED
HOST TARGET: Ubuntu 26.04 LTS x86_64
DEVICE: OnePlus 3 / MSM8996
```

## Historical 7.x investigation (2026-08-29) — shelved

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

## Next action (7.2, unresolved) — SUPERSEDED 2026-08-30: 7.x shelved, baseline is 6.12.1 (see below)

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

## EGL gate PASS + ACM debug console (2026-08-30, layer 05)

**OP3-EGL-001 passes all three criteria** on the pmOS 6.12 control kernel plus
the v74 DTB:

```text
image:   artifacts/boot-oneplus3-pmos612-v74dtb-egl.img (284936e6…)
         bundle sda15:/opt/op3-egl (e7b1df23…), Mesa 26.0.1
result:  EGL 1.5 on /dev/dri/card0, GL renderer FD530 (freedreno hardware,
         not llvmpipe), OpenGL ES 3.1; kmscube 30 s windows, 1680 frames at
         59.8 fps, exit 0; owner confirmed the rotating cube repeatedly,
         including two image-only boots and three fastboot-boot runs
scope:   layer 05 EGL only. Not Wayland, Weston, Cog, WPE, and not a Linux 7.2
         acceptance result. Promotion is the Integration role's decision.
```

Fixed en route (details in `docs/handoff/pmos612-egl-gate.md`, row
`OP3-EGL-001` in `docs/test-matrix.md`):

- `run.sh` must not export `LD_LIBRARY_PATH` (broke busybox, SIGBUS in
  kmscube), and must feed kmscube stdin from a `sleep` pipe (kmscube treats
  any readable stdin as `user interrupted!` and exited after one frame).
- GPU runtime PM resume hard-resets the device (DTB has dummy GPU regulators);
  the launcher disables A530 runtime PM at boot as a workaround
  (`docs/known-issues.md`, DTB fix is a separate task).

**ACM debug console is now operational end to end**
(`boot-oneplus3-pmos612-v74dtb-egl-acm.img`, `65cac825…`):
the launcher fixes the stale `/dev/ttyGS0` placeholder node, relays
`/dev/kmsg` to it and spawns a debug shell; the host needs the udev rule
`scripts/99-op3-acm.rules` (group `dialout`) and `kai` in `dialout`. Verified:
live kernel log reaches `cat /dev/ttyACM0`, and commands typed on the ACM port
execute on the device (fallback channel when RNDIS/SSH is dead). Usage notes
and pitfalls in `docs/known-issues.md`. Root fix for a real `console=ttyGS0`
(panics, init output) needs a pmOS 6.12 kernel rebuild with
`CONFIG_U_SERIAL_CONSOLE=y` — recorded as a separate task.

This does not change the Linux 7.2 line below.

## Wayland gate PASS (2026-08-30, layer 06)

weston 14 composites on the panel with the GL renderer and an animated Wayland
client (`docs/handoff/pmos612-wayland-weston.md`, row `OP3-WAYLAND-001`):

```text
image:  artifacts/boot-oneplus3-pmos612-v74dtb-weston.img (ea6043cc…)
bundle: artifacts/op3-weston-bundle.tar.gz (0a8163cb…, whole Buildroot target)
result: weston DRM backend + GL renderer (FD530) on DSI-1 1080x1920@60;
        weston-simple-egl spinning triangle visible on the panel; owner
        confirmed; two runs + live session, zero assertions
scope:  layer 06 only. Not the browser, and not a Linux 7.2 acceptance result.
```

Six issues were fixed on the way (loader/module paths, eudev input_id rule,
/usr/libexec helper wrappers, stale weston twin holding DRM master, Mesa
built without the wayland platform) — details in the gate doc.

Next: layer 07 browser gate (Cog + WPE WebKit) — large Buildroot build.

## Browser gate PASS (2026-08-30, layer 07)

cog (WPE WebKit) renders a local HTML page fullscreen on the panel through
WPEBackend-fdo → weston → freedreno (`docs/handoff/pmos612-browser-cog.md`,
row `OP3-BROWSER-001`):

```text
image:  artifacts/boot-oneplus3-pmos612-v74dtb-browser.img (802d803c…)
bundle: artifacts/op3-browser-bundle.tar.gz (bec5ce8b…)
result: cog `wl` platform, fullscreen 1080x1920; page "Loaded successfully";
        CSS colour animation + JavaScript seconds counter advancing
        (owner-confirmed, two runs + deploy-session run)
scope:  layer 07 local-page rendering. Network browsing and multimedia are
        out of scope (gstreamer OFF in the build).
```

The graphics/user-space chain on the pmOS 6.12 control is validated end to
end: **DRM dumb buffer → GPU firmware → EGL → Wayland compositor → browser**.
Ten build/runtime fixes were required en route (all documented in the gate
doc): build parallelism, harfbuzz-icu, Mesa `-Dplatforms=wayland`, Mesa
`legacy-wayland=bind-wayland-display` (diagnosed with a cross-compiled
eglGetProcAddress probe), cog platform name `wl`, `--fullscreen` removal,
ln -sfn bridge gotcha, DejaVu fonts, wpe-webkit-2.0 helper wrappers,
shared-mime-info, and a cog fullscreen-size patch.

Next: the control-baseline bring-up is COMPLETE through layer 07. Remaining
project lines: Linux 7.2 early-boot (last), GPU regulator nodes in DTB,
`CONFIG_U_SERIAL_CONSOLE=y` kernel rebuild.

## Cross test: browser on the 6.3.1 (v100) kernel — PASS (2026-08-30)

The same browser bundle ran unchanged on `6.3.1-msm8996+ #31`
(`artifacts/boot-oneplus3-v100-browser.img`, v100 Image+DTB + browser
initramfs): weston GL renderer FD530, cog `wl` platform, page loaded,
desktop + rendered page owner-confirmed. Caveat: ~452 GPU SMMU context
faults (b40000.iommu, iova=0x0) on 6.3.1's a5xx path — non-blocking,
worth investigating in the parallel project.

**Conclusion: the 6.3.1 parallel project's browser failures were
user-space, not kernel.** The browser bundle + run.sh (fonts, MIME database,
loader discipline, helper wrapping, `wl` platform) transfers directly.
Details in `docs/handoff/pmos612-browser-cog.md` cross-test section.

## PROJECT BASELINE DECISION (2026-08-30): pmOS 6.12.1 LTS — 7.x line SHELVED

**The long-term project kernel is the pmOS 6.12.1 LTS line** (LTS EOL
2028-12-31; already validated end-to-end through the layer-07 browser gate on
this device). Non-LTS 6.19/7.0/7.1/7.2 carry no long-term maintenance and are
out of scope for the product path.

- **7.x line shelved** (owner decision): OP3's LK produces no early-boot
  output without a physical UART, so every 7.x boot is blind (fastboot
  return / no ACM / no pstore). Last data points: OP3-BOOT-040/041 — 7.0-rc1
  fails with BOTH the upstream OP3 DTB and the v74 DTB under the gemini-proven
  recipe; regression window is the 6.19.5 → 7.0-rc1 merge window. Full
  analysis and resume criteria in `docs/handoff/linux70rc1-minimal-ab.md`.
- **source/ reduced** to `linux-mainline-6.12.1`, `linux-pmos-msm8996-6.12`,
  `linux-pmos-msm8996-6.3.1` (+ `buildroot`). Unpushed local work from the
  removed trees (S6E3FA5 driver ports for 6.16/6.19.5, the 7.2 A/B patches)
  is archived in `patches/shelved-7x/` — the reusable asset if a newer LTS
  (e.g. 6.18) is ever picked up again.
- **Baseline retest PASS (OP3-BROWSER-003, 2026-08-30)**: the layer-07 browser
  gate re-runs clean on the pmOS 6.12.1 baseline after the source/out cleanup —
  weston desktop + cog fullscreen animated page owner-confirmed, and for the
  first time with a full ACM kernel-log capture from boot (554 lines,
  `artifacts/console-browser-retest-20260830.log`).
- Still-open project lines (unchanged): GPU regulator nodes in DTB,
  `CONFIG_U_SERIAL_CONSOLE=y` kernel rebuild for a real `console=ttyGS0`.

## Key artifacts

- pmOS 6.12 own-DTB + reproducible-initramfs boot PASS (OP3-BOOT-042/043):
  `artifacts/boot-oneplus3-pmos612-own-dtb-repro-initrd.img`
  (`0b2e85ee…`); source `91df7ccd…`, tree-built DTB `cb29ab65…`, generated
  initramfs `61cf6338…`. The source uses the direct `rpm-glink` topology; it
  replaces the v74 DTB only for the pmOS 6.12 product path. The generated
  initramfs is a reproducible/auditable reserialization of the still-pinned
  historical archive; independent firmware/base-userland provenance remains a
  separate next task. Details: `docs/handoff/op3-dts-rpm-001.md` and
  `docs/handoff/op3-initrd-001.md`.
- 6.16.12 bootable (v74 DTB): `artifacts/boot-oneplus3-pmos616-v74strict-v74dtb.img`
- 6.19.5 bootable (v74 DTB): `artifacts/boot-oneplus3-pmos6195-v74strict-v74dtb.img`
- 6.16.12 worktree: `source/linux-pmos-msm8996-6.16` (tag `v6.16.12-msm8996`)
- v74 DTB: `out/pmos-msm8996-6.3.1-v74full/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb` (73383 bytes, `463b2c72...`)
- Panel driver source: `source/linux-mainline-6.4/drivers/gpu/drm/panel/panel-samsung-s6e3fa5.c`
- 6.19.5 kernel: `source/linux-pmos-msm8996-v6.19.5`, build `out/pmos-msm8996-6.19.5-v74strict`
