# OP3 six-axis route review

Date: 2026-09-15

## Completion record

- Task / GitHub Issue: owner-requested independent review and direction to Luna; [#13](https://github.com/iamgreatwk/op3/issues/13).
- Role: Review.
- Baseline commit: formal kernel `67b0bbc3cbf46bae712a2606a43361756fcbd829`; integrated recovery parent `4f8595b13fbd`; reviewed sensor kernel `8b9430d14e2f3d0187add1dc5f7f1ae3eef413b9`.
- Working branch: `agent/review/op3-six-axis-route-001`, separate worktree `/home/kai/src/oneplus3-six-axis-review-001`.
- Changed files: this report and `docs/handoff/latest.md` in the review worktree only.
- Commit SHA: the commit containing this report, based on project `789c3c39b5f4b1fb9b6be98748f9a71e79f779b7`; retrieve with `git log -1 --format=%H -- docs/handoff/op3-six-axis-route-review-001.md`.
- Layer: evidence review of SSC registry and SMGR enumeration.
- Hypothesis tested: existing evidence fully excludes registry semantics and QMI decoding as causes of missing motion sensors.
- Only variable changed: documentation; no runtime or source behavior changed.
- Build run by project owner: none in this review.
- Build result: NOT_RUN.
- Artifacts and SHA256: existing audit inputs listed below; no new device artifact.
- Device test run by project owner: none in this review.
- Device result: NOT_RUN.
- Evidence links / log paths: listed below; reviewed source and existing captures only.
- Conclusion: REJECTED for the claimed complete exclusion. Root cause remains INCONCLUSIVE.
- Uncertainties: actual bus encoding and physical wiring, firmware/registry schema agreement, raw QMI response, delayed enumeration, SLPI probe failure reason.
- Recommended next experiment: first complete the bounded offline SSI audit assigned below; then prepare a read-only, time-indexed raw SMGR inventory query on a fixed existing image. Device execution needs authorization covering that test.

## Judgment

The working route remains SLPI/SSC → QRTR → SMGR → IIO. Repeated enumeration of MAG and PROX_LIGHT supports a functioning transport. It does not establish a working six-axis IMU. The one early gyro result is not a reproducible milestone.

The supplied review is right to reject guessed HLOS I2C nodes and unrelated Buildroot changes. However, it overstates what the registry audit and inventory log prove. Neither a firmware replacement nor a physical sensor failure conclusion is justified yet.

## Confirmed corrections

### 1. Registry group IDs were confused with sensor identities

The checked `sns-reg` source at commit `4d238e5f0baba3fb77456fe2bffbf8e8f18a71a0`, `map.c:1022`, maps group **2900 to item 2800**, and group **2910 to item 2900**. The extracted vendor configuration labels those items `basic ges` and `Facing` under SAM configuration. These are not the DEVINFO ACCEL/GYRO groups.

The same map links **2692 to DEVINFO ACCEL keys 2000…** and **2693 to DEVINFO GYRO keys 2100…**. Group 2690 contains SMGR configuration starting with keys 1900…. Group IDs and item IDs must remain separate columns in any audit.

The saved device log also shows successful responses for groups 2692 and 2693, so correcting the labels alone does not demonstrate a new missing request or fix. Their returned bytes and interpretation remain to be checked.

### 2. Successful registry responses do not validate their meaning

In `drivers/soc/qcom/qcom_sns_reg.c`, success is assigned after a static offset/size lookup and bounds check, followed by `memcpy` from the firmware file. It does not validate a driver UUID, bus identifier, voltage resource, sensor presence, or firmware schema.

The kernel implementation explicitly derives from the userspace `sns-reg` implementation. The existing generator and userspace daemon use related maps and the same input binary. Their A/B result therefore rejects switching implementation as a sufficient fix; it is not independent confirmation that the shared configuration and layout are correct. The [upstream registry documentation](https://gitlab.com/lucaweiss/sns-reg/-/blob/master/README.md) explains that the group mapping was reconstructed from Android transactions and that a group-size validator checks that mapping.

### 3. The inventory log is after QMI decoding

In `drivers/iio/common/qcom_smgr/qcom_smgr.c:29`, `qmi_txn_wait` completes before `resp.item_len` and `resp.items` are logged. The entries are visible before IIO filtering, but after QMI decoding. Calling this wire-level/raw-TLV evidence is inaccurate.

The current result argues against later IIO type filtering as the explanation. A decoder or message-boundary issue has not been directly tested. No decoder bug was demonstrated by this review, and no parser change is recommended without a contrary raw response.

### 4. A concrete SSC bus clue was overlooked

In the extracted vendor configuration, keys 2009/2109 contain `0x1001`, and the adjacent 2010/2110 fields contain zero with a SPI chip-select comment. The generated binary registry values agree; existing SMGR keys 1906/1922 also hold 4097 and 1912/1928 hold zero.

This is a strong reason to investigate the SSC SPI bus encoding. It is not yet proof of the handset's wiring or permission to create an AP SPI device. The values must not be translated into Linux `i2c-1`, I2C address zero, AP GPIO 117, or a named PMIC rail without the corresponding source definitions and ownership evidence. The negative HLOS I2C reads do not settle this path.

### 5. Initial enumeration timing is not fully tested

The saved registry-audit log shows the initial inventory at about 3.206 seconds and further registry requests afterward. That is a timing clue, not proof of a race: later requests can be unrelated algorithm setup. A second read of `/sys/bus/iio/devices` merely observes devices created by the initial query; it is not a fresh SMGR inventory transaction.

## Directed next work

Instructions were sent to Luna's existing task `01a07417-deb0-7521-9303-1eb864dbfaee`, titled **移植 recovery 启动 Wayland 浏览器**, using the project's subtask template.

The bounded assignment is an offline research audit in a separate top-level worktree `/home/kai/src/oneplus3-imu-ssi-audit-001`, branch `agent/research/op3-imu-ssi-audit-001`. It must:

1. Decode groups 2690/2692/2693 into a table of group, item, offset, width, value, and source-defined meaning. Include UUID, version, candidate count, bus, chip select/address, IRQ, flags, rail encoding, and startup delay fields. Preserve padding when comparing payloads.
2. Resolve `0x1001` from a pinned primary SSC/DDF source. Record provenance and platform differences; leave unsupported mappings unknown.
3. Compare the existing binary, generated text, kernel payload construction, and userspace payload construction without treating one derived representation as an independent oracle.
4. Correct the group-label and raw-log claims in its own branch, preserving historical test results.
5. Produce at most one next experiment and its exact evidence gate. No new builds, device requests, module reloads, firmware replacements, or writes are authorized by this audit assignment.

Suggested follow-up diagnostic, contingent on the audit: an existing or small read-only userspace SMGR query tool on a fixed image. Discover the live endpoint rather than hard-coding port 17. Collect full response bytes, actual length, transaction/message identity, result, TLV lengths, count, IDs and type strings at specified elapsed times. Query only documented inventory messages; do not send guessed commands or restart remoteproc to obtain a sample.

Decision gates:

- Raw response contains ACCEL/GYRO but decoded list does not: isolate QMI decode/buffer handling next.
- Delayed raw inventory gains ACCEL/GYRO: isolate initial-query timing next; do not mix with registry changes.
- Raw inventory remains two devices and an independently supported SSI field mismatch exists: one configuration-field A/B, with the remaining firmware, kernel and payload fixed.
- Raw inventory remains two and configuration is independently supported: seek SLPI probe logs or a same-handset stock-system health comparison, subject to separate owner authorization. Do not infer hardware failure from absence alone.

Eventual device PASS requires repeatable motion-sensor enumeration and buffered data that changes appropriately with orientation/movement across fresh boots. Bytes returned or an IIO directory alone are insufficient. Integration acceptance remains with the assigned Integration role after the owner's device report.

## Audited inputs

| Input | SHA256 / version |
| --- | --- |
| `/home/kai/op3-recovery-external-inputs/sensors/sns.reg` and existing extracted `/tmp/op3-persist-inspect.wSnBqt/sns.reg` | `2644c56bce535a7c8930e497d2f36601b302573a358493fad2732d1109518f06` |
| `/tmp/op3-vendor-sensor-inspect.S4pPon/sensor_def_qcomdev.conf` | `3364e90c0acbe0706f3f23f20870ca05c3e7de6ff62e3c490190e3dff298635b` |
| `/tmp/op3-sensor-registry.conf` | `f206f6eeb93ee1c797108eb791149b7e756b2f1723c4a6e0bd36a18eb13e3f3a` |
| `/tmp/op3-sns-reg-src.S9P88t/map.c` | `adf458c8604959b268321ed1f3f2af48117f05c3d88bd94629071a7c33ea00e5` |
| `sns-reg` source repository | `4d238e5f0baba3fb77456fe2bffbf8e8f18a71a0` |
| Existing `/tmp/op3-sensor-smgr-regaudit-device.log` | Source of the quoted request and inventory sequence; no new capture |

The extracted files remain evidence copies in temporary directories. Their matching names alone do not certify stock-system provenance; the research audit must tie them back to the original backup and preserve hashes. No proprietary firmware or full device identifiers are copied into this report.
