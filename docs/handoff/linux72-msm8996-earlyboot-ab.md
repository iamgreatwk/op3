# OnePlus 3 Linux 7.2 / MSM8996 early-boot built-in drivers A/B

```text
Task / GitHub Issue: Owner-authorized early-boot kernel-configuration A/B;
no GitHub Issue supplied
Role: Implementation agent
Baseline commit: 8d3ae59288f1e7d58d76558a6ee96d533bc5019f (Linux v7.2-3-gd73a640c6)
Working branch: agent/implementation/s6e3fa5-linux72-port
Changed files:
- kernel/configs/oneplus3-msm8996-earlyboot.fragment
- docs/handoff/linux72-msm8996-earlyboot-ab.md
- docs/test-matrix.md
Commit SHA: pending

Layer: Kernel configuration / early boot (single layer: MSM8996 platform
driver module-vs-builtin for early-boot-critical probes)

Origin of the hypothesis:
The project owner pointed at the external handoff
`agent-os/docs/交接给Codex_20260822.md` (2026-08-22), which identifies
`_mainline_test/v74_actual_config.txt` / `Image-v74` as the single trusted,
IKCONFIG-extracted, bootable 6.3.1 configuration baseline ("内核配置唯一基准").
Extracting that config and diffing it against the Linux 7.2 build showed that
nearly every MSM8996 platform driver is =y in v74 but =m in 7.2.

Hypothesis tested:
The Linux 7.2 return-to-fastboot is caused by MSM8996 early-boot-critical
platform drivers being modules (=m) that are never loaded, because the
diagnostic initramfs carries only a static /init and NO kernel modules.  The
bootable v74 image carries those drivers built-in (=y); the full v100
initramfs also carries the matching modules and firmware.  Leaving the
display/mmcc clocks, watchdog, and UFS/USB PHYs as modules means their probe
never runs and early platform bring-up stalls before any output.

Only variable changed (relative to OP3-BOOT-016):
the early-boot-relevant MSM8996 drivers below change from module to built-in.
Everything else (48-bit VA/PA, EFI disabled, DRM/MSM/FA5 built-in, DTB, boot
profile, minimal initramfs, cmdline) is identical to OP3-BOOT-016.

Drivers forced built-in in this fragment:
- Clocks/display:  CONFIG_MSM_MMCC_8996=y  (DTS mdss/mmss clock consumer)
- Watchdog/reset:  CONFIG_QCOM_WDT=y, CONFIG_RESET_QCOM_PDC=y
- SPMI/misc:       CONFIG_QCOM_SPMI_TEMP_ALARM=y, CONFIG_QCOM_COINCELL=y
- PHYs:            CONFIG_PHY_QCOM_QMP_UFS=y, CONFIG_PHY_QCOM_QMP_USB=y,
                   CONFIG_PHY_QCOM_QUSB2=y
- RTC/backlight:   CONFIG_RTC_DRV_PM8XXX=y, CONFIG_BACKLIGHT_QCOM_WLED=y

Deliberately NOT forced built-in:
- ADSP/SLPI remoteproc (QCOM_RPROC_COMMON, QCOM_Q6V5_*, QCOM_PIL_INFO):
  their probe depends on firmware (adsp.mbn, slpi.mbn, a530_zap.mbn) that the
  minimal initramfs does not carry; forcing them built-in risks a probe hang.
  v74 boots with those as modules + firmware in its full initramfs.
- ATH10K is kept =m per the external handoff "铁律 #2" (built-in ATH10K hangs
  early probe).  Already =m in the 7.2 build.

Kconfig verification (run by agent, config-only, no compile):
- merge_config.sh + olddefconfig accepted all fragment options; final .config
  resolves every target above to =y.
- CONFIG_QCOM_RPMCC does not exist in Linux 7.2; the MSM8996 RPM clock
  controller (qcom,rpmcc-msm8996) is served by CONFIG_QCOM_CLK_SMD_RPM, which
  is already =y, so no RPMCC entry is added.

Build run by project owner: NOT_RUN (this agent does not compile)
Build result: NOT_RUN
Required config fragments, in this order:
1. kernel/configs/oneplus3-s6e3fa5.fragment
2. kernel/configs/oneplus3-vabits48.fragment
3. kernel/configs/oneplus3-noefi.fragment
4. kernel/configs/oneplus3-msm8996-earlyboot.fragment

Owner build command (not run by this agent):
```bash
project=/home/kai/src/oneplus3-mainline
kernel="$project/source/linux-7.2"
output="$project/out/linux-7.2-oneplus3-s6e3fa5-vabits48-noefi-earlyboot"

mkdir -p "$output" && \
make -C "$kernel" O="$output" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 defconfig && \
"$kernel/scripts/kconfig/merge_config.sh" -m -O "$output" "$output/.config" \
  "$project/kernel/configs/oneplus3-s6e3fa5.fragment" \
  "$project/kernel/configs/oneplus3-vabits48.fragment" \
  "$project/kernel/configs/oneplus3-noefi.fragment" \
  "$project/kernel/configs/oneplus3-msm8996-earlyboot.fragment" && \
make -C "$kernel" O="$output" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 olddefconfig && \
grep -qx 'CONFIG_MSM_MMCC_8996=y' "$output/.config" && \
grep -qx 'CONFIG_QCOM_WDT=y' "$output/.config" && \
grep -qx 'CONFIG_PHY_QCOM_QMP_UFS=y' "$output/.config" && \
grep -qx '# CONFIG_EFI is not set' "$output/.config" && \
grep -qx 'CONFIG_ARM64_VA_BITS=48' "$output/.config" && \
grep -qx 'CONFIG_DRM_MSM=y' "$output/.config" && \
grep -qx 'CONFIG_DRM_PANEL_SAMSUNG_S6E3FA5=y' "$output/.config" && \
make -C "$kernel" O="$output" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  -j"$(nproc)" Image.gz qcom/msm8996-oneplus3.dtb
```

Post-build check (before packing):
```bash
gzip -dc "$output/arch/arm64/boot/Image.gz" | head -c 8 | xxd   # must NOT be "4d 5a"
```

Packing:
```bash
./scripts/pack-boot.sh \
  "$output/arch/arm64/boot/Image.gz" \
  "$output/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb" \
  artifacts/initrd-op3-minimal.cpio.gz \
  artifacts/boot-oneplus3-fa5-linux72-vabits48-noefi-earlyboot.img
```

Artifacts and SHA256: pending owner build.

Device test run by project owner: NOT_RUN
Device test procedure: `fastboot boot` only; no flash.
Device test PASS condition: the device leaves fastboot (any observable kernel
presence: black screen without fastboot return, serial line, ramoops record,
or USB enumeration).  Returning to fastboot is FAIL.

Conclusion: INCONCLUSIVE pending owner build and test.

Uncertainties:
- If this still returns to fastboot, the remaining untested differentiator is
  the 7.2 kernel/configuration/startup chain versus the 6.3.1 chain itself,
  and the correct next step is a physical MSM8996 UART trace (and/or reusing
  the bootable v74 Image + full initramfs as a positive control), not further
  blind config substitution.
- A PASS proves the module-vs-builtin layout was the cause; display, DRM/MSM
  probe, DSI attach, panel output and rootfs still require later evidence.

Recommended next experiment:
- Owner runs only the build command above, then only `fastboot boot`.
- If it boots, promote this fragment set into the default OnePlus 3 build path
  in a separate scoped task.
- If it still fails, stop config A/Bs and obtain a physical MSM8996 UART trace.
```

## Reference (external handoff, read-only)

- Source: `/home/kai/下载/WorkBuddy-20260826/WorkBuddy/2026-08-04-10-03-12/agent-os/docs/交接给Codex_20260822.md`
- Trusted 6.3.1 config baseline: `_mainline_test/v74_actual_config.txt`
  (IKCONFIG-extracted from Image-v74); extracted to `/tmp/Image-v74.config`.
- 铁律 #1: `make olddefconfig` without `ARCH=arm64` clears ARCH_QCOM and sets
  VA_BITS=39 -> return to fastboot (v95 incident).  Our builds always pass
  ARCH=arm64.
- 铁律 #2: ATH10K must be =m (built-in hangs early probe).  Kept =m.
- 铁律 #3: pack with three-segment page alignment, ramdisk tail zero-padded;
  our packer matches this and OP3-BOOT-003 proved it boots v100.
