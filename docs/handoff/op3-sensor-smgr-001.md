# Agent handoff: OP3 Sensor Manager / SLPI enumeration

Task / GitHub Issue: [#13 — OP3 sensor transport and IIO enumeration](https://github.com/iamgreatwk/op3/issues/13)
Role: Implementation
Baseline commit: `4f8595b13fbd0bc0caf18897bbb3361699cbb2e5` integrated recovery checkpoint; formal 6.12.1 baseline `67b0bbc3cbf46bae712a2606a43361756fcbd829`
Working branch: top-level `agent/implementation/op3-sensor-smgr-001`; nested kernel `agent/implementation/op3-sensor-smgr-001`
Changed files: Qualcomm Sensor Manager/IIO stack, sensor config fragment, registry staging script/manifest, patch archive, handoff and test records
Commit SHA: top-level `9e565fa`; nested kernel diagnostic commit `8b9430d14e2f`

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
Device result: PROVISIONAL PASS for SLPI/QRTR/IIO transport and buffered data
stream; full hardware inventory is incomplete because the runtime inventory
does not expose all expected motion sensors
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

## Inventory diagnostic build

The follow-up diagnostic adds only `dev_info()` output for the raw Sensor
Manager inventory returned by SLPI. It does not change sensor mapping,
registration, timing, DTS, initramfs, or Buildroot. The clean build was run by
Codex under explicit owner authorization after the power interruption.

```text
Nested source commit: 9eb796adcee8
Output directory: out/pmos-msm8996-6.12-sensor-smgr-inventory
Image.gz: ac99cae3f8fafe562c3785a550fd44b64916d64720adc6afbb090c34848ebba9
DTB:      acf85fd6ae148861374ec4d65feee0e3d909cce9b75e96d09c2f44a102914d1b
Module.symvers: b18c78299914d159f7d2fc32699aabe081c4b4dc46bc00b1352017b00ca020e4
.config: c9db8711c24544950f1020a10231ca6718b900b9d7cbd23ba699073491c65acf
Temporary boot image: artifacts/boot-oneplus3-pmos612-recovery-sensor-smgr-inventory-test.img
Boot image SHA256: 6864133e7f7b99fc1eb4b027ce469f3a7a79ebdcd965e0326b5b02824605e215
Temporary initrd SHA256: 3f193f242b6c500a1c62d2b8176c834bf3365bd28dd3c5bf975b3e688c53b2b8
```

The inventory diagnostic image was boot-tested twice on 2026-09-15. Both boots
reported exactly two raw SLPI entries (`MAG`, id `0x14`, and `PROX_LIGHT`, id
`0x28`) and registered only `qcom-smgr-mag` and `qcom-smgr-prox-light`. The
second boot used an additional byte-identical
`sns.reg-oneplus,oneplus3` file; the board-specific lookup then succeeded, but
the inventory remained unchanged. This rules out the generic-registry
fallback/name as the cause of the missing accelerometer and gyroscope. The
image is intended for `fastboot boot` only; it is not a Buildroot artifact and
has not been flashed.

Naming A/B artifacts:

```text
Board-name initrd: 63556b8c16a11a184bd7239873a08e1faa4c133eb48878850554ff7b1b80b472
Board-name boot image: 640aed56eb30cbd8debbe79fa20be1526cd3444e002f14837f187ce57fd017fb
```

The next experiment must remain separate: inspect or validate the SLPI
registry/firmware sensor availability, without changing IIO type mapping or
adding guessed HLOS sensor nodes. Timestamp validation remains a separate
follow-up.

## Registry group audit diagnostic

The next one-variable kernel diagnostic logs every group request made by the
SLPI Sensor Registry service, including the static map offset/size, registry
size, response result, returned data length, and QMI send result. It does not
change the registry data, group map, response bytes, IIO mapping, or DTS.

```text
Nested source commit: 8b9430d14e2f
Archived patch: patches/pmos612-op3-sensor-smgr/0012-soc-qcom-log-sensor-registry-group-requests.patch
Patch SHA256: 2324e8ae58f0913a420bc22854d837e03588b25a79eac1e2a5c6a483d1e94530
```

The owner-authorized build used a new output directory and a fresh
`fastboot boot` test. The previous two-device inventory result was reproduced
and is not overwritten.

The registry-group audit build completed after the power interruption under
explicit owner authorization. It used the clean nested sensor branch at
`8b9430d14e2f3d0187add1dc5f7f1ae3eef413b9` and did not modify the registry
data, IIO mapping, DTS, or Buildroot. The exact build command was:

```text
make -C source/linux-pmos-msm8996-6.12-sensor-smgr \
  O=out/pmos-msm8996-6.12-sensor-smgr-regaudit ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  -j$(nproc) Image.gz dtbs modules
```

Build artifacts:

```text
c9db8711c24544950f1020a10231ca6718b900b9d7cbd23ba699073491c65acf  out/pmos-msm8996-6.12-sensor-smgr-regaudit/.config
f4a1d8446ac752eac3e6bb094b4f3ac8ca89905e9e52e1bbfc93df6eb971207e  out/pmos-msm8996-6.12-sensor-smgr-regaudit/arch/arm64/boot/Image.gz
acf85fd6ae148861374ec4d65feee0e3d909cce9b75e96d09c2f44a102914d1b  out/pmos-msm8996-6.12-sensor-smgr-regaudit/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb
b18c78299914d159f7d2fc32699aabe081c4b4dc46bc00b1352017b00ca020e4  out/pmos-msm8996-6.12-sensor-smgr-regaudit/Module.symvers
```

The temporary boot image was packaged with the byte-identical board-name
registry initrd. It has not been flashed:

```text
63556b8c16a11a184bd7239873a08e1faa4c133eb48878850554ff7b1b80b472  artifacts/initrd-op3-recovery-buildroot-sensor-smgr-boardname-test.cpio.gz
b1c791fcc24afd4ec52f28e7fb1daaa18c54e787581fd17d4304d289385bfbc1  artifacts/boot-oneplus3-pmos612-recovery-sensor-smgr-regaudit-test.img
```

Device test status: COMPLETED. The clean-boot capture contained 70 registry
group requests with `bad=0`: every request returned `result=0`, the expected
data length, and `send_ret=0`. There were no unmapped groups or registry
transport errors. The post-QMI-decode Sensor Manager log showed only two
entries, `MAG` (`0x14`) and `PROX_LIGHT` (`0x28`); IIO registered only
`qcom-smgr-mag` and `qcom-smgr-prox-light`. This proves the decoded Linux-side
result, not the exact bytes returned by SLPI. Do not change the registry group
map or client mapping on this evidence.

## Follow-up: firmware and HLOS bus A/B

After the power interruption, the previously recorded sensor test image was
booted again as an A/B control. It used the older Sensor Manager kernel at
`23b5bfc59973` and the generic-registry initrd. This image also exposed only
`qcom-smgr-mag` and `qcom-smgr-prox-light`; the earlier provisional gyro result
was not reproduced.

The registry-audit image was then booted again. The captured log included:

```text
group id=2692: result=0 data_len=256 send_ret=0
group id=2693: result=0 data_len=256 send_ret=0
group id=2900: result=0 data_len=4 send_ret=0
group id=2910: result=0 data_len=4 send_ret=0
```

The independent `sns-reg` map audit corrected the old labels: `2692` and
`2693` are the DEVINFO ACCEL/GYRO candidate groups. `2900` and `2910` are SAM
configuration groups (`basic ges` and `Facing`), not motion-sensor groups.
Successful replies show that the static registry service returned mapped
blocks; they do not show that SLPI probed a device or accepted those settings.

The same boot's post-decode log still reported exactly two inventory entries:
`MAG(0x14)` and `PROX_LIGHT(0x28)`. All local `slpi.mbn` copies, including the
verified firmware tree and preserved mainline-test copy, are byte-identical
(`5398c39071c4154cce71195848a522729ac1ff1c58f1e434bf2f821d62c2d902`). A
read-only HLOS check found clients only on I2C-0 through I2C-4 for the known
audio/power/USB/touch devices. Candidate WHO_AM_I reads for LSM6DS3/BMI160 on
I2C-0/1/2 and then I2C-3/4 all returned `ENXIO`; the latter probe was limited
to the four candidate addresses and did not unload or alter the capacitive-key
or touch clients. After the probe, the clients remained `s1302-capkey` on
I2C-3 and `rmi4-i2c` on I2C-4.

This A/B shows the same two-entry *decoded* inventory across the tested kernel
registry/userspace-registry paths and registry filename choices. It does not
exclude a QMI decode or message-boundary issue, and the HLOS I2C `ENXIO` checks
do not establish absence when the sensor may be owned by SLPI. Root cause
remains **INCONCLUSIVE**. Do not add guessed HLOS sensor nodes or change the
registry map. The next experiment is a read-only capture of the complete
all-sensor-info QMI response before decoding, compared with the existing
post-decode count/type/ID log.

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

## Follow-up: userspace `sns-reg` A/B

This follow-up isolates whether the in-kernel registry service was masking or
competing with the pmOS userspace registry. The top-level A/B configuration
commit is `3a11b30` (`sensor: add userspace registry A/B config`); it sets
`CONFIG_QCOM_SNS_REG=n` while retaining QRTR and Sensor Manager. No DTS,
IIO mapping, SLPI firmware, or Buildroot source was changed.

The owner-authorized kernel build used the clean nested sensor worktree at
`8b9430d14e2f` and output directory
`out/pmos-msm8996-6.12-sensor-smgr-userspace-registry`:

```text
9480e82306b033d4085142361728e4d06679bf83df09fc08f93e5c987eb04532  out/pmos-msm8996-6.12-sensor-smgr-userspace-registry/.config
2d1421ff69ae9f65cc6cc82cf12b5445c2c1f022bd5ba8b7e65659aad52e3f7e  out/pmos-msm8996-6.12-sensor-smgr-userspace-registry/arch/arm64/boot/Image.gz
acf85fd6ae148861374ec4d65feee0e3d909cce9b75e96d09c2f44a102914d1b  out/pmos-msm8996-6.12-sensor-smgr-userspace-registry/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb
```

The temporary initramfs used the static pmOS `sns-reg` binary and the real
`sns.reg` converted by `sns-reg-generator`, not Android's
`sensor_def_qcomdev.conf`:

```text
9941bbb3dcc16d53c80668b6b53289f14cc73dd96df465446649c128737b1fc9  sns-reg
f206f6eeb93ee1c797108eb791149b7e756b2f1723c4a6e0bd36a18eb13e3f3a  generated registry.conf (1437 numeric entries)
27e903075bb4e1519df435ea5474fe8378fc4beb3969e0ffa3d3987a2cdc9f87  temporary initrd
5f273591249130775f5ca58a60980f4f2163eeab9a1f078cdb2181463b8b5f18  temporary boot image
```

The v2 image was booted with `fastboot boot` on 2026-09-15. Device evidence:

```text
CONFIG_QCOM_SNS_REG is not set
/usr/bin/sns-reg: PID 175, PPid 1, sleeping in do_sys_poll
qcom_smgr: available sensor count=2
qcom_smgr: sensor[0]: id=0x14 type=MAG
qcom_smgr: sensor[1]: id=0x28 type=PROX_LIGHT
```

The first temporary image used the wrong Android-format text and returned
`available sensor count=0`. The corrected v2 image returned two decoded
entries, so the userspace registry process and QRTR path were active. This
does not determine whether the raw reply itself contained more entries. The
kernel/userspace registry A/B did not change the decoded result, but root cause
remains **INCONCLUSIVE** until the pre-decode reply is captured. Do not promote
this temporary userspace registry into the default recovery image or change
the registry map without new evidence.

## Owner live verification: SLPI and Sensor Manager are running (2026-09-15)

A read-only SSH inspection of the currently booted recovery image confirmed
that the transport is genuinely active, rather than merely present in the
kernel log:

```text
/sys/class/remoteproc/remoteproc0: state=running
  firmware=qcom/msm8996/oneplus3/slpi.mbn
/sys/class/remoteproc/remoteproc1: state=running
  firmware=qcom/msm8996/oneplus3/adsp.mbn

remote processor 1c00000.remoteproc is now up
qcom_smgr 9-17: available sensor count=2
qcom_smgr 9-17: sensor[0]: id=0x14 type=MAG parsed=3
qcom_smgr 9-17: sensor[1]: id=0x28 type=PROX_LIGHT parsed=4

/usr/bin/sns-reg: PID 175
iio:device0: qcom-smgr-mag
iio:device1: qcom-smgr-prox-light
```

This confirms that `SLPI firmware -> QRTR/IPCRTR -> sns-reg -> Sensor Manager
-> IIO` is active for the two decoded entries. It does not localize the missing
ACCEL/GYRO result to SLPI rather than QMI decoding. No source or artifact was
changed by this read-only verification.

## Pre-decode SMGR inventory response diagnostic (2026-09-20)

This resumes Issue #13 using the corrections from the independent six-axis
route review (`30bb4f7`) and SSI/registry audit (`c8c2c76`, report
`docs/handoff/op3-sensor-ssi-audit-001.md`). The prior `2900 (ACCEL)` and
`2910 (GYRO)` labels were incorrect: the actual DEVINFO groups are `2692`
and `2693`; `2900`/`2910` are SAM configuration groups. Existing
`available sensor count=2` logs are emitted after QMI decoding, so they do not
show whether the received packet itself contained more entries.

The next experiment changes one thing: nested kernel commit
`d0ae61511d192a2d9bd10aebca9c9a561e3a0ddd` adds an optional per-transaction
raw-response callback in the QMI core and enables it only for the SMGR
all-sensor-info query. Before decoding, it logs the sender node/port, QMI type,
transaction ID, message ID, declared payload length, packet length, and the
complete packet bytes. Existing post-decode item count/type/ID logs remain the
comparison point. Registry contents, registry group mapping, IIO mapping, DTS,
and request timing are unchanged.

Interpretation gate:

- If the captured response contains ACCEL/GYRO entries but the decoded log
  still has only MAG/PROX_LIGHT, investigate QMI decoding/message boundaries.
- If the captured response itself contains only MAG/PROX_LIGHT, conclude only
  that SLPI returned two entries for this initial query; this does not prove
  the physical IMU is absent. Timing and SLPI probe evidence remain separate
  questions.
- If the QMI header/TLV lengths are inconsistent or the payload is truncated,
  treat the result as a protocol/transport diagnostic, not a sensor result.

Build: **NOT_RUN**. Device test: **NOT_RUN**. No new artifact or device claim
is made. The next gate is a clean committed-branch kernel build, followed by an
owner-authorized temporary boot and collection of the raw response plus the
existing decoded inventory logs. Do not flash the image or change any registry,
DTS, or sensor mapping as part of this experiment.
