# OP3 CAMSS/VFE stream-teardown diagnostic (2026-09-14)

Task: OP3 camera same-boot reopen failure follow-up
Role: camera kernel implementation
Layer: CAMSS/VFE only

## Candidate

The camera kernel worktree is
`source/linux-pmos-msm8996-6.12-camera-imx298` on branch
`agent/implementation/op3-camera-imx298-001`.

Diagnostic source commit:
`36118b36d58d7235ab9a1cef9840ae64cdfcf526`

This commit adds diagnostic messages only. It does not change the stream,
power, buffer, DRM, DTS, rootfs, or Buildroot behavior. The messages record:

- each sub-device's `s_stream(0)` order and return code;
- Gen1 VFE output state, write-master count, stream count, and disable result;
- VFE power count, stream count, `was_streaming`, and halt result;
- VFE output buffer pointers during buffer flush.

The purpose is to distinguish a failed VFE stop, an incomplete pipeline stop,
and a stale output state after the first capture. The device test completed on
2026-09-14: the first AF session captured `8/8` frames, while the second
same-boot session timed out at frame 0 with repeated `VFE0 rdi0 overflow`.
Every recorded VFE and sub-device stop/power/halt return was 0. The overflow
was emitted by the ISPIF interrupt handler about 150 ms after the first
power-off, so the diagnostic points to stale ISPIF state rather than a failed
VFE stop.

## Build boundary

Build only the CAMSS module against the matching prepared camera-kernel output;
do not rebuild Buildroot or the recovery rootfs for this diagnostic.

```sh
project=/home/kai/src/oneplus3-mainline
kernel=$project/source/linux-pmos-msm8996-6.12-camera-imx298
kout=$project/out/pmos-msm8996-6.12-camera-imx298-camss-teardown-diag
base=$project/out/pmos-msm8996-6.12-camera-imx298-cci400
fullout=$project/out/pmos-msm8996-6.12-recovery-audio-full-s1302-poll-drm100

mkdir -p "$kout"
cp "$base/.config" "$kout/.config"
make -C "$kernel" O="$kout" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- \
  CC=aarch64-linux-gnu-gcc-11 olddefconfig modules_prepare

ln -s "$fullout/vmlinux.o" "$kout/vmlinux.o"

make -C "$kernel" O="$kout" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- \
  CC=aarch64-linux-gnu-gcc-11 \
  drivers/media/common/videobuf2/videobuf2-common.ko \
  drivers/media/common/videobuf2/videobuf2-dma-sg.ko \
  drivers/media/common/videobuf2/videobuf2-memops.ko \
  drivers/media/common/videobuf2/videobuf2-v4l2.ko \
  drivers/media/mc/mc.ko \
  drivers/media/v4l2-core/v4l2-async.ko \
  drivers/media/v4l2-core/v4l2-fwnode.ko \
  drivers/media/v4l2-core/videodev.ko \
  drivers/media/platform/qcom/camss/qcom-camss.ko

sha256sum "$kout/drivers/media/platform/qcom/camss/qcom-camss.ko"
```

The module must be loaded only after a fresh `fastboot boot`. Do not unload
CAMSS/CCI in a running session. On the device, load the diagnostic
`qcom-camss.ko` in the same dependency order as the existing camera loader,
then load `bu63165gwl.ko` and `imx298.ko`; verify the module SHA256 before
`insmod`.

The host-only module build passed. The versioned test bundle is
`artifacts/op3-camera-camss-teardown-diag-bundle-20260914-v2.tar.gz` with
SHA256
`064f6bceb7703c0ffb1ac097332b4955128ada8b48decb410babedb793f44fe9`.
It contains 12 modules. The diagnostic CAMSS module hash is
`b92933dbb649c4094d908f13de67157344ec361b894b838ac920fbe39535c874` and
the current-branch CCI module hash is
`8e7b5159271ce46414a56335990ae2037bca32509f4f855da3073275e7070223`.
The IMX298 and BU63165GWL modules are the previously validated unchanged AF
modules, with hashes recorded in the bundle's `MODULES.sha256`.

The CAMSS module was built from output directory
`out/pmos-msm8996-6.12-camera-imx298-camss-teardown-diag-modules-20260914`;
the standalone CCI module was built from
`out/pmos-msm8996-6.12-camera-imx298-cci-diag-20260914`.

## Test

Run the known-good AF helper twice in one boot, without DRM preview and without
module unload. Save the complete `dmesg` and both helper logs. The required
diagnostic lines are:

```sh
dmesg | grep -iE \
  'stream-off|power-off|flush line|VFE0 rdi0 overflow|sof timeout|reg update timeout|halt timeout|smmu|context fault'
```

Expected comparison:

| Session | Required result |
| --- | --- |
| First | `8/8` frames, normal stop sequence |
| Second | Determine the first failed stop/state transition; do not accept a frame timeout alone |

This candidate is diagnostic only. It must not be promoted as a fix. The next
isolated candidate calls the existing `ispif_reset()` before the final ISPIF
clock/runtime-power shutdown, so the post-stop overflow can be tested without
changing VFE, sensor, DTS, DRM, or userspace behavior.

## ISPIF power-off reset candidate

Nested-kernel source commit:
`3ef48e38df32` (`media: camss: reset ISPIF before power-off`). This is the
only behavior change in the candidate: when ISPIF's power reference reaches
zero, it invokes the existing `ispif_reset()` for the active VFE before
disabling ISPIF clocks and runtime power. A reset error is logged but does not
skip the normal clock/runtime-power shutdown. No VFE, sensor, DTS, DRM,
initramfs, or Buildroot file changed.

The host-built replacement module bundle is
`artifacts/op3-camera-ispif-reset-bundle-20260914.tar.gz` with SHA256
`47b285f8619f7f3ff2857f98eef631c8bd8bece65df5bd3399b22c462388824a`. It
contains the changed `qcom-camss.ko`, its V4L2/VB2/MC dependencies, and the
previously validated CCI, IMX298, and BU63165GWL modules. The archive was
verified with `gzip -t` and all twelve entries passed `MODULES.sha256`.

This candidate has not yet been tested on the phone. The required test is a
fresh `fastboot boot`, explicit module load, then two consecutive AF helper
runs without DRM preview or module unload. PASS for this experiment is both
runs capturing `8/8` frames with no post-stop ISPIF overflow; a reset timeout,
frame timeout, or any reboot is FAIL and must be retained in the handoff log.
