# OP3 rear IMX298 probe handoff

Task / GitHub Issue: #12, `[LAYER-05] OP3 rear IMX298 mainline camera bring-up`
Role: Implementation Agent
Baseline commit: `67b0bbc3cbf46bae712a2606a43361756fcbd829`
Working branch: `agent/implementation/op3-camera-imx298-001`
Working tree: `source/linux-pmos-msm8996-6.12-camera-imx298`
Commit SHA: `42da3ad95668` (tip; commits `c37102be3c5b..42da3ad95668`)

Layer: kernel camera / CCI / CAMSS
Hypothesis tested: The OP3 15801 rear IMX298 can be powered and identified on
the existing MSM8996 CCI/CAMSS path using its documented board wiring.
Only variable changed: rear IMX298 probe support; front IMX179, OIS, actuator,
flash, EEPROM, and camera userspace remain out of scope.

Changed files:

- `drivers/media/i2c/imx298.c`: probe-only V4L2 sensor driver; reads chip ID
  `0x0016/0x0017` after power-on and registers a source pad.
- `drivers/media/i2c/Kconfig`, `drivers/media/i2c/Makefile`.
- `Documentation/devicetree/bindings/media/i2c/sony,imx298.yaml`.
- `arch/arm64/boot/dts/qcom/msm8996-oneplus3.dts`: enables CCI0/CAMSS and
  describes the 4-lane rear sensor at address `0x1a`.
- Top-level candidate fragment:
  `kernel/configs/oneplus3-recovery-imx298-probe.fragment`.

The DT uses the old 15801 evidence for GPIO30 reset/XCLR, GPIO13 MCLK0,
24 MHz, CCI0 address `0x1a`, four CSI-2 lanes, and 1.1 V / 1.8 V / 2.6 V
sensor rails. The vendor Android mode tables were not copied.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: none

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: none

Conclusion: INCONCLUSIVE
Uncertainties: The IMX298 ID register layout follows the existing Sony IMX318
driver convention and must be confirmed on hardware. The driver intentionally
does not implement formats, modes, or streaming, so this stage cannot produce
a `/dev/video*` capture node or a frame.
Recommended next experiment: build this candidate, load the existing CCI and
CAMSS modules plus `imx298.ko`, and check for the `IMX298 probe passed` log and
a registered V4L2 sub-device. If the ID passes, add one independently tested
IMX298 RAW10 mode and capture path in a follow-up commit.

Owner test commands:

```sh
project=/home/kai/src/oneplus3-mainline
kernel=$project/source/linux-pmos-msm8996-6.12-camera-imx298
kout=$project/out/pmos-msm8996-6.12-camera-imx298-probe
baseout=$project/out/pmos-msm8996-6.12-recovery-audio-full-s1302-poll-drm100
symvers=$baseout/Module.symvers
base=$project/kernel/configs/oneplus3-recovery-audio-full.config
fragment=$project/kernel/configs/oneplus3-recovery-imx298-probe.fragment
firmware_fragment=$project/kernel/configs/oneplus3-recovery-camera-module-only.fragment

mkdir -p "$kout"
cp "$base" "$kout/.config"
"$kernel/scripts/kconfig/merge_config.sh" -m -O "$kout" \
  "$kout/.config" "$fragment" "$firmware_fragment"

make -C "$kernel" O="$kout" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  olddefconfig modules_prepare

# Build the one module as an external kbuild target. The integrated kernel
# keeps V4L2/MC symbols in modules, so use its Module.symvers for modpost.
test -s "$symvers"
module_dir="$kout/imx298-module"
mkdir -p "$module_dir"
ln -sfn "$kernel/drivers/media/i2c/imx298.c" "$module_dir/imx298.c"
ln -sfn "$project/scripts/op3-imx298-module.mk" "$module_dir/Makefile"

make -C "$kernel" O="$kout" M="$module_dir" \
  ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  KBUILD_EXTRA_SYMBOLS="$symvers" modules

make -C "$kernel" O="$kout" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  qcom/msm8996-oneplus3.dtb

grep -E '^CONFIG_(VIDEO_IMX298|VIDEO_QCOM_CAMSS|I2C_QCOM_CCI)=' \
  "$kout/.config"
find "$module_dir" -name imx298.ko -print
sha256sum "$kout/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb" \
  "$module_dir/imx298.ko"
```

The owner should package the new kernel/DTB with the existing known-good
Buildroot initramfs, then boot or flash according to the active test plan.
The current deployed Buildroot rootfs also lacks the existing `qcom-camss.ko`
and `i2c-qcom-cci.ko` modules, so a probe-only test must temporarily copy
those modules and their V4L2/Media dependencies together with `imx298.ko`.
Do not rebuild Buildroot for this first probe; integrate the tested module set
only after the device result is known.

After boot, use the device's existing SSH path and run:

```sh
modprobe i2c-qcom-cci
modprobe qcom-camss
modprobe imx298

dmesg | grep -iE 'imx298|camss|cci|csiphy|csid|vfe|media'
find /dev -maxdepth 1 \( -name 'media*' -o -name 'v4l-subdev*' -o -name 'video*' \) -ls
find /sys/bus/i2c/devices -maxdepth 2 -type f -name name -print -exec cat {} \;
```

For this probe-only stage, PASS is a clean `IMX298 probe passed` message with
chip ID `0x0298`, a sensor sub-device, and no CAMSS fault. A video node or
frame is not expected until the mode/streaming follow-up.
