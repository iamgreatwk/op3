# Linux 7.2 / MSM8996 no-HCR_ATA EL2-entry A/B

```text
Task / GitHub Issue: Owner-authorized no-Issue ARM64 entry-source A/B
Role: Implementation
Baseline commit: Linux v7.2 (`8d3ae59288f1e7d58d76558a6ee96d533bc5019f`)
Working branches:
  project metadata: agent/implementation/s6e3fa5-linux72-port
  kernel source: agent/implementation/linux72-no-hcr-ata-ab
Kernel source commit: 44a2bd4483d15116ad68ff786f74a2dc4fccec1f
Changed files:
  source/linux-7.2/arch/arm64/kernel/head.S
  docs/handoff/linux72-no-hcr-ata-ab.md
Source A/B commit: 44a2bd4483d15116ad68ff786f74a2dc4fccec1f

Layer: Kernel ARM64 EL2 entry
Hypothesis tested: The OnePlus 3 bootloader enters Linux at EL2 and Linux 7.2's
unconditional HCR_ATA bit causes the return to fastboot on MSM8996.
Only variable changed: init_el2_hcr argument removes HCR_ATA.

Build run by project owner: RUN
Build result: PASS (kernel built and boot image packed; artifact SHA256 not supplied)
Artifacts and SHA256: boot-oneplus3-linux72-v74strict-no-hcr-ata.img; SHA256 not supplied

Device test run by project owner: RUN (`fastboot boot`)
Device result: FAIL — returned to fastboot mode
Evidence links / log paths: owner report in this Codex task; no serial/USB log
```

## Exact source delta

At `arch/arm64/kernel/head.S`, Linux 7.2 changed the EL2 entry value from the
6.19.5 control's:

```text
init_el2_hcr HCR_HOST_NVHE_FLAGS
```

to:

```text
init_el2_hcr HCR_HOST_NVHE_FLAGS | HCR_ATA
```

The A/B source commit restores the former expression only. `HCR_ATA` controls
Armv8.5 MTE allocation-tag access and is not implemented by MSM8996's
Cortex-A53/A72 CPUs. The line executes only after `init_kernel_el()` identifies
an EL2 entry; it has no effect if the bootloader enters at EL1.

This test intentionally retains `CONFIG_ARM64_MTE=y`. The earlier no-MTE A/B
failed, so using `oneplus3-no-mte.fragment` here would introduce a second
variable. No DTS, panel, DSI, DRM/MSM, GPU, PM, IOMMU, UFS, USB, firmware,
initramfs, cmdline or boot-image-profile file changes are included.

## Fixed companion inputs

- Base config: `kernel/configs/pmos631/v72-v74strict-full.config`.
- v74 DTB:
  `out/pmos-msm8996-6.3.1-v74full/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb`,
  SHA256 `463b2c7203e28359ce4039c8e4aa9b0d211171879db8bbea07834d3cb8b2bde3`.
- Reference initrd: `artifacts/reference-initrd.img`, SHA256
  `c3358a1cadb747996ddaa492e636827f2d72974040e8fd40d81f8a213e676366`.
- Boot profile: `boot/oneplus3-fa5.env`, with the unchanged UUID-free cmdline
  `fbcon=nodefault console=tty0 pmos.debug-shell`.

## Owner build, package and device-test command

```bash
set -e

project=/home/kai/src/oneplus3-mainline
kernel="$project/source/linux-7.2"
output="$project/out/linux-7.2-v74strict-no-hcr-ata"
base_config="$project/kernel/configs/pmos631/v72-v74strict-full.config"
dtb="$project/out/pmos-msm8996-6.3.1-v74full/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb"
initrd="$project/artifacts/reference-initrd.img"
bootimg="$project/artifacts/boot-oneplus3-linux72-v74strict-no-hcr-ata.img"

git -C "$kernel" switch agent/implementation/linux72-no-hcr-ata-ab
test "$(git -C "$kernel" rev-parse HEAD)" = 44a2bd4483d15116ad68ff786f74a2dc4fccec1f
test -z "$(git -C "$kernel" status --porcelain)"
test -f "$base_config"
test -f "$dtb"
test -f "$initrd"

mkdir -p "$output"
cp "$base_config" "$output/.config"
make -C "$kernel" O="$output" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 olddefconfig
grep -qx 'CONFIG_ARM64_MTE=y' "$output/.config"
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

fastboot boot "$bootimg"
```

The project owner alone runs the final `fastboot boot` command. Return to
fastboot is **FAIL**. Any non-fastboot state is a provisional early-entry
**PASS** only; it does not establish panel or display success. Record the
artifact SHA256 and observed device result before selecting another variable.

## Result and next decision

**REJECTED.** The owner reports that the no-HCR_ATA image still returns to
fastboot mode. Therefore the Linux 7.2 HCR_ATA addition is not the primary
cause under the fixed strict configuration and companion inputs. This result
does not prove the entry EL: the line can be irrelevant on EL1 entry, or can
execute at EL2 without being the failing operation.

Keep source commit `44a2bd4483` as failure evidence and do not merge it into
the formal Linux 7.2 branch. Both direct 7.2-versus-6.19.5 pre-MMU candidates
identified by the prior audit (`TCMA1` and `HCR_ATA`) are now rejected. The
next task should obtain an early log or perform a newly scoped read-only audit;
do not combine another ARM64 source change with this test.
