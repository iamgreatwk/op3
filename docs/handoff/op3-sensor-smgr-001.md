# Agent handoff: OP3 Sensor Manager / SLPI enumeration

Task / GitHub Issue: [#13 — OP3 sensor transport and IIO enumeration](https://github.com/iamgreatwk/op3/issues/13)
Role: Implementation
Baseline commit: `4f8595b13fbd0bc0caf18897bbb3361699cbb2e5` integrated recovery checkpoint; formal 6.12.1 baseline `67b0bbc3cbf46bae712a2606a43361756fcbd829`
Working branch: top-level `agent/implementation/op3-sensor-smgr-001`; nested kernel `agent/implementation/op3-sensor-smgr-001`
Changed files: Qualcomm Sensor Manager/IIO stack, sensor config fragment, registry staging script/manifest, patch archive, handoff and test records
Commit SHA: top-level archive checkpoint pending; nested kernel through `23b5bfc59973`

Layer: Linux kernel sensor transport and IIO enumeration
Hypothesis tested: The physical OP3 motion/environment sensors are exposed through the existing SLPI/SSC Sensor Manager path, so the 6.12 kernel can enumerate them without inventing direct HLOS I²C nodes.
Only variable changed: add the Qualcomm sns-reg service, SLPI Sensor Manager transport, and IIO clients; no camera, DRM, recovery UI, Buildroot configuration, or DTS wiring was changed.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: none

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: Issue #13; vendor `sensor_def_qcomdev.conf` and `hals.conf` from the preserved OnePlus 3 backup; downstream OnePlus/Lineage kernel source; `/tmp/op3-snsreg.RYqTOf` source hash `2644c56bce535a7c8930e497d2f36601b302573a358493fad2732d1109518f06`

Conclusion: INCONCLUSIVE
Uncertainties: The vendor registry is a required external binary and is now locked but not committed; the exact physical sensor variants must be confirmed from runtime IIO channels; the SLPI firmware must be present and must accept this registry on the owner build.
Recommended next experiment: stage `sensors/sns.reg` into `artifacts/op3-initramfs-firmware`, build the sensor fragment into the clean recovery kernel, boot once, and collect `dmesg`, `/sys/bus/iio/devices`, and `iio_info`/raw channel data. Treat one stable accelerometer or gyroscope stream as the first PASS gate; test magnetometer, proximity/light, and pressure only after that gate.

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
```

The OP3 device tree already enables `slpi_pil` with
`qcom/msm8996/oneplus3/slpi.mbn`, so this experiment intentionally adds no
new sensor DTS node. The default recovery profile remains unchanged until an
owner-run sensor build and device result are available.
