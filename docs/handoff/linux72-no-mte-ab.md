# Linux 7.2 / MSM8996 no-MTE early-entry A/B

```text
Task / GitHub Issue: Owner-authorized no-Issue early-entry configuration A/B
Role: Implementation
Baseline commit: Linux v7.2 (`8d3ae59288f1e7d58d76558a6ee96d533bc5019f`)
Working branch: agent/implementation/s6e3fa5-linux72-port
Changed files:
  kernel/configs/oneplus3-no-mte.fragment
  docs/handoff/linux72-no-mte-ab.md
Commit SHA: pending

Layer: Kernel configuration / pre-MMU ARM64 entry
Hypothesis tested: Linux 7.2 returns to fastboot because its default
CONFIG_ARM64_MTE=y adds TCR_EL1_TCMA1 to the value written by __cpu_setup()
before the MMU is enabled on the MSM8996 Cortex-A53/A72.
Only variable changed: CONFIG_ARM64_MTE changes from y to not set.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: NOT_RUN

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: pending owner result
```

## Fixed inputs

- Kernel source branch and required clean commit:
  `source/linux-7.2`, `agent/implementation/s6e3fa5-linux72-port`,
  `825ddb98252d63651ead70eed32dfe5537c72ac6`.
  This includes only the accepted FA5 port and the reverted DTS A/B; it does
  **not** include the separately failed DSI experiment.
- Base config: `kernel/configs/pmos631/v72-v74strict-full.config`.
- Sole delta: `kernel/configs/oneplus3-no-mte.fragment`.
- Fixed appended DTB:
  `out/pmos-msm8996-6.3.1-v74full/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb`,
  SHA256 `463b2c7203e28359ce4039c8e4aa9b0d211171879db8bbea07834d3cb8b2bde3`.
- Fixed initrd: `artifacts/reference-initrd.img`, SHA256
  `c3358a1cadb747996ddaa492e636827f2d72974040e8fd40d81f8a213e676366`.
- Fixed boot profile: `boot/oneplus3-fa5.env`, Android header v0, page size
  4096, raw appended DTB, and UUID-free default cmdline
  `fbcon=nodefault console=tty0 pmos.debug-shell`.

## Rationale

The read-only comparison in
`docs/handoff/linux72-v6195-early-entry-source-audit.md` found that the
boot-critical MSM8996 driver configuration is equal between the booting pmOS
6.19.5 control and failing Linux 7.2 build. In `arch/arm64/mm/proc.S`, however,
the two builds differ before the MMU starts:

```text
pmOS 6.19.5: TCR_EL1_TBI1 | TCR_EL1_TBID1
Linux 7.2:   TCR_EL1_TCMA1 | TCR_EL1_TBI1 | TCR_EL1_TBID1
```

`CONFIG_ARM64_MTE=n` removes only the latter `TCMA1` contribution. It does not
alter HCR_ATA, which remains a separate EL2-only hypothesis. It does not alter
the panel port, DTS, DSI, DRM/MSM, GPU, runtime PM, UFS, USB, firmware, DTB,
ramdisk or boot-image format.

## Owner build and package command

Do not run this command against the current DSI experiment branch. The owner
must first use the exact source branch and commit shown above. This command
does not use or copy a pmOS root UUID.

```bash
set -e

project=/home/kai/src/oneplus3-mainline
kernel="$project/source/linux-7.2"
output="$project/out/linux-7.2-v74strict-no-mte"
base_config="$project/kernel/configs/pmos631/v72-v74strict-full.config"
fragment="$project/kernel/configs/oneplus3-no-mte.fragment"
dtb="$project/out/pmos-msm8996-6.3.1-v74full/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb"
initrd="$project/artifacts/reference-initrd.img"
bootimg="$project/artifacts/boot-oneplus3-linux72-v74strict-no-mte.img"

git -C "$kernel" switch agent/implementation/s6e3fa5-linux72-port
test "$(git -C "$kernel" rev-parse HEAD)" = 825ddb98252d63651ead70eed32dfe5537c72ac6
test -z "$(git -C "$kernel" status --porcelain)"
test -f "$base_config"
test -f "$fragment"
test -f "$dtb"
test -f "$initrd"

mkdir -p "$output"
cp "$base_config" "$output/.config"
"$kernel/scripts/kconfig/merge_config.sh" -m -O "$output" \
  "$output/.config" "$fragment"
make -C "$kernel" O="$output" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 olddefconfig
grep -qx '# CONFIG_ARM64_MTE is not set' "$output/.config"
grep -qx 'CONFIG_SCSI_UFS_QCOM=y' "$output/.config"
grep -qx 'CONFIG_DRM_MSM=y' "$output/.config"
grep -qx 'CONFIG_DRM_PANEL_SAMSUNG_S6E3FA5=y' "$output/.config"

make -C "$kernel" O="$output" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  -j"$(nproc)" Image.gz

"$project/scripts/pack-boot.sh" \
  "$output/arch/arm64/boot/Image.gz" "$dtb" "$initrd" "$bootimg"
abootimg -i "$bootimg"
sha256sum "$bootimg"
```

## Owner device criterion

The owner alone performs `fastboot boot`. A return to fastboot is **FAIL**.
Any non-fastboot early state (including black screen) is a provisional **PASS
for this bootloader-entry A/B only**, not a display success; record the exact
observation and then pursue early logs. The test must not add a UUID, change
the cmdline, replace the DTB/initrd, or combine any other configuration or
source change.

## Status and next decision

**READY_FOR_OWNER_BUILD.** No build, packaging, fastboot operation or device
test has been performed by this agent. If the result is FAIL, preserve this
same image evidence and consider the separately authorized HCR_ATA source A/B
only after reviewing the device entry EL. If it is a provisional PASS, first
collect early console/USB evidence before changing display code.
