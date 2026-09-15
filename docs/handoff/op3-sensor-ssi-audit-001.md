# OP3 六轴传感器 SSI/注册表离线审计交接

日期：2026-09-15  
用途：交给其它模型复盘“为什么 SLPI 只公布 MAG/PROX_LIGHT，而 ACCEL/GYRO 没有出现”。  
范围：只做主机侧静态审计和既有日志审计；本轮没有编译、刷机、设备测试或内核/设备树修改。

## Agent handoff template

```text
Task / GitHub Issue: [#13 — OP3 sensor transport and IIO enumeration](https://github.com/iamgreatwk/op3/issues/13)
Role: Research / offline SSI and Sensor Registry audit
Baseline commit: formal pmOS MSM8996 Linux 6.12.1 `67b0bbc3cbf46bae712a2606a43361756fcbd829`; project checkout `789c3c39b5f4b1fb9b6be98748f9a71e79f779b7`
Working branch: `agent/research/op3-imu-ssi-audit-001`
Changed files: `docs/handoff/op3-sensor-ssi-audit-001.md`, `docs/handoff/op3-six-axis-sensor-review-001.md`, `docs/handoff/op3-sensor-smgr-001.md`, `docs/handoff/latest.md`, `docs/test-matrix.md`
Commit SHA: `89c4295` (report and wording corrections)

Layer: host-side evidence / sensor registry and SLPI transport audit
Hypothesis tested: the missing ACCEL/GYRO may be explained by a wrong registry-group interpretation, a wrong binary offset/width map, a mismatch between the kernel and userspace SNS_REG response paths, or an unexamined SSI/SPI clue.
Only variable changed: none; all inputs and logs were read-only.

Build run by project owner: no
Build result: NOT_RUN
Artifacts and SHA256: none produced by this audit

Device test run by project owner: no
Device result: NOT_RUN
Evidence links / log paths: Issue #13; `/tmp/op3-sensor-smgr-regaudit-device.log`; `/home/kai/op3-recovery-external-inputs/sensors/sns.reg`; `/tmp/op3-sensor-registry.conf`; `/tmp/op3-vendor-sensor-inspect.S4pPon/sensor_def_qcomdev.conf`; `/tmp/op3-sns-reg-src.S9P88t/`; `patches/pmos612-op3-sensor-smgr/`

Conclusion: INCONCLUSIVE
Uncertainties: the physical six-axis part, its actual AP/SLPI bus wiring, the authoritative Qualcomm `sns_reg_api_v02.h`/DDF bus definitions, and SLPI's internal probe failure are not available in this audit. The value `0x1001` is a strong SSI/SPI-related clue from vendor comments, but it does not prove the physical bus or authorize an HLOS node.
Recommended next experiment: on one fixed, already bootable image, add only a read-only wire-level Sensor Manager response diagnostic at the QMI decode boundary, recording the response item count, type/id, and length before client mapping; do not change registry bytes, map entries, DTS, or IIO mapping. PASS would show ACCEL/GYRO in the wire response but lost during parsing; FAIL would show that SLPI itself returns only MAG/PROX_LIGHT.
```

## 1. Audit result in one paragraph

The registry transport is functioning. On the existing audit boot, the kernel
served 70 mapped groups with `result=0`, expected lengths, and `send_ret=0`.
The relevant six-axis configuration groups are `2692` (DEVINFO ACCEL) and
`2693` (DEVINFO GYRO), and both were served successfully. The previously used
labels `2900 (ACCEL)` and `2910 (GYRO)` were wrong: the map says `2900` contains
item `2800` (`basic ges`) and `2910` contains item `2900` (`Facing`), both SAM
configuration values. This correction removes a misleading argument, but it
does not explain why the later Sensor Manager inventory contains only MAG and
PROX_LIGHT. The remaining boundary is still SLPI-side discovery, physical
transport/power/IRQ, firmware variant, or a parser issue that cannot be
distinguished from the current post-decode log.

## 2. Evidence inputs and provenance

| Input | Meaning | SHA256 / revision |
| --- | --- | --- |
| `/home/kai/op3-recovery-external-inputs/sensors/sns.reg` | binary registry staged for the recovery sensor line | `2644c56bce535a7c8930e497d2f36601b302573a358493fad2732d1109518f06` |
| `/tmp/op3-persist-inspect.wSnBqt/sns.reg` | read-only persist copy | same SHA256 as above |
| `/tmp/op3-vendor-sensor-inspect.S4pPon/sensor_def_qcomdev.conf` | vendor development/default registry text | `3364e90c0acbe0706f3f23f20870ca05c3e7de6ff62e3c490190e3dff298635b` |
| `/tmp/op3-sensor-registry.conf` | numeric registry generated from the binary registry in an earlier run | `f206f6eeb93ee1c797108eb791149b7e756b2f1723c4a6e0bd36a18eb13e3f3a` |
| `/tmp/op3-sns-reg-src.S9P88t/map.c` | group/key map used for binary decoding | `adf458c8604959b268321ed1f3f2af48117f05c3d88bd94629071a7c33ea00e5` |
| `/tmp/op3-sns-reg-src.S9P88t/generator.c` | generator implementation used for map-to-text decoding | `386ac620a334d00184cb41bd808e0b8c7fccf0196048a30a39acdbe025225ece` |
| `/tmp/op3-sns-reg-src.S9P88t/validator.c` | static group-size validator | `f3336baf4a8c038e9287084f721191d0b324ceb1ba51683e84bcf38f3ac77d20` |
| `/tmp/op3-sensor-smgr-regaudit-device.log` | existing owner boot log used for timeline only | `d1a19b160b150378e94bdd459f9b1595da9467bc93661a25bab3daa33fb76556` |
| local `sns-reg` source | map/generator project provenance | commit `4d238e5f0baba3fb77456fe2bffbf8e8f18a71a0`, origin `https://gitlab.com/msm8996-mainline/sns-reg` |

The public project architecture note is the
[`sns-reg` README](https://gitlab.com/lucaweiss/sns-reg/-/blob/master/README.md).
It supports the general AP-served SNS_REG / SSC model; it does not provide the
missing handset-specific Qualcomm DDF bus definition.

The vendor text contains explicit comments for the `2000`/`2100` families:
`2000/2001` and `2100/2101` enable and count ACCEL/GYRO candidates; the
`2002..2071` and `2102..2171` fields include UUID, timing, GPIO1, registry and
calibration groups, the vendor-labelled `I2C_BUS`, `CS for SPI`, flags, VDD,
and VDDIO. The vendor text itself contains only `1900` and `1901` for the
SMGR version; the full `1902..1996` semantics are not present in the local
authoritative input.

## 3. Correct group and item identity

The map at sns-reg commit `4d238e5f0baba3fb77456fe2bffbf8e8f18a71a0` gives:

| Group requested by SLPI | Binary block | Group meaning supported by local evidence | Important correction |
| ---: | --- | --- | --- |
| `2690` | offset `0x1700`, size `0x100` | SMGR configuration block; individual `1902..1996` semantics are not available locally | not the ACCEL/GYRO inventory itself |
| `2692` | offset `0x1800`, size `0x100` | DEVINFO ACCEL candidate configurations | true ACCEL candidate group |
| `2693` | offset `0x1900`, size `0x100` | DEVINFO GYRO candidate configurations | true GYRO candidate group |
| `2800` | offset `0x1f00`, size `0x22` | separate group containing item IDs beginning at `2700` | not item `2800` |
| `2900` | offset `0x2000`, size `0x004` | one four-byte SAM item | contains item `2800` (`basic ges`) |
| `2910` | offset `0x2100`, size `0x004` | one four-byte SAM item | contains item `2900` (`Facing`) |

The `groups[]` and `group_map[]` definitions are independent tables. A group
ID is not automatically the same as an item ID. In particular, `group 2900`
must not be renamed ACCEL merely because its numeric value is close to the
DEVINFO group IDs.

## 4. Exact binary map audit

All values below are decoded little-endian from the binary `sns.reg`. `abs` is
the absolute byte offset in that file, `rel` is relative to the group block,
and `w` is the mapped width. The UUID values are shown in their normal numeric
form; the raw bytes are reversed by the little-endian decode.

### 4.1 Group 2690: SMGR configuration block

The first two bytes are the SMGR version (`1900=0`, `1901=2`). There are 14
padding bytes before the first 16-byte positional configuration and 4 padding
bytes between configuration blocks. The local map identifies the keys and
widths, but no local primary header gives the exact semantic names for
`1902..1996`. Therefore “UUID/timing/bus-like” below is a positional or value
comparison only, not a protocol assertion.

| Group | Item | abs | rel | w | Value | Meaning status |
| ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 2690 | 1900 | `0x1700` | `0x00` | 1 | `0` | SMGR major version |
| 2690 | 1901 | `0x1701` | `0x01` | 1 | `2` | SMGR minor version |
| 2690 | 1902 | `0x1710` | `0x10` | 8 | `0x1246e1cb09a92baa` | UUID-like; same as LSM6DS3 entry in DEVINFO |
| 2690 | 1903 | `0x1718` | `0x18` | 8 | `0x1a0bd9d5956c508e` | UUID-like; same as LSM6DS3 entry in DEVINFO |
| 2690 | 1904 | `0x1720` | `0x20` | 4 | `10009` | timing-like; exact field name unknown |
| 2690 | 1905 | `0x1724` | `0x24` | 4 | `0` | timing-like; exact field name unknown |
| 2690 | 1906 | `0x1728` | `0x28` | 2 | `0x1001` (`4097`) | bus-related clue; semantic not proven |
| 2690 | 1907 | `0x172a` | `0x2a` | 2 | `1000` | schema unknown |
| 2690 | 1908 | `0x172c` | `0x2c` | 2 | `0` | schema unknown |
| 2690 | 1909 | `0x172e` | `0x2e` | 2 | `117` | GPIO-like value by comparison only |
| 2690 | 1910 | `0x1730` | `0x30` | 2 | `0xffff` | schema unknown |
| 2690 | 1911 | `0x1732` | `0x32` | 1 | `0` | schema unknown |
| 2690 | 1912 | `0x1733` | `0x33` | 1 | `0` | address/CS-like clue by comparison only |
| 2690 | 1913 | `0x1734` | `0x34` | 1 | `1` | schema unknown |
| 2690 | 1914 | `0x1735` | `0x35` | 1 | `0` | schema unknown |
| 2690 | 1915 | `0x1736` | `0x36` | 1 | `0xff` | schema unknown |
| 2690 | 1916 | `0x1737` | `0x37` | 1 | `2` | schema unknown |
| 2690 | 1917 | `0x1738` | `0x38` | 1 | `0xd0` (`208`) | flags-like by comparison only |
| 2690 | 1982 | `0x1739` | `0x39` | 1 | `0` | extra/supply-like; exact field unknown |
| 2690 | 1987 | `0x173a` | `0x3a` | 1 | `2` | extra/supply-like; exact field unknown |
| 2690 | 1988 | `0x173b` | `0x3b` | 1 | `2` | extra/supply-like; exact field unknown |
| 2690 | 1918 | `0x1740` | `0x40` | 8 | `0x1246e1cb09a92baa` | UUID-like; same as LSM6DS3 entry in DEVINFO |
| 2690 | 1919 | `0x1748` | `0x48` | 8 | `0x1a0bd9d5956c508e` | UUID-like; same as LSM6DS3 entry in DEVINFO |
| 2690 | 1920 | `0x1750` | `0x50` | 4 | `10009` | timing-like; exact field name unknown |
| 2690 | 1921 | `0x1754` | `0x54` | 4 | `120025` | timing-like; exact field name unknown |
| 2690 | 1922 | `0x1758` | `0x58` | 2 | `0x1001` (`4097`) | bus-related clue; semantic not proven |
| 2690 | 1923 | `0x175a` | `0x5a` | 2 | `1010` | schema unknown |
| 2690 | 1924 | `0x175c` | `0x5c` | 2 | `10` | schema unknown |
| 2690 | 1925 | `0x175e` | `0x5e` | 2 | `117` | GPIO-like value by comparison only |
| 2690 | 1926 | `0x1760` | `0x60` | 2 | `0xffff` | schema unknown |
| 2690 | 1927 | `0x1762` | `0x62` | 1 | `10` | schema unknown |
| 2690 | 1928 | `0x1763` | `0x63` | 1 | `0` | address/CS-like clue by comparison only |
| 2690 | 1929 | `0x1764` | `0x64` | 1 | `3` | schema unknown |
| 2690 | 1930 | `0x1765` | `0x65` | 1 | `0` | schema unknown |
| 2690 | 1931 | `0x1766` | `0x66` | 1 | `0xff` | schema unknown |
| 2690 | 1932 | `0x1767` | `0x67` | 1 | `2` | schema unknown |
| 2690 | 1933 | `0x1768` | `0x68` | 1 | `0xd0` (`208`) | flags-like by comparison only |
| 2690 | 1983 | `0x1769` | `0x69` | 1 | `0` | extra/supply-like; exact field unknown |
| 2690 | 1989 | `0x176a` | `0x6a` | 1 | `2` | extra/supply-like; exact field unknown |
| 2690 | 1990 | `0x176b` | `0x6b` | 1 | `2` | extra/supply-like; exact field unknown |

These values show a repeated LSM6DS3 UUID and `0x1001` clue, but they do not
prove that an LSM6DS3 is physically connected, nor that `0x1001` means a
particular AP-visible bus. The vendor comments on the separate DEVINFO fields
are stronger than a positional interpretation of this SMGR block.

### 4.2 Group 2692: DEVINFO ACCEL

The vendor text gives the semantic names for these fields. The two configured
candidates are LSM6DS3 (cfg0) and BMI160 (cfg1).

| Group | Item | abs | rel | w | Value | Meaning from vendor config |
| ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 2692 | 2000 | `0x1800` | `0x00` | 1 | `2` | enable/candidate configuration value |
| 2692 | 2001 | `0x1801` | `0x01` | 1 | `2` | number of ACCEL sensors to auto-detect |
| 2692 | 2002 | `0x1804` | `0x04` | 8 | `0x1246e1cb09a92baa` | cfg0 LSM6DS3 UUID high |
| 2692 | 2003 | `0x180c` | `0x0c` | 8 | `0x1a0bd9d5956c508e` | cfg0 LSM6DS3 UUID low |
| 2692 | 2004 | `0x1814` | `0x14` | 4 | `10000` | cfg0 OFF_TO_IDLE |
| 2692 | 2005 | `0x1818` | `0x18` | 4 | `0` | cfg0 IDLE_TO_READY |
| 2692 | 2006 | `0x181c` | `0x1c` | 2 | `117` | cfg0 GPIO1 |
| 2692 | 2007 | `0x181e` | `0x1e` | 2 | `1000` | cfg0 registry group |
| 2692 | 2008 | `0x1820` | `0x20` | 2 | `0` | cfg0 calibration-priority group |
| 2692 | 2009 | `0x1822` | `0x22` | 2 | `0x1001` (`4097`) | cfg0 vendor-labelled I2C_BUS |
| 2692 | 2010 | `0x1824` | `0x24` | 1 | `0` | cfg0 vendor-labelled CS for SPI |
| 2692 | 2011 | `0x1825` | `0x25` | 1 | `2` | cfg0 sensitivity |
| 2692 | 2012 | `0x1826` | `0x26` | 1 | `0xd0` (`208`) | cfg0 flags |
| 2692 | 2068 | `0x1827` | `0x27` | 1 | `2` | cfg0 VDD |
| 2692 | 2069 | `0x1828` | `0x28` | 1 | `2` | cfg0 VDDIO |
| 2692 | 2013 | `0x182e` | `0x2e` | 8 | `0xd646cb83ec0cd5a5` | cfg1 BMI160 UUID high |
| 2692 | 2014 | `0x1836` | `0x36` | 8 | `0x0f4d0fd654c7eab5` | cfg1 BMI160 UUID low |
| 2692 | 2015 | `0x183e` | `0x3e` | 4 | `10000` | cfg1 OFF_TO_IDLE |
| 2692 | 2016 | `0x1842` | `0x42` | 4 | `120000` | cfg1 IDLE_TO_READY |
| 2692 | 2017 | `0x1846` | `0x46` | 2 | `117` | cfg1 GPIO1 |
| 2692 | 2018 | `0x1848` | `0x48` | 2 | `1000` | cfg1 registry group |
| 2692 | 2019 | `0x184a` | `0x4a` | 2 | `0` | cfg1 calibration-priority group |
| 2692 | 2020 | `0x184c` | `0x4c` | 2 | `0x1001` (`4097`) | cfg1 vendor-labelled I2C_BUS |
| 2692 | 2021 | `0x184e` | `0x4e` | 1 | `0` | cfg1 vendor-labelled CS for SPI |
| 2692 | 2022 | `0x184f` | `0x4f` | 1 | `1` | cfg1 sensitivity |
| 2692 | 2023 | `0x1850` | `0x50` | 1 | `0xd0` (`208`) | cfg1 flags |
| 2692 | 2070 | `0x1851` | `0x51` | 1 | `2` | cfg1 VDD |
| 2692 | 2071 | `0x1852` | `0x52` | 1 | `2` | cfg1 VDDIO |

### 4.3 Group 2693: DEVINFO GYRO

Group 2693 has the same layout as 2692 with item IDs offset by 100. The vendor
text gives the same two candidate parts for the GYRO function.

| Group | Item | abs | rel | w | Value | Meaning from vendor config |
| ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 2693 | 2100 | `0x1900` | `0x00` | 1 | `2` | enable/candidate configuration value |
| 2693 | 2101 | `0x1901` | `0x01` | 1 | `2` | number of GYRO sensors to auto-detect |
| 2693 | 2102 | `0x1904` | `0x04` | 8 | `0x1246e1cb09a92baa` | cfg0 LSM6DS3 UUID high |
| 2693 | 2103 | `0x190c` | `0x0c` | 8 | `0x1a0bd9d5956c508e` | cfg0 LSM6DS3 UUID low |
| 2693 | 2104 | `0x1914` | `0x14` | 4 | `10000` | cfg0 OFF_TO_IDLE |
| 2693 | 2105 | `0x1918` | `0x18` | 4 | `120000` | cfg0 IDLE_TO_READY |
| 2693 | 2106 | `0x191c` | `0x1c` | 2 | `117` | cfg0 GPIO1 |
| 2693 | 2107 | `0x191e` | `0x1e` | 2 | `1010` | cfg0 registry group |
| 2693 | 2108 | `0x1920` | `0x20` | 2 | `10` | cfg0 calibration-priority group |
| 2693 | 2109 | `0x1922` | `0x22` | 2 | `0x1001` (`4097`) | cfg0 vendor-labelled I2C_BUS |
| 2693 | 2110 | `0x1924` | `0x24` | 1 | `0` | cfg0 vendor-labelled CS for SPI |
| 2693 | 2111 | `0x1925` | `0x25` | 1 | `2` | cfg0 sensitivity |
| 2693 | 2112 | `0x1926` | `0x26` | 1 | `0xd0` (`208`) | cfg0 flags |
| 2693 | 2168 | `0x1927` | `0x27` | 1 | `2` | cfg0 VDD |
| 2693 | 2169 | `0x1928` | `0x28` | 1 | `2` | cfg0 VDDIO |
| 2693 | 2113 | `0x192e` | `0x2e` | 8 | `0xd646cb83ec0cd5a5` | cfg1 BMI160 UUID high |
| 2693 | 2114 | `0x1936` | `0x36` | 8 | `0x0f4d0fd654c7eab5` | cfg1 BMI160 UUID low |
| 2693 | 2115 | `0x193e` | `0x3e` | 4 | `10000` | cfg1 OFF_TO_IDLE |
| 2693 | 2116 | `0x1942` | `0x42` | 4 | `120000` | cfg1 IDLE_TO_READY |
| 2693 | 2117 | `0x1946` | `0x46` | 2 | `117` | cfg1 GPIO1 |
| 2693 | 2118 | `0x1948` | `0x48` | 2 | `1010` | cfg1 registry group |
| 2693 | 2119 | `0x194a` | `0x4a` | 2 | `10` | cfg1 calibration-priority group |
| 2693 | 2120 | `0x194c` | `0x4c` | 2 | `0x1001` (`4097`) | cfg1 vendor-labelled I2C_BUS |
| 2693 | 2121 | `0x194e` | `0x4e` | 1 | `0` | cfg1 vendor-labelled CS for SPI |
| 2693 | 2122 | `0x194f` | `0x4f` | 1 | `4` | cfg1 sensitivity |
| 2693 | 2123 | `0x1950` | `0x50` | 1 | `0xd0` (`208`) | cfg1 flags |
| 2693 | 2170 | `0x1951` | `0x51` | 1 | `2` | cfg1 VDD |
| 2693 | 2171 | `0x1952` | `0x52` | 1 | `2` | cfg1 VDDIO |

### 4.4 SAM correction: groups 2900 and 2910

| Group | Item | abs | rel | w | Raw bytes | Numeric value | Vendor comment |
| ---: | ---: | ---: | ---: | ---: | --- | ---: | --- |
| 2900 | 2800 | `0x2000` | `0x00` | 4 | `00 00 0f 00` | `983040` | `basic ges` |
| 2910 | 2900 | `0x2100` | `0x00` | 4 | `00 00 0f 00` | `983040` | `Facing` |

This is the exact reason the prior `group id=2900 (ACCEL)` and `group
id=2910 (GYRO)` labels must be removed from future reports.

## 5. Kernel and userspace response-path comparison

The local kernel SNS_REG patch and the local userspace `sns-reg` source use the
same QMI field layout:

| Wire field | TLV | Width / representation | Kernel source | Userspace source |
| --- | ---: | --- | --- | --- |
| group request ID | `0x01` | one unsigned 16-bit value | `patches/pmos612-op3-sensor-smgr/0002-soc-qcom-Add-in-kernel-sensors-registry-implementati.patch:121-133` | `/tmp/op3-sns-reg-src.S9P88t/qmi/sns_reg.c:7-19` |
| result | `0x02` | one unsigned 16-bit value | same patch `:135-143` | same source `:21-29` |
| returned group ID | `0x03` | one unsigned 16-bit value | same patch `:144-151` | same source `:30-37` |
| data length | `0x04` | unsigned 16-bit QMI data length | same patch `:152-159` | same source `:38-45` |
| group bytes | `0x04` | variable byte array, max `0x100` | same patch `:160-167` | same source `:46-53` |

The kernel handler uses the static map, checks the selected block, copies the
binary bytes from `sns.reg`, and sends them as the response. This is documented
in the kernel patch at `:381-443`, including `rsp->data_len = group->size` and
`memcpy(... + group->offset, group->size)`. The userspace source has the same
response schema. Thus the static layouts do not reveal a kernel/userspace
endianness or TLV mismatch.

This was a source comparison only. No new runtime request was sent in this
audit, and no wire packet bytes were captured. Therefore byte-for-byte runtime
equivalence is **NOT_RUN**, even though the static response definitions agree.

## 6. Existing device-log timeline

The existing log shows the following order on the audit boot:

```text
2.907638  SLPI remoteproc becomes available after slpi.mbn boot
2.919179  group 2690 served: offset 0x1700, size 0x100, result 0, data_len 256
2.919788  group 2692 served: offset 0x1800, size 0x100, result 0, data_len 256
2.919965  group 2693 served: offset 0x1900, size 0x100, result 0, data_len 256
3.206745  qcom_smgr reports available sensor count=2
3.206785  item 0: id=0x14 type=MAG
3.206809  item 1: id=0x28 type=PROX_LIGHT
3.225414  group 2900 served: offset 0x2000, size 4, result 0, data_len 4
3.225477  group 2910 served: offset 0x2100, size 4, result 0, data_len 4
```

The `2692/2693` requests precede the decoded inventory by about 287 ms. This
is a timing clue worth preserving, not proof of a race: the log does not show
the SLPI internal probe state or the raw response packet. The inventory log is
added after `qmi_txn_wait()` and after response decoding; its `count/type/id`
lines are not a raw QMI capture. This distinction matters for deciding whether
the loss occurs in SLPI or in Linux parsing.

The same log ends with only:

```text
/sys/bus/iio/devices/iio:device0 qcom-smgr-mag
/sys/bus/iio/devices/iio:device1 qcom-smgr-prox-light
```

This proves the current Linux-visible inventory, not the physical absence of a
six-axis part.

## 7. Newly found omission: generator padding write

The local `generator.c` has this code at lines 46-50:

```c
while ((key = group->keys[key_index++]).len) {
        if (key.id != -1)
                key_map[key.id].id = entry->addr + size;
                key_map[key.id].len = key.len;
        size += key.len;
}
```

The braces are missing. Consequently, for every padding entry with `id=-1`,
the second assignment still writes `key_map[-1].len = key.len`. This is an
out-of-bounds write and makes the text-generator path non-reproducible in
principle. It is not evidence that the current kernel SNS_REG response is
wrong: the kernel serves the static map directly, and the listed binary values
decode consistently with the existing numeric text. However, any future task
that regenerates a userspace numeric registry must first isolate and fix this
generator defect in its own source task; do not silently treat the current
generator output as an authoritative semantic decode.

The validator only checks that the sum of mapped key widths equals each static
group size (`validator.c:15-49`). It cannot detect wrong semantic labels,
wrong bus interpretation, or a generator out-of-bounds write.

## 8. What is established, and what is not

Established by this audit:

- `2692` and `2693` are the map groups containing the vendor ACCEL/GYRO
  candidate records; both binary blocks contain two enabled candidates and
  LSM6DS3/BMI160 UUIDs.
- Their blocks are served successfully by the current in-kernel SNS_REG path.
- `2900`/`2910` are SAM groups and must not be used as ACCEL/GYRO evidence.
- The kernel and userspace QMI response schemas are statically equivalent.
- The existing Sensor Manager log contains decoded inventory only: MAG and
  PROX_LIGHT.
- The repeated `0x1001` and zero CS fields are present in both ACCEL and GYRO
  candidate records and are also visible in the SMGR block.

Not established:

- `0x1001` is not proven to mean a particular AP I2C controller, AP SPI
  controller, or physical SSI instance. The vendor comment says `I2C_BUS`,
  while the adjacent CS field is commented `CS for SPI`; this is a strong clue
  but not a complete Qualcomm DDF bus definition.
- The LSM6DS3/BMI160 names are candidate records, not proof of the handset's
  installed component or its electrical connection.
- Successful registry responses do not prove that SLPI probed or accepted the
  corresponding hardware.
- The current log does not distinguish SLPI probe failure from a Linux parser
  loss because no wire-level inventory response was captured.
- No authoritative Qualcomm `sns_reg_api_v02.h`, `sns_ddf_comm.h`, or equivalent
  bus-definition source was found in the preserved local inputs or the pinned
  project sources during this audit.

## 9. Safety boundary for the next model

Do not use this report to add an HLOS I2C/SPI node, change the `0x1001` value,
guess a GPIO or IRQ, switch to `lvs1`, or change the IIO mapping. Such changes
would combine the registry, physical-bus, and Linux-client hypotheses and could
reintroduce the known boot-restart failure. The one permitted next step is the
read-only wire-level inventory diagnostic stated in the handoff template. If
that diagnostic is not possible, the correct status is an evidence gap rather
than a guessed transport mapping.

## 10. Reproduction commands used for this audit

These commands are host-side and read-only. They do not build or contact the
phone:

```sh
sha256sum \
  /home/kai/op3-recovery-external-inputs/sensors/sns.reg \
  /tmp/op3-vendor-sensor-inspect.S4pPon/sensor_def_qcomdev.conf \
  /tmp/op3-sensor-registry.conf \
  /tmp/op3-sns-reg-src.S9P88t/map.c \
  /tmp/op3-sensor-smgr-regaudit-device.log

rg -n -C 2 '2690|2692|2693|2800|2900|2910' \
  /tmp/op3-sns-reg-src.S9P88t/map.c

rg -n -C 4 'available sensor|serving group id=(2690|2692|2693|2900|2910)' \
  /tmp/op3-sensor-smgr-regaudit-device.log

sed -n '144,220p' \
  /tmp/op3-vendor-sensor-inspect.S4pPon/sensor_def_qcomdev.conf
```

The audit branch contains only the report and the corrections to project
handoff wording. It does not modify the nested sensor kernel worktree.
