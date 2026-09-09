# OP3 rear IMX298 probe handoff

Task / GitHub Issue: #12, `[LAYER-05] OP3 rear IMX298 mainline camera bring-up`
Role: Implementation Agent
Baseline commit: `67b0bbc3cbf46bae712a2606a43361756fcbd829`
Working branch: `agent/implementation/op3-camera-imx298-001`
Working tree: `source/linux-pmos-msm8996-6.12-camera-imx298`
Commit SHA: `c79909f9a448` (tip; commits `c37102be3c5b..c79909f9a448`)

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
sensor rails. CCI0 is now set to 400 kHz, matching the old OP3 Android
camera stack's I2C fast mode; the previous probe candidate used 1 MHz. The
VIO rail is kept on the known-good always-on RPM-request `vreg_s4a_1p8`; the
experimental PM8994 `lvs1 {}` child node remains removed. Both the direct
`LVS1` mapping and the control test that retained only the node rebooted
before userspace. The vendor Android mode tables were not copied.

Build run by project owner: 2026-09-08. The DTB from `ec5025c75ff4` built
successfully, but its direct-`LVS1` boot image rebooted immediately and did
not reach SSH. The control checkpoint `a112a6f19fa2` also rebooted before
userspace. The current checkpoint `306d4a364565` then built and booted
successfully with the experimental `lvs1 {}` node removed.
Artifacts and SHA256: new DTB
`9718c5f334d778aee57e3f7f3e59e0fd19f228ab68c744944fa8b997c1fdbe97`; reused
`Image.gz` `5c89259d9340071c9c8684d361042482ca25c8f76b2de4d33cdacf2105f78861`
and initrd
`27736d1d662158bedd5170c033f9b73d807d559a564d3fd6c9090438b6d0f968`;
temporary boot image
`bb71fb07cd3461fdf3bd79ee272779db5f306f4d4c7f8e2497b6eecb36f79c43`;
reused module bundle
`33918d7cb399894a719f1567091054eaceb2eec8c6d96586ad61f26cdd6739ef`

Current no-`LVS1` control artifacts: DTB
`51f494ef4f0a20697b0aecd8d4edbdd376a1614eda4959ef934735f4b75f4df3`;
temporary boot image
`176885492e91e1ac308bac146fa62f8daa5ad01d27335aa6f85e6fe233e932f4`.

CCI-400 follow-up build by project owner: 2026-09-09. The DTB SHA256 is
`067f4788407299dfe4dca1014a666e0e383036276b928848e7a06f87b0482588`.
It reuses the locked Image.gz
`aac420e188dd2ede0e0ade0e42fb110af6f5d03643d2d22be2581c9cdc03233a`,
Buildroot initrd
`27736d1d662158bedd5170c033f9b73d807d559a564d3fd6c9090438b6d0f968`, and
camera module bundle
`33918d7cb399894a719f1567091054eaceb2eec8c6d96586ad61f26cdd6739ef`.
The packaged test image is
`artifacts/boot-oneplus3-pmos612-recovery-imx298-cci400.img` with SHA256
`ba95c78431c13b681ddb7780bb6bf8412f7566ff7de3a68cefe8f6530e6a5295`.

CCI-400 device test: 2026-09-09 over SSH at `172.16.42.1`. The module
archive was extracted and all eleven modules loaded successfully with
explicit `insmod` paths. CAMSS exposed `/dev/video0` through `/dev/video5`,
but the sensor still reported `imx298 5-001a: failed to read chip ID: -6`;
no sensor driver binding or chip-ID pass was observed. This is a device FAIL
for the CCI-speed hypothesis, not a camera-layer acceptance.

The next diagnostic-only revision is commit `c79909f9a448`. It changes only
`drivers/media/i2c/imx298.c` logging: it records the reset GPIO logical value
before/after assertion and release, each rail's `regulator_is_enabled()` and
`regulator_get_voltage()` result, the bulk-enable return value, the MCLK
enable return value and effective rate, and separate results for the
`0x0016`/`0x0017` chip-ID reads. It deliberately keeps the existing
`GPIOD_OUT_LOW`, bulk regulator enable, reset timing, and CCI-400 DTS
unchanged. No boot image or Buildroot rebuild is required for this revision.

Device test run: first candidate tested 2026-09-08 by direct agent access;
both the direct-`LVS1` boot image and the `lvs1`-node-only control image
rebooted before userspace. The known-good probe image booted normally on the
same packaging path.
Device result: the no-`LVS1` image reached recovery. After temporarily
uploading the complete archived camera module closure and loading it in the
correct order, CAMSS loaded and created `/dev/video0` through `/dev/video5`;
the sensor probe still failed with I²C error `-6`.
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

Conclusion: INCONCLUSIVE (the CCI-400 subexperiment failed; power-on
diagnostic evidence is pending)
Uncertainties: The IMX298 ID register layout follows the existing Sony IMX318
driver convention and must be confirmed on hardware. The first device run
also showed no sensor I²C acknowledgement after the initial software stack
was corrected. The driver intentionally does not implement formats, modes,
or streaming, so this stage cannot produce a capture frame.
The old Android OP3 IMX298 node also declares `cam_v_custom1` at 2.15 V,
`cam_vaf` at 2.8 V, and GPIO39 for the VAF path; those resources remain out
of scope until the diagnostic run identifies the current rail/reset/clock
state.
Recommended next experiment: complete the diagnostic-only module test, then
change exactly one behavior variable: the reset request from
`GPIOD_OUT_LOW` to `GPIOD_OUT_HIGH`. Keep CCI at 400 kHz, reset GPIO30, CCI
address, sensor MCLK, core rails, endpoint, and chip-ID registers fixed. Do
not reintroduce `lvs1` or add auxiliary rails in the reset experiment. PASS
remains a clean
`IMX298 probe passed: chip ID=0x0298` message with a registered V4L2 sensor
sub-device and no CAMSS fault; FAIL is the unchanged `-ENXIO`/`-6` probe
result. The diagnostic revision itself has no PASS/FAIL behavior change; its
purpose is to select the next falsifiable experiment.

Owner test commands:

`modules_prepare` creates generated headers but intentionally does not create
the kernel export table needed by `modpost`. The existing full recovery
output has the required `Module.symvers` and the same `6.12.1-msm8996+`
release and cross-compiler configuration. The in-tree single-target path
does not consume `KBUILD_EXTRA_SYMBOLS`, so build this one source file through
a temporary external-module wrapper under the output directory:

```sh
project=/home/kai/src/oneplus3-mainline
kernel=$project/source/linux-pmos-msm8996-6.12-camera-imx298
kout=$project/out/pmos-msm8996-6.12-camera-imx298-cci400
fullout=$project/out/pmos-msm8996-6.12-recovery-audio-full-s1302-poll-drm100
symvers=$fullout/Module.symvers
module_build=$kout/imx298-module

test "$(git -C "$kernel" rev-parse --short=12 HEAD)" = c79909f9a448
git -C "$kernel" log -1 --oneline
grep -qx 'CONFIG_VIDEO_IMX298=m' "$kout/.config"
test -s "$symvers"

make -C "$kernel" O="$kout" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  modules_prepare

mkdir -p "$module_build"
ln -sfn "$project/scripts/op3-imx298-module.mk" "$module_build/Makefile"
ln -sfn "$kernel/drivers/media/i2c/imx298.c" "$module_build/imx298.c"

KBUILD_EXTRA_SYMBOLS="$symvers" \
make -C "$kernel" O="$kout" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  M="$module_build" modules

sha256sum "$module_build/imx298.ko"
```

This is a module-only build. Do not rebuild `Image.gz`, the DTB, Buildroot,
or the boot image. The existing CCI-400 boot image is sufficient because the
diagnostic revision changes only `imx298.ko`.

The current deployed Buildroot rootfs lacks the camera module closure. Reuse
the already built and hash-verified `artifacts/op3-imx298-probe-modules.tar.gz`;
it contains the old `imx298.ko`, `qcom-camss.ko`, `i2c-qcom-cci.ko`, and their
V4L2/Media dependencies. Upload the newly built module separately so the old
driver is not loaded:

```sh
scp -O "$module_build/imx298.ko" \
  root@172.16.42.1:/newroot/tmp/imx298-diagnostic.ko
```

After rebooting the already tested CCI-400 image, extract the dependency
bundle under `/newroot`. The bundle is intentionally minimal and does not
contain `modules.dep`; use explicit `insmod` paths. Do not unload
`qcom-camss`, because the previous device test triggered an unload-time
kernel segfault.

```sh
busybox gzip -dc /newroot/tmp/op3-imx298-probe-modules.tar.gz |
  busybox tar -x -C /newroot

module_root=/newroot/lib/modules/6.12.1-msm8996+/kernel
insmod "$module_root/drivers/media/mc/mc.ko"
insmod "$module_root/drivers/media/v4l2-core/videodev.ko"
insmod "$module_root/drivers/media/v4l2-core/v4l2-async.ko"
insmod "$module_root/drivers/media/v4l2-core/v4l2-fwnode.ko"
insmod "$module_root/drivers/media/common/videobuf2/videobuf2-common.ko"
insmod "$module_root/drivers/media/common/videobuf2/videobuf2-memops.ko"
insmod "$module_root/drivers/media/common/videobuf2/videobuf2-dma-sg.ko"
insmod "$module_root/drivers/media/common/videobuf2/videobuf2-v4l2.ko"
insmod "$module_root/drivers/i2c/busses/i2c-qcom-cci.ko"
insmod "$module_root/drivers/media/platform/qcom/camss/qcom-camss.ko"
insmod /newroot/tmp/imx298-diagnostic.ko

dmesg | grep -iE 'imx298|camss|cci|csiphy|csid|vfe|media'
find /dev -maxdepth 1 \( -name 'media*' -o -name 'v4l-subdev*' -o -name 'video*' \) -print
cat /sys/bus/i2c/devices/5-001a/name 2>/dev/null || true
```

Collect the complete diagnostic lines, especially:

```sh
dmesg | grep -iE 'imx298.*(power_on|power_off|power:|mclk|reset|chip-id)|failed to read chip ID|probe passed'
```

For this probe-only stage, PASS is still a clean `IMX298 probe passed`
message with chip ID `0x0298`, a sensor sub-device, and no CAMSS fault. A
video node or frame is not expected until the mode/streaming follow-up. The
diagnostic revision itself cannot turn the previous `-6` into a PASS; it
provides the evidence needed to select the next one-variable reset test.
