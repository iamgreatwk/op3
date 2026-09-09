# OP3 rear IMX298 probe handoff

Task / GitHub Issue: #12, `[LAYER-05] OP3 rear IMX298 mainline camera bring-up`
Role: Implementation Agent
Baseline commit: `67b0bbc3cbf46bae712a2606a43361756fcbd829`
Working branch: `agent/implementation/op3-camera-imx298-001`
Working tree: `source/linux-pmos-msm8996-6.12-camera-imx298`
Commit SHA: `e5332d149d85` (tip; previous SMD-RPM registration candidate
`67630ec3b9e5`; previous rejected direct-SPMI LVS1 candidate
`c298ff3e7197` was reverted by `8cce8d4643b3`)

Layer: kernel camera / CCI / CAMSS

## Device result: IMX298 chip-ID PASS through SMD-RPM LVS1 (2026-09-09)

The owner-built DTB-only candidate was booted with `fastboot boot` on the
OnePlus 3. DTB SHA256 is
`a53362f960057d7e9d8b8efa2808ea68d84dd82ca15ae6486bf2a227efe49d7a`; the
temporary boot image SHA256 is
`8809a46d5be7b5f983a0d3acfb27bd33c34b12bcb8951d486d123d8252933e84`.
The locked kernel Image.gz and Buildroot initrd were reused. The existing
camera dependency bundle SHA256 is
`33918d7cb399894a719f1567091054eaceb2eec8c6d96586ad61f26cdd6739ef`, and
the compatible original-power-sequence `imx298.ko` used for the test has
SHA256 `47052972b47ac33611686be017322fb329e9088a9ad18e39466eea9dc4fc2a65`.

After explicit `insmod` of the archived CAMSS/CCI/V4L2 dependencies and the
module, the device log showed VANA 2.6 V, VDIG 1.1 V, VIO via LVS1 at 1.8 V,
CUSTOM1 2.15 V, MCLK 24 MHz, and RESET logical `1` to `0`. Reads of `0x0016`
and `0x0017` returned `0x02` and `0x98`, followed by
`IMX298 probe passed: chip ID=0x0298, CSI-2 lanes=4`. CAMSS exposed
`/dev/media0` and `/dev/video0` through `/dev/video5`. The sensor's power-off
sequence then disabled VIO and the two LVS1 sysfs entries remained registered
and disabled, as expected after cleanup. No image was flashed.

Result: PASS for the isolated IMX298 VIO-routing hypothesis. This is not yet
Integration acceptance: no mode table, CSI-2 stream, RAW10 frame, exposure,
gain, or capture userspace has been implemented.

## Current next step: implement and test the minimum IMX298 RAW10 stream

The SMD-RPM LVS1 registration candidate passed its isolated boot test:
`vreg_lvs1a_1p8` and `lvs1` appeared in sysfs and the phone reached userspace.
Commit `e5332d149d85` now changes exactly one downstream connection in the
camera node: IMX298 `vio-supply` moves from `&vreg_s4a_1p8` to
`&vreg_lvs1a_1p8`. The sensor driver, all other rails, GPIOs, MCLK, kernel
image, initramfs, module bundle, and Buildroot remain unchanged for that
registration test.

The next camera-only hypothesis is that the confirmed sensor can be started
with one documented OP3 IMX298 mode: four-lane CSI-2, RAW10, one conservative
preview resolution, and the matching link frequency/pixel rate. Keep this as
a driver-only change; do not change regulators, DTS, CAMSS, or Buildroot.

Hypothesis: the sensor's VIO must be supplied by the PM8994 LVS1 output rather
than the always-on S4 rail. PASS requires boot to userspace, successful
IMX298 resource enable, and a chip-ID read of `0x0298`; an early reboot, rail
enable failure, or unchanged I2C `-6` is FAIL. Do not flash this candidate;
use `fastboot boot` only.

Owner build and package commands (DTB only; reuse the existing camera module
bundle; no kernel image, module, or Buildroot rebuild):

```sh
project=/home/kai/src/oneplus3-mainline
kernel=$project/source/linux-pmos-msm8996-6.12-camera-imx298
baseout=$project/out/pmos-msm8996-6.12-camera-imx298-cci400
kout=$project/out/pmos-msm8996-6.12-camera-imx298-vio-smd-lvs1
fullout=$project/out/pmos-msm8996-6.12-recovery-audio-full-s1302-poll-drm100
initrd=$project/artifacts/initrd-op3-recovery-buildroot-s1302-poll-drm100.cpio.gz
bootimg=$project/artifacts/boot-oneplus3-pmos612-recovery-imx298-vio-smd-lvs1.img

test "$(git -C "$kernel" rev-parse --short=12 HEAD)" = e5332d149d85
test -r "$baseout/.config"
mkdir -p "$kout"
cp "$baseout/.config" "$kout/.config"

make -C "$kernel" O="$kout" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  olddefconfig qcom/msm8996-oneplus3.dtb

dtb=$kout/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb
sha256sum "$dtb"
test -r "$fullout/arch/arm64/boot/Image.gz"
test -r "$initrd"

"$project/scripts/pack-boot.sh" \
  "$fullout/arch/arm64/boot/Image.gz" "$dtb" "$initrd" "$bootimg"
sha256sum "$bootimg"
fastboot boot "$bootimg"
```

After boot, upload and load the already hash-verified dependency bundle with
explicit `insmod` paths. It contains the probe-only `imx298.ko`; do not
unload `qcom-camss`:

```sh
scp -O "$project/artifacts/op3-imx298-probe-modules.tar.gz" \
  root@172.16.42.1:/newroot/tmp/
ssh root@172.16.42.1 '
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
insmod "$module_root/drivers/media/i2c/imx298.ko"
dmesg | grep -iE "imx298|camss|cci|csiphy|csid|vfe|media|lvs1|regulator"
'
```

If the module path or kernel release differs, stop and inspect the archive
layout; do not substitute a module built for another kernel release.

## Previous next step: owner DTB-only test of the SMD-RPM LVS1 declaration

The previous LVS1 attempt used the wrong DT topology: it added the node below
`&pm8994_spmi_regulators`, while this OP3 board registers PM8994 switch rails
under `&rpm_requests` with `compatible = "qcom,rpm-pm8994-regulators"`.
Commit `67630ec3b9e5` adds the missing `vdd_lvs1_2-supply` and an `lvs1` child
under that SMD-RPM node, matching the mainline MSM8996 PM8994 regulator
layout. This is registration-only: IMX298 `vio-supply` remains
`&vreg_s4a_1p8`, and the camera driver, kernel config, initramfs, and
Buildroot are unchanged.

Hypothesis: the earlier reboot was caused by declaring LVS1 through the
wrong regulator provider, not by LVS1 itself. Only variable changed: the DT
provider/topology for LVS1 registration. PASS requires boot to userspace and
an LVS1 regulator entry; an early reboot, regulator probe failure, or no LVS1
entry is FAIL. Do not flash this candidate; use `fastboot boot` only.

Owner build and package commands (DTB only; no kernel image, module, or
Buildroot rebuild):

```sh
project=/home/kai/src/oneplus3-mainline
kernel=$project/source/linux-pmos-msm8996-6.12-camera-imx298
baseout=$project/out/pmos-msm8996-6.12-camera-imx298-cci400
kout=$project/out/pmos-msm8996-6.12-camera-imx298-rpm-lvs1-smd
fullout=$project/out/pmos-msm8996-6.12-recovery-audio-full-s1302-poll-drm100
initrd=$project/artifacts/initrd-op3-recovery-buildroot-s1302-poll-drm100.cpio.gz
bootimg=$project/artifacts/boot-oneplus3-pmos612-recovery-imx298-rpm-lvs1-smd.img

test "$(git -C "$kernel" rev-parse --short=12 HEAD)" = 67630ec3b9e5
test -r "$baseout/.config"
mkdir -p "$kout"
cp "$baseout/.config" "$kout/.config"

make -C "$kernel" O="$kout" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  olddefconfig qcom/msm8996-oneplus3.dtb

dtb=$kout/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb
sha256sum "$dtb"
test -r "$fullout/arch/arm64/boot/Image.gz"
test -r "$initrd"

"$project/scripts/pack-boot.sh" \
  "$fullout/arch/arm64/boot/Image.gz" "$dtb" "$initrd" "$bootimg"
sha256sum "$bootimg"
fastboot boot "$bootimg"
```

Owner DTB-only build and device test: 2026-09-09. The candidate DTB SHA256
is `cc1a046822cf867b6deaf082458a64f3eeb98c38358b30ad9bc8d0aefa3d39cd` and
the temporary boot image SHA256 is
`4933bbeec5631113c2a7311ee68524ed31fc78bae39f9a3da7d9cc01025b5e63`.
`fastboot boot` reached userspace; the phone reported kernel
`6.12.1-msm8996+` and remained up for at least two minutes. The device showed
both `/sys/class/regulator/regulator.32` (`vreg_lvs1a_1p8`) and
`/sys/class/regulator/regulator.76` (`lvs1`), each with `state=disabled` as
expected for a registration-only test. The first evidence command also
exposed that the recovery BusyBox `find` lacks `-printf`; the compatible loop
in the handoff was used for the final result. No camera module was loaded and
no image was flashed.

Result: PASS for the isolated SMD-RPM LVS1 registration hypothesis; this is
not Integration acceptance and does not establish IMX298 detection. The
camera sensor still needs a separate VIO-routing or hardware-wiring
experiment, with the current no-LVS1 DT remaining the safe control.

After a successful boot, collect only the registration evidence:

```sh
host=172.16.42.1
ssh root@"$host" '
set +e
echo "--- regulator links ---"
find /sys/class/regulator -maxdepth 1 -type l -printf "%f -> %l\n" | sort | grep -i lvs1
echo "--- regulator names ---"
for r in /sys/class/regulator/regulator*; do
    [ -r "$r/name" ] || continue
    name=$(cat "$r/name")
    case "$name" in *lvs1*|*LVS1*) echo "$r: $name";; esac
done
echo "--- rpm/lvs dmesg ---"
dmesg | grep -iE "rpm|lvs1|regulator|reboot|panic"
'
```

If this candidate again reboots before userspace, do not try another DTS
variant. On the next known-good boot run the pstore collection below; if it
is mounted but empty, the required next diagnostic is physical MSM8996 UART.

## Prior next step: collect early-reboot evidence before another DTS experiment

The no-`LVS1` camera control boots, and the current 6.12 recovery line already
has `CONFIG_PSTORE_RAM=y`, `CONFIG_PSTORE_CONSOLE=y`, and the OP3
`ramoops@ac000000` DT node. Do not change the camera DTS or add `lvs1` again
until the result of this check is recorded. On the next successful no-`LVS1`
boot, run the following read-only collection command before testing another
candidate:

```sh
host=172.16.42.1
ssh root@"$host" '
set +e
mkdir -p /sys/fs/pstore
grep -q " /sys/fs/pstore " /proc/mounts || \
  mount -t pstore pstore /sys/fs/pstore
echo "--- pstore mount ---"
grep " /sys/fs/pstore " /proc/mounts
echo "--- pstore files ---"
ls -la /sys/fs/pstore
for f in /sys/fs/pstore/*; do
    [ -f "$f" ] || continue
    echo "--- $f ---"
    sed -n "1,240p" "$f"
done
echo "--- ramoops/pstore dmesg ---"
dmesg | grep -iE "ramoops|pstore|persistent ram"
'
```

If the phone returns to fastboot and the next no-`LVS1` boot shows a mounted
but empty pstore, treat that as evidence that the reset is not a persisted
kernel oops/panic. The next required diagnostic is a physical MSM8996 UART,
using the existing `earlycon=msm_hsl_uart,0x75B0000` / `115200 8N1` path; do not
guess another regulator or camera power mapping. Connect only TX, RX, and GND
at the board's UART level and do not connect the adapter's VCC.

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

This diagnostic-only revision is commit `c79909f9a448`. It changes only
`drivers/media/i2c/imx298.c` logging: it records the reset GPIO logical value
before/after assertion and release, each rail's `regulator_is_enabled()` and
`regulator_get_voltage()` result, the bulk-enable return value, the MCLK
enable return value and effective rate, and separate results for the
`0x0016`/`0x0017` chip-ID reads. It deliberately keeps the existing
`GPIOD_OUT_LOW`, bulk regulator enable, reset timing, and CCI-400 DTS
unchanged. No boot image or Buildroot rebuild is required for this revision.

Diagnostic module device test: 2026-09-09 over SSH at `192.168.1.4`. The
owner-built module SHA256 is
`d9b5ea7a08a4a11840f95777f272eceb73750e8a9b16228d5b38ea3ce417c70f`.
The old `imx298` module was removed with `rmmod` return code 0 and the
diagnostic module was inserted with `insmod` return code 0; CAMSS and all
dependencies were left loaded. The diagnostic output showed:

- reset before assertion: logical `0`; asserted: logical `1`; released:
  logical `0`;
- `vdig`: enabled `1`, `1100000` µV; `vio`: enabled `1`, `1800000` µV;
  `vana`: enabled `1`, `2600000` µV;
- MCLK enable return `0`, effective rate `24000000` Hz;
- chip-ID high register `0x0016`: return `-6`.

After probe cleanup, VDIG and VANA became disabled while VIO remained enabled,
confirming that the current VIO source is effectively always-on. This is a
diagnostic PASS for the power/clock evidence, but the sensor probe remains a
FAIL because the address still returns `-6`.

Reset-initial-state experiment: commit `d247ce811242` changed exactly one
behavior variable in `imx298_probe()`: `devm_gpiod_get()` now requests the
active-low reset GPIO with `GPIOD_OUT_HIGH`, keeping the sensor asserted from
GPIO acquisition until the existing `imx298_power_on()` release point. DTS,
regulators, MCLK, delays, CCI400, and chip-ID reads are unchanged. This
experiment was tested on 2026-09-09 over SSH at `192.168.1.4` with module
SHA256
`33afe61c5fc19aaf32a51285476731af77b2775575c8acbc5b66244f958fa24a`.
`rmmod imx298` and insertion of the reset-high module both returned 0. The
diagnostic log confirmed reset-before-assert logical `1`, but the `0x0016`
read still returned `-6`; no chip-ID pass occurred. This is a FAIL for the
reset-initial-state hypothesis.

Original-power-resource sequence experiment: commit `7fe1f2f950b2` changes
only the documented OP3 Android IMX298 power-resource sequence. The preserved
`vendor.img` and original OnePlus Android kernel show that the rear sensor
uses VANA 2.6 V, VDIG 1.1 V, VIO, the S5/CUSTOM1 2.15 V rail, and GPIO39 as
the CAM_VAF0 control. The recovered `libmmcamera_imx298.so` table gives this
order and delay: assert RESET for 2 ms; enable VANA, VDIG, and VIO with 2 ms
between each; set GPIO39 high; enable CUSTOM1 and wait 5 ms; enable 24 MHz
MCLK and wait 2 ms; release RESET and wait 2 ms. The driver now performs
that order explicitly with individual regulator calls and exact reverse
cleanup. The DTS adds `custom1-supply = <&vreg_s5a_2p15>`,
`vaf-gpios = <&tlmm 39 GPIO_ACTIVE_HIGH>`, and GPIO39 active/sleep states.
The known-good `vreg_s4a_1p8` remains VIO; the crashing `lvs1` node is not
reintroduced. No sensor mode table, stream operation, or Buildroot content
was changed. Static `checkpatch.pl` and `git diff --check` passed. The
owner-authorized agent build completed on 2026-09-09 using the existing
CCI-400 configuration: `imx298.ko` SHA256
`47052972b47ac33611686be017322fb329e9088a9ad18e39466eea9dc4fc2a65`, new
DTB SHA256
`91da5fe9f15ef61114fe9c4f37d0b19aebab1e68ab617f957bdce2c665bea28b`, and
temporary boot image SHA256
`6009fd778e47dab26dd6e33b5fa79062f29875955e0d9e88da1d303836e653ec`.
Device test: 2026-09-09 over SSH at `172.16.42.1` after `fastboot boot`.
The new module inserted with `insmod` return code 0, and the archived camera
dependency bundle loaded successfully; CAMSS created `/dev/video0` through
`/dev/video5`. The new log confirmed VANA 2.6 V, VDIG 1.1 V, VIO 1.8 V,
VAF GPIO39 logical 1, CUSTOM1 2.15 V, MCLK 24 MHz, and RESET logical 1 to 0.
The sensor still returned `chip-id-high read reg=0x0016 rc=-6`, then cleaned
up its switchable rails; `/sys/bus/i2c/devices/5-001a` remained unbound.
Result: FAIL for the original-power-resource-sequence hypothesis. No image
was flashed.

The next owner build must produce both the new DTB and the new external
`imx298.ko`; the old CCI-400 image cannot test the new supply/GPIO properties.
Reuse the existing kernel `Image.gz` and Buildroot initrd when repacking the
temporary boot image. The falsifiable PASS condition remains a clean
`IMX298 probe passed: chip ID=0x0298` message. A boot failure, resource
enable failure, or unchanged I2C `-6` is a FAIL for this power-sequence
hypothesis.

Owner build and package commands:

```sh
project=/home/kai/src/oneplus3-mainline
kernel=$project/source/linux-pmos-msm8996-6.12-camera-imx298
baseout=$project/out/pmos-msm8996-6.12-camera-imx298-cci400
kout=$project/out/pmos-msm8996-6.12-camera-imx298-original-power-sequence
fullout=$project/out/pmos-msm8996-6.12-recovery-audio-full-s1302-poll-drm100
symvers=$fullout/Module.symvers
module_build=$kout/imx298-module

test "$(git -C "$kernel" rev-parse --short=12 HEAD)" = 7fe1f2f950b2
test -r "$baseout/.config"
test -s "$symvers"
mkdir -p "$kout"
cp "$baseout/.config" "$kout/.config"
grep -qx 'CONFIG_VIDEO_IMX298=m' "$kout/.config"

make -C "$kernel" O="$kout" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  olddefconfig \
  qcom/msm8996-oneplus3.dtb \
  modules_prepare

mkdir -p "$module_build"
ln -sfn "$project/scripts/op3-imx298-module.mk" "$module_build/Makefile"
ln -sfn "$kernel/drivers/media/i2c/imx298.c" "$module_build/imx298.c"

KBUILD_EXTRA_SYMBOLS="$symvers" \
make -C "$kernel" O="$kout" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  M="$module_build" modules

sha256sum \
  "$kout/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb" \
  "$module_build/imx298.ko"

initrd=$project/artifacts/initrd-op3-recovery-buildroot-s1302-poll-drm100.cpio.gz
bootimg=$project/artifacts/boot-oneplus3-pmos612-recovery-imx298-original-power-sequence.img
test -f "$fullout/arch/arm64/boot/Image.gz"
test -f "$initrd"
"$project/scripts/pack-boot.sh" \
  "$fullout/arch/arm64/boot/Image.gz" \
  "$kout/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb" \
  "$initrd" "$bootimg"
sha256sum "$bootimg"
```

Do not flash this image yet. First use `fastboot boot` and load the new
module with the existing camera dependency bundle. Keep `qcom-camss` loaded;
the previous test found an unload-time kernel segfault. Capture the complete
`dmesg` lines containing `imx298`, `power_on`, `VAF`, `enable`, and `chip-id`.

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

Conclusion: INCONCLUSIVE (the CCI-400 subexperiment failed; diagnostics show
the expected rails and 24 MHz MCLK, but the sensor still does not ACK)
Uncertainties: The IMX298 ID register layout follows the existing Sony IMX318
driver convention and must be confirmed on hardware. The first device run
also showed no sensor I²C acknowledgement after the initial software stack
was corrected. The driver intentionally does not implement formats, modes,
or streaming, so this stage cannot produce a capture frame.
The old Android OP3 IMX298 node also declares `cam_v_custom1` at 2.15 V,
`cam_vaf` at 2.8 V, and GPIO39 for the VAF path; those resources remain out
of scope until the diagnostic run identifies the current rail/reset/clock
state.
The original power-resource order is now implemented in commit
`7fe1f2f950b2`; its owner build and test instructions are above. CCI remains
at 400 kHz, reset GPIO30, the CCI address, sensor MCLK, endpoint, and chip-ID
registers remain fixed. The `lvs1` node is not reintroduced. PASS remains a
clean `IMX298 probe passed: chip ID=0x0298` message with a registered V4L2
sensor sub-device and no CAMSS fault; FAIL is a boot/resource-enable failure
or the unchanged `-ENXIO`/`-6` probe result.

## PM8994 LVS1 complete-DT declaration candidate

Nested-kernel commit `c298ff3e7197` changes one regulator declaration in
`arch/arm64/boot/dts/qcom/msm8996-oneplus-common.dtsi`: it adds
`vdd_lvs_1_2-supply = <&vreg_s4a_1p8>` and restores the
`pm8994_lvs1: lvs1 {}` child node. This follows the PM8994 SPMI binding and
the working OP3 board S4 1.8 V rail. The camera sensor's `vio-supply` remains
`&vreg_s4a_1p8`, so this does not yet test camera VIO routing; it tests only
whether the correctly declared LVS1 regulator can register without the
previous early reboot.

Hypothesis: the previous LVS1-only boot failure was caused by an incomplete
SPMI regulator declaration without its parent input supply. Only variable:
complete the LVS1 DT declaration. PASS: the owner boots the new DTB to
userspace and sees a registered `lvs1` regulator without reboot. FAIL: early
reboot, regulator probe error, or no LVS1 registration. Use UART/pstore if the
device reboots before SSH. Do not unload CAMSS and do not rebuild Buildroot.

Owner DTB-only build/test is pending. Reuse the known-good Image.gz and
Buildroot initrd; do not build `imx298.ko` for this regulator-only test.

Owner DTB build and device test: 2026-09-09. The candidate DTB SHA256 is
`721a4a15c7366b5b3b44bfabea9f566b478d063a7fb9b036d4b93f4f5b45ab2b`; the
temporary boot image is
`artifacts/boot-oneplus3-pmos612-recovery-imx298-lvs1-decl-20260909.img`
with SHA256
`221f3077913f9ef393d57d1e2133638c694570851ca47354babd9dd08d48dc3e`.
`fastboot boot` completed, but the phone rebooted back to fastboot before
userspace and never exposed SSH. Adding the parent input supply therefore did
not make the LVS1 path safe. Result: FAIL; the incomplete-parent hypothesis is
rejected. Keep the known-good no-LVS1 DT as the camera control baseline. Any
future LVS1 work requires early-boot UART or pstore evidence and must not be
used in the normal recovery image.

The nested camera branch was returned to the known-good no-LVS1 state by
revert commit `8cce8d4643b3`. The rejected `c298ff3e7197` remains in history
as the reproducible failed experiment; no destructive reset or history rewrite
was used.

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
kout=$project/out/pmos-msm8996-6.12-camera-imx298-cci400-reset-high
baseout=$project/out/pmos-msm8996-6.12-camera-imx298-cci400
fullout=$project/out/pmos-msm8996-6.12-recovery-audio-full-s1302-poll-drm100
symvers=$fullout/Module.symvers
module_build=$kout/imx298-module

test "$(git -C "$kernel" rev-parse --short=12 HEAD)" = d247ce811242
git -C "$kernel" log -1 --oneline
test -r "$baseout/.config"
mkdir -p "$kout"
cp "$baseout/.config" "$kout/.config"
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
