# Agent handoff: OP3 Sensor Manager / SLPI enumeration

Task / GitHub Issue: [#13 — OP3 sensor transport and IIO enumeration](https://github.com/iamgreatwk/op3/issues/13)
Role: Implementation
Baseline commit: `4f8595b13fbd0bc0caf18897bbb3361699cbb2e5` integrated recovery checkpoint; formal 6.12.1 baseline `67b0bbc3cbf46bae712a2606a43361756fcbd829`
Working branch: top-level `agent/implementation/op3-sensor-smgr-001`; nested kernel `agent/implementation/op3-sensor-smgr-001`
Changed files: Qualcomm Sensor Manager/IIO stack, sensor config fragment, registry staging script/manifest, patch archive, handoff and test records
Commit SHA: top-level `e5c61a0`; nested kernel through diagnostic commit `9eb796adcee8`

Layer: Linux kernel sensor transport and IIO enumeration
Hypothesis tested: The physical OP3 motion/environment sensors are exposed through the existing SLPI/SSC Sensor Manager path, so the 6.12 kernel can enumerate them without inventing direct HLOS I²C nodes.
Only variable changed: add the Qualcomm sns-reg service, SLPI Sensor Manager transport, and IIO clients; no camera, DRM, recovery UI, Buildroot configuration, or DTS wiring was changed.

Build run: Codex agent under explicit owner authorization on 2026-09-14
Build result: PASS
Output directory: `out/pmos-msm8996-6.12-sensor-smgr`
Command: `make -C source/linux-pmos-msm8996-6.12-sensor-smgr O=out/pmos-msm8996-6.12-sensor-smgr ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 -j$(nproc) Image.gz dtbs modules`
Artifacts and SHA256:

```text
c9db8711c24544950f1020a10231ca6718b900b9d7cbd23ba699073491c65acf  out/pmos-msm8996-6.12-sensor-smgr/.config
df8b5a032542f0c0ebba952e2260afdf4e85eeacf2b705636188d4fe69ec725e  out/pmos-msm8996-6.12-sensor-smgr/arch/arm64/boot/Image.gz
acf85fd6ae148861374ec4d65feee0e3d909cce9b75e96d09c2f44a102914d1b  out/pmos-msm8996-6.12-sensor-smgr/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb
b18c78299914d159f7d2fc32699aabe081c4b4dc46bc00b1352017b00ca020e4  out/pmos-msm8996-6.12-sensor-smgr/Module.symvers
```

Device test run: Codex agent under explicit owner authorization on 2026-09-14
Device result: PROVISIONAL PASS for SLPI/QRTR/IIO enumeration and buffered data
stream; full hardware inventory is incomplete because no accelerometer IIO
device appeared
Evidence links / log paths: Issue #13; vendor `sensor_def_qcomdev.conf` and `hals.conf` from the preserved OnePlus 3 backup; downstream OnePlus/Lineage kernel source; `/tmp/op3-snsreg.RYqTOf` source hash `2644c56bce535a7c8930e497d2f36601b302573a358493fad2732d1109518f06`; `/tmp/op3-sensor-smgr-kernel-build-final.log` (final clean incremental pass); `/tmp/op3-sensor-smgr-kernel-build.log` (earlier compatibility-fix attempts); `/tmp/op3-sensor-smgr-object-build.log`; `/tmp/op3-sensor-smgr-affected-build.log`; `/tmp/op3-sensor-smgr-pack.log`

Temporary device-test package prepared on 2026-09-14 (not an accepted recovery
artifact and not flashed): the existing Buildroot initrd was repacked with only
`lib/firmware/qcom/sensors/sns.reg`, then paired with the sensor branch's
`Image.gz` and unchanged OP3 DTB using `scripts/pack-boot.sh` and
`boot/oneplus3-fa5.env`.

```text
3f193f242b6c500a1c62d2b8176c834bf3365bd28dd3c5bf975b3e688c53b2b8  artifacts/initrd-op3-recovery-buildroot-sensor-smgr-test.cpio.gz
8c94cbf7914dc6511a5727c711489e9cfcae23f5df7d0d0cd1c63953ba9df625  artifacts/boot-oneplus3-pmos612-recovery-sensor-smgr-test.img
```

Device evidence from the fresh temporary `fastboot boot` run: the phone stayed
in recovery and SSH became available. The generic registry fallback loaded
successfully after the board-specific lookup returned `-ENOENT`. The kernel
enumerated `qcom-smgr-gyro` at 200 Hz, `qcom-smgr-mag` at 52 Hz, and
`qcom-smgr-prox-light` at 5 Hz. Enabling and disabling each IIO buffer returned
zero; buffered reads returned 96, 96, and 32 bytes respectively, with changing
non-zero gyro, magnetometer, and proximity values. `iio_info` is not installed
in this recovery image. The next isolated follow-up is to account for the
missing accelerometer and validate/fix the IIO timestamp field; no Buildroot
integration or default recovery promotion is justified yet.

Conclusion: INCONCLUSIVE
Uncertainties: The vendor registry is a required external binary and is now locked but not committed; the exact physical sensor variants must be confirmed from runtime IIO channels; the SLPI firmware must be present and must accept this registry on the device build. The build embeds the separately locked ath10k files from the external-input directory; they are not part of the sensor source archive.
Recommended next experiment: create a separate one-variable follow-up for the
missing accelerometer (first determine whether the vendor registry/SLPI report
contains an `ACCEL` entry, then change only the matching or client mapping).
Keep timestamp validation as another isolated follow-up. Test pressure and
other sensor classes only after those gates.

## Static implementation record

Nested kernel commits:

```text
b853c4b962fa  iio: Add Qualcomm Sensor Manager driver
e17f973f5fa8  soc: qcom: Add in-kernel sensors registry implementation.
d19b3b6bddaa  remoteproc: qcom: Enable in-kernel sns-reg
8c1093c17676  net: qrtr: Turn QRTR into a bus
a2af2e73df40  net: qrtr: Define macro to convert QMI version and instance to QRTR instance
603c36de0534  modpost: keep QRTR alias in current devtable API
f6d4b4706f83  iio: qcom: Adapt Sensor Manager to Linux 6.12 APIs
76822cd7b4ae  net: qrtr: connect SMD service device callbacks
 b93eacd248c3  net: qrtr: use token namespace for Linux 6.12 exports
23b5bfc59973  iio: qcom: Select kfifo buffer for Sensor Manager
9eb796adcee8  iio: qcom: log Sensor Manager inventory
```

The OP3 device tree already enables `slpi_pil` with
`qcom/msm8996/oneplus3/slpi.mbn`, so this experiment intentionally adds no
new sensor DTS node. The default recovery profile remains unchanged until an
owner-run sensor build and device result are available.
