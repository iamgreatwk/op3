# OP3 rear IMX298 probe handoff

Task / GitHub Issue: #12, `[LAYER-05] OP3 rear IMX298 mainline camera bring-up`
Role: Implementation Agent
Baseline commit: `67b0bbc3cbf46bae712a2606a43361756fcbd829`
Working branch: `agent/implementation/op3-camera-imx298-001`
Working tree: `source/linux-pmos-msm8996-6.12-camera-imx298`
Commit SHA: `ec5025c75ff4` (tip; commits `c37102be3c5b..ec5025c75ff4`)

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
sensor rails. The VIO rail is now explicitly mapped to the PM8994 `LVS1`
regulator (`ec5025c75ff4`); the previous candidate incorrectly used the
always-on RPM-request `vreg_s4a_1p8`. The vendor Android mode tables were not
copied.

Build run by project owner: NOT_RUN for `ec5025c75ff4`
Build result: the earlier probe candidate built successfully, but this new
DT-only power mapping has not been built yet
Artifacts and SHA256: earlier probe candidate only — `imx298.ko`
`fdddeb1f52710274c12e5f669a0643c8df7ca5a4fc5f26767316d1eadb3c2602`, DTB
`51f494ef4f0a20697b0aecd8d4edbdd376a1614eda4959ef934735f4b75f4df3`, boot
image `c01411ab8085ceb2366f5cd510d521f2b8b6893298a311e5f013e5df33305fe9`

Device test run: 2026-09-08, direct agent test on the owner-authorized device
Device result: software camera stack loaded after correcting module order;
sensor probe still failed with I²C error `-6`
Evidence: `qcom-camss` loaded and `/dev/video0` through `/dev/video5` appeared.
The clean load order was `mc`, `videodev`, `v4l2-async`, `v4l2-fwnode`,
`videobuf2-common`, `videobuf2-memops`, `videobuf2-dma-sg`,
`videobuf2-v4l2`, `i2c-qcom-cci`, `qcom-camss`, `imx298`. The sensor node
`5-001a` had no bound driver and dmesg reported `imx298 5-001a: failed to
read chip ID: -6`. Debugfs showed `vreg_l3a_1p1` and `vreg_l17a_2p6` off after
probe cleanup, while the old candidate's `vreg_s4a_1p8` remained always-on.
The direct test also found that unloading `qcom_camss` triggers a device
kernel unload-time segfault; no further unload or broad I²C scan should be
used in the next run.

Conclusion: INCONCLUSIVE
Uncertainties: The IMX298 ID register layout follows the existing Sony IMX318
driver convention and must be confirmed on hardware. The first device run
also showed no sensor I²C acknowledgement after the initial software stack
was corrected. The driver intentionally does not implement formats, modes,
or streaming, so this stage cannot produce a capture frame.
Recommended next experiment: build `ec5025c75ff4` and its DTB, load the same
module closure, and check whether routing VIO through PM8994 `LVS1` produces
`IMX298 probe passed: chip ID=0x0298`. Do not change the clock, reset GPIO,
CCI address, or driver in that run. If the ID passes, add one independently
tested IMX298 RAW10 mode and capture path in a follow-up commit.

Owner test commands:

```sh
project=/home/kai/src/oneplus3-mainline
kernel=$project/source/linux-pmos-msm8996-6.12-camera-imx298
kout=$project/out/pmos-msm8996-6.12-camera-imx298-vio-lvs1
base=$project/kernel/configs/oneplus3-recovery-audio-full.config
fragment=$project/kernel/configs/oneplus3-recovery-imx298-probe.fragment

mkdir -p "$kout"
cp "$base" "$kout/.config"
"$kernel/scripts/kconfig/merge_config.sh" -m -O "$kout" \
  "$kout/.config" "$fragment"

make -C "$kernel" O="$kout" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  olddefconfig

make -C "$kernel" O="$kout" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  qcom/msm8996-oneplus3.dtb

grep -E '^CONFIG_(VIDEO_IMX298|VIDEO_QCOM_CAMSS|I2C_QCOM_CCI)=' \
  "$kout/.config"
sha256sum "$kout/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb"
```

The owner should package the new kernel/DTB with the existing known-good
Buildroot initramfs, then boot or flash according to the active test plan.
The current deployed Buildroot rootfs also lacks the existing camera module
closure. Reuse the already built and hash-verified
`artifacts/op3-imx298-probe-modules.tar.gz`; it contains `imx298.ko`,
`qcom-camss.ko`, `i2c-qcom-cci.ko`, and their V4L2/Media dependencies. Only
the DTB changes in this experiment, so do not rebuild Buildroot or rebuild
the module bundle.

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
