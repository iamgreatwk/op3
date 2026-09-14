# OP3 CAMSS/VFE stream-teardown diagnostic (2026-09-14)

Task: OP3 camera same-boot reopen failure follow-up
Role: camera kernel implementation
Layer: CAMSS/VFE only

## Candidate

The camera kernel worktree is
`source/linux-pmos-msm8996-6.12-camera-imx298` on branch
`agent/implementation/op3-camera-imx298-001`.

Source commit:
`36118b36d58d7235ab9a1cef9840ae64cdfcf526`

This commit adds diagnostic messages only. It does not change the stream,
power, buffer, DRM, DTS, rootfs, or Buildroot behavior. The messages record:

- each sub-device's `s_stream(0)` order and return code;
- Gen1 VFE output state, write-master count, stream count, and disable result;
- VFE power count, stream count, `was_streaming`, and halt result;
- VFE output buffer pointers during buffer flush.

The purpose is to distinguish a failed VFE stop, an incomplete pipeline stop,
and a stale output state after the first capture. The observed failure remains:
first AF session `8/8` frames, second same-boot session frame-0 timeout with
`VFE0 rdi0 overflow`.

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

This candidate is diagnostic only. It must not be promoted as a fix. If the
logs show a clean stop but the second run still overflows, the next isolated
change should target VFE hardware reset/state reinitialization. If a stop
return or timeout is exposed, repair that specific teardown path first.
