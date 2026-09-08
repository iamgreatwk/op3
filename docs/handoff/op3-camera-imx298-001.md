# OP3 rear IMX298 probe handoff

Task / GitHub Issue: #12, `[LAYER-05] OP3 rear IMX298 mainline camera bring-up`
Role: Implementation Agent
Baseline commit: `67b0bbc3cbf46bae712a2606a43361756fcbd829`
Working branch: `agent/implementation/op3-camera-imx298-001`
Working tree: `source/linux-pmos-msm8996-6.12-camera-imx298`
Commit SHA: `a112a6f19fa2` (tip; commits `c37102be3c5b..a112a6f19fa2`)

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
regulator in the previous candidate; the current checkpoint keeps the
always-on RPM-request `vreg_s4a_1p8` while isolating the new `lvs1 {}` node.
The direct `LVS1` mapping rebooted before userspace. The vendor Android mode
tables were not copied.

Build run by project owner: 2026-09-08, DTB-only build from `ec5025c75ff4`
Build result: PASS for the previous direct-`LVS1` candidate. The resulting
boot image rebooted immediately and did not reach SSH; `/sys/fs/pstore` was
empty after booting the known-good control image. The current checkpoint
`a112a6f19fa2` has not been built yet.
Artifacts and SHA256: new DTB
`9718c5f334d778aee57e3f7f3e59e0fd19f228ab68c744944fa8b997c1fdbe97`; reused
`Image.gz` `5c89259d9340071c9c8684d361042482ca25c8f76b2de4d33cdacf2105f78861`
and initrd
`27736d1d662158bedd5170c033f9b73d807d559a564d3fd6c9090438b6d0f968`;
temporary boot image
`bb71fb07cd3461fdf3bd79ee272779db5f306f4d4c7f8e2497b6eecb36f79c43`;
reused module bundle
`33918d7cb399894a719f1567091054eaceb2eec8c6d96586ad61f26cdd6739ef`

Device test run: first candidate tested 2026-09-08 by direct agent access;
the direct-`LVS1` boot image rebooted before userspace. The known-good probe
image booted normally on the same packaging path.
Device result: software camera stack loaded after correcting module order;
sensor probe still failed with I²C error `-6` on the previous DTB
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
Recommended next experiment: build `a112a6f19fa2` and its DTB, then use the
same known-good initrd and module closure. This keeps the new `lvs1 {}` node
but restores the old camera VIO mapping, isolating whether the node itself
causes the early reboot. Do not change the clock, reset GPIO, CCI address, or
driver in that run. If it boots, the next experiment can test a safer LVS1
enable sequence separately; if it still reboots, remove the unused node and
revisit the regulator implementation.

Owner test commands:

```sh
project=/home/kai/src/oneplus3-mainline
kernel=$project/source/linux-pmos-msm8996-6.12-camera-imx298
kout=$project/out/pmos-msm8996-6.12-camera-imx298-lvs1-node-control
base=$project/kernel/configs/oneplus3-recovery-audio-full.config
fragment=$project/kernel/configs/oneplus3-recovery-imx298-probe.fragment

test "$(git -C "$kernel" rev-parse HEAD)" = a112a6f19fa2
git -C "$kernel" log -1 --oneline

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

The owner should package the control DTB with the existing known-good
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
