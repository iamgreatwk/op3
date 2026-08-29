# Linux 7.2 / pmOS 6.19.5 MSM8996 early-entry source audit

```text
Task / GitHub Issue: Owner-authorized no-Issue source audit
Role: Research / review
Baseline commit: Linux v7.2 (`8d3ae59288f1e7d58d76558a6ee96d533bc5019f`)
Working branch: agent/implementation/s6e3fa5-linux72-port
Changed files: docs/handoff/linux72-v6195-early-entry-source-audit.md
Commit SHA: pending

Layer: Kernel early entry / source audit only
Hypothesis tested: With the v74 DTB, initrd and strict configuration fixed,
an ARM64 early-entry difference between the booting pmOS 6.19.5 control and
Linux 7.2 explains the 7.2 return to fastboot.
Only variable changed: none; this is a read-only comparison.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: none created

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: owner results OP3-BOOT-037 (6.19.5 PASS) and
OP3-BOOT-038/039 (7.2 FAIL), recorded in docs/handoff/latest.md
```

## Compared inputs

- Linux 7.2 source baseline with the accepted FA5 panel/DTS commits through
  `d73a640c6af7ad461bce8f54967c0abdf44d1204`. The local tree currently also
  contains the separately failed DSI experiment `9edd80e552`; that experiment
  modifies only `drivers/gpu/drm/msm/dsi/`, not any file audited below.
- Booting reference source: pmOS MSM8996 `v6.19.5-msm8996`, commit
  `1aed438cb5f49d7a61593f764d4a82f83b14114f`. Its working-tree-only change is
  `arch/arm64/boot/dts/qcom/msm8996-oneplus3.dts`, which is outside this source
  comparison and was not read as evidence.
- Existing owner-built strict-config outputs:
  `out/pmos-msm8996-6.19.5-v74strict` (booted with fixed v74 DTB) and
  `out/linux-7.2-v74strict` (failed with the same v74 DTB).

## Fixed configuration facts

Both effective configs have the boot-critical settings below, so this audit
does not support a missing MSM8996 driver/module explanation:

```text
CONFIG_ARM64_4K_PAGES=y
CONFIG_ARM64_VA_BITS_48=y
CONFIG_ARM64_PA_BITS_48=y
# CONFIG_EFI is not set
CONFIG_RANDOMIZE_BASE=y
CONFIG_RELOCATABLE=y
CONFIG_SCSI_UFS_QCOM=y
CONFIG_PHY_QCOM_QMP_UFS=y
CONFIG_PINCTRL_MSM8996=y
CONFIG_COMMON_CLK_QCOM=y
CONFIG_QCOM_SMEM=y
CONFIG_QCOM_SMEM_STATE=y
CONFIG_QCOM_SMD_RPM=y
CONFIG_QCOM_SCM=y
CONFIG_QCOM_WDT=y
CONFIG_SERIAL_MSM=y
CONFIG_SERIAL_MSM_CONSOLE=y
CONFIG_USB_CONFIGFS=y
CONFIG_USB_CONFIGFS_ACM=y
CONFIG_PSTORE=y
CONFIG_PSTORE_RAM=y
CONFIG_DRM=y
CONFIG_DRM_MSM=y
CONFIG_DRM_PANEL_SAMSUNG_S6E3FA5=y
```

`CONFIG_ARM64_LSUI` is already disabled in the 7.2 output, and OP3-BOOT-039
still failed. The current evidence therefore excludes neither an early CPU
register setup problem nor an unobservable reset before the console starts.

## Direct early-entry comparison

The early-boot-critical Qualcomm GCC source is byte-identical in the two
trees: `drivers/clk/qcom/gcc-msm8996.c` has SHA256
`b45e05b1c612d48020f51640e73b98b98961e815e6b05c6b2edd64de2ab772b1` in both.
The MSM8996 pinctrl change only removes redundant interrupt-target field
initialisers and moves `MODULE_DEVICE_TABLE`; it is not before-MMU code.

The direct ARM64 entry differences are limited to two instructions/values:

1. `arch/arm64/mm/proc.S` executes `__cpu_setup` before enabling the MMU. In
   the common strict configuration, `CONFIG_ARM64_MTE=y`. Linux 7.2 therefore
   builds `TCR_MTE_FLAGS` as `TCR_EL1_TCMA1 | TCR_EL1_TBI1 | TCR_EL1_TBID1`,
   while pmOS 6.19.5 builds only `TCR_EL1_TBI1 | TCR_EL1_TBID1`. This means
   7.2 unconditionally includes the extra `TCMA1` TCR bit in the value written
   by `__cpu_setup`, before runtime CPU-feature alternatives can help.

2. `arch/arm64/kernel/head.S` initialises EL2 with
   `HCR_HOST_NVHE_FLAGS | HCR_ATA` in Linux 7.2; pmOS 6.19.5 uses only
   `HCR_HOST_NVHE_FLAGS`. This is conditional on entering the kernel at EL2;
   `init_kernel_el()` branches directly to EL1 setup otherwise. The available
   evidence does not establish the OnePlus 3 bootloader entry EL, so this is a
   lower-confidence candidate until early logging is available.

The legacy `.idmap.text` theory is excluded for this pair: both 6.19.5 and
7.2 use section flag `"a"` in `head.S` and `proc.S`. Reintroducing `"awx"`
would not test a difference between these two controls and must not be done.

## Later probe-path differences, not early-entry fixes

`drivers/soc/qcom/smem.c` changes its partition store from a fixed 25-entry
array to an xarray and allocates each discovered partition with `devm_kzalloc`.
`smem_state.c` changes to `kzalloc_obj()`. These execute in platform-driver
probe after the ARM64 entry path and are possible later runtime suspects only;
this audit supplies no basis to alter or backport either.

## Conclusion

**INCONCLUSIVE, but narrowed.** No pmOS-specific MSM8996 clock, pinctrl, UFS,
SMEM enablement, DRM, GPU, PM, or DTS patch is justified. The most focused
remaining source-level A/B is a configuration-only test of
`CONFIG_ARM64_MTE=n`: it removes the 7.2-only pre-MMU `TCMA1` contribution
while leaving the DTB, initrd, all MSM8996 drivers and the HCR_ATA candidate
unchanged. It must be a separate task and owner-run build; this audit creates
no fragment and changes no kernel source.

If that test still returns to fastboot, the next single-variable experiment is
an EL2-only A/B of the HCR_ATA addition, but only after explicit authorization
because it changes ARM64 entry source. The preferred diagnostic path remains
an early boot log, which would determine the entry EL and failure stage before
any source patch is attempted.
