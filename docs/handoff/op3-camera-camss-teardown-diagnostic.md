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

The fresh-boot phone test completed on 2026-09-14. The candidate reset did not
report an error, and the first AF session captured `8/8` frames, but the
second session still timed out at frame 0. The same post-stop ISPIF overflow
started about 160 ms after the first power-off. This candidate is **FAIL** for
the repeat-open hypothesis. The next isolated candidate adds a VFE hardware
reset after the final VFE halt and before VFE clocks/runtime power are shut
down.

## VFE reset candidate

Nested-kernel source commit: `7c4f984e08cf` (`media: camss: reset VFE after
final halt`). This candidate added one VFE hardware reset after the final halt
and before VFE clock/runtime-power shutdown. It did not change the sensor,
ISPIF, DTS, DRM, rootfs, or Buildroot. The host bundle was
`artifacts/op3-camera-vfe-reset-bundle-20260914.tar.gz`, SHA256
`b25689e65bde54fc59d1fd613d800c58ddde505711835dc87f36cc8e9c43e4b4`, with
CAMSS module SHA256
`fc7e9a3a5396c73640621b5ae0b7c148f0030f170e4c3c65020bdb9464a0bb71`.

On a fresh boot, AF1 captured `8/8`; the VFE reset returned 0, but AF2 timed
out at frame 0 and the overflow appeared about 140 ms after power-off. The
candidate was reverted as nested-kernel commit `40bbefb43baa`. Result: **FAIL**.

## ISPIF immediate-stop candidate

Nested-kernel source commit: `66bdf7c21d81` (`media: camss: stop ISPIF
interface immediately`). It changed only the ISPIF stop path to issue an
immediate interface stop. The test bundle was
`artifacts/op3-camera-ispif-immediate-bundle-20260914.tar.gz`, SHA256
`3b935a38f2b3cb73c99fc7588e59b5b51b7da65c0f044c55de52b3b5f2150313`, with
CAMSS module SHA256
`448af73a667600dee64fc2d73fe8f5e6ae8b3a97b54bd55c3ffebec2dc6500e1`.

On a fresh boot, AF1 captured `8/8`, but AF2 timed out at frame 0 and the
overflow appeared about 150 ms after power-off. The candidate was reverted as
`a0741d40cbdf`. Result: **FAIL**.

## Source-first teardown-order candidate

Nested-kernel source commit: `04bc53ae836e`, which changed the teardown order
to stop from the sensor toward VFE. It was reverted as `088e1b1c48aa` after a
fresh-boot test. AF1 captured `8/8`, but teardown produced ISPIF stop timeouts
and VFE SOF/reg-update timeouts; AF2 then stopped at frame 0. The test bundle
was `artifacts/op3-camera-stop-source-first-bundle-20260914.tar.gz`, SHA256
`40a1799fc6d040d2c2e2f70d9ae85a80e24c78cebce289851428493f1d4d8fb4`, with
CAMSS module SHA256
`6287f27377d17cdc99b8b14c190ac7509ccf9ece1093986db7692d64115f4bd9`.
Result: **FAIL**.

## ISPIF register diagnostic

Nested-kernel commit `5a2b9b6be870` logs ISPIF status, masks, input select,
commands, RDI status, and clock mux before/after stream-off, cleanup, and
power-off. The diagnostic bundle is
`artifacts/op3-camera-ispif-teardown-diag-bundle-20260914.tar.gz`, SHA256
`76d35e5dd6a3f7e10b03594cb88d80771050b474d1f843ba5cefe179c1111fed`, with
CAMSS module SHA256
`3bc2414302033482595bcef6777d866c1cfc48d40d3b2fbb352912d9fe861a8b`.

The fresh-boot test captured `8/8` in AF1 and 0 frames in AF2. After the
first teardown the key state was `status0=0000c000`, `mask0=08000000`; the
later overflow preceded the failed second stream. This commit is retained as
the current diagnostic baseline and changes no behavior.

## ISPIF IRQ-quiesce candidate

Nested-kernel commit `54f246e8d5cd` masked ISPIF IRQs, cleared status, and
issued a memory barrier before power-off. It was reverted as
`22519f46a62c`. The bundle was
`artifacts/op3-camera-ispif-quiesce-bundle-20260914.tar.gz`, SHA256
`6b255db1b0cd28f9176c80a4f81e11780ab6cc9aa402cc059b7338f49d475c6e`, with
CAMSS module SHA256
`93f9b9158f0b2d6203cd3d2d48b43affff57154729537f072018b61e6995f029`.

AF1 captured `8/8`; the post-quiesce registers were all zero, but AF2 still
timed out at frame 0 and the overflow persisted. Result: **FAIL**.

## ISPIF all-immediate-disable candidate

Nested-kernel commit `8f88bde92df6` wrote
`CMD_ALL_DISABLE_IMMEDIATELY` to both ISPIF command registers before final
power-off. It was reverted as `1a51c46794e0`. The bundle was
`artifacts/op3-camera-ispif-all-disable-bundle-20260914.tar.gz`, SHA256
`aabef4ce50d436a558ce8e2c05ce06bf4f561fb1325c1b7414af1ffe7f100e88`, with
CAMSS module SHA256
`bcbde51a02a6f90d77c86bb2edf479260ef79a7526d471636cdc55006049364d`.

AF1 captured `8/8`; the command completed (`cmd0=aaaaaaaa`,
`cmd1=0000aa00`), but AF2 still timed out at frame 0 and the overflow
persisted. Result: **FAIL**.

## Current state and next boundary

The net current branch state is commit `1a51c46794e0`: the behavioral
candidates above are reverted, leaving the ISPIF register diagnostics as the
only effective change. No candidate is accepted. The next experiment must be
a new CAMSS/VFE diagnostic or an isolated reset-sequencing test; do not change
Buildroot, the recovery rootfs, DRM, DTS, or userspace while this repeat-open
failure remains unresolved.

The nested camera worktree is clean at `1a51c46794e0`. Its branch push to the
GitHub project remote was attempted with both normal and `--no-thin` packs, but
the remote rejected both with `did not receive expected object
ef4af66b54baac7dea616d2d772ba841d3dae125`; no force-push was used. The source
history therefore remains available locally and is not yet published on that
remote branch.
