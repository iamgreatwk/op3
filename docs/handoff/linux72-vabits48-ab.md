# OnePlus 3 Linux 7.2 / 48-bit VA-PA early-boot A/B

```text
Task / GitHub Issue: Owner-authorized early-boot kernel-configuration A/B;
no GitHub Issue supplied
Role: Implementation agent
Baseline commit: 8d3ae59288f1e7d58d76558a6ee96d533bc5019f (Linux v7.2-3-gd73a640c6)
Working branch: agent/implementation/s6e3fa5-linux72-port
Changed files:
- kernel/configs/oneplus3-vabits48.fragment
- docs/handoff/linux72-vabits48-ab.md
- docs/test-matrix.md
Commit SHA: pending

Layer: Kernel configuration / early boot (single variable: VA/PA size)

Hypothesis tested:
The Linux 7.2 (and v6.19.5) immediate return to fastboot is caused by the
upstream-default 52-bit virtual/physical address size
(CONFIG_ARM64_VA_BITS_52 / ARM64_PA_BITS_52).  The OnePlus 3 MSM8996
Cortex-A53/A72 cores implement only ARMv8.0, which does not provide the
ARMv8.2 LVA/LPA2 features required for 52-bit addresses.  During the primary
CPU boot path the alternatives machinery is not yet applied, so __cpu_setup()
in arch/arm64/mm/proc.S executes its unpatched sequence and unconditionally
writes TCR_EL1.T1SZ = 12 (52-bit) and 52-bit IPS.  On ARMv8.0 that TCR value
is architecturally reserved: the MMU faults immediately after __enable_mmu(),
the LK bootloader sees a reset and returns the device to fastboot before any
kernel-visible output (ramoops, serial, USB ACM) can exist.

Only variable changed:
CONFIG_ARM64_VA_BITS changes from 52 to 48 and CONFIG_ARM64_PA_BITS follows
to 48.  Kernel source, DTB, boot profile, initramfs, cmdline, and every other
option remain identical to the last failing pstore build.

Evidence supporting the mechanism:
- Linux v7.2 upstream arch/arm64/Kconfig:
  choice "Virtual address space size" -> default ARM64_VA_BITS_52.
- Linux v6.19.5 upstream Kconfig also defaults to ARM64_VA_BITS_52.
- pmOS 6.3.1 defconfig explicitly sets CONFIG_ARM64_VA_BITS_48=y, and its
  Kconfig makes 52-bit depend on 64K pages, so 4K-pages builds cannot select
  52-bit.  The known-good v100 kernel is therefore always 48-bit.
- All failing boots (OP3-BOOT-001/002/004/007/008/009/011/012/013) share
  VA_BITS=52 / PA_BITS=52 (7.2 and v6.19.5 builds).  The known-good 6.3.1
  and linux-fa5 configs share VA_BITS=48 / PA_BITS=48.
- head.S guards only the secondary CPUs with __cpu_secondary_check52bitva;
  there is no primary-CPU 52-bit guard.  __cpu_setup() runs before
  apply_boot_alternatives(), so the alternative_if ARM64_HAS_VA52 branch
  executes its original (52-bit) instruction sequence unconditionally.
- Empty /sys/fs/pstore after the failed boot (OP3-BOOT-009) is consistent
  with the reset happening before ramoops can initialize.

Build run by project owner: NOT_RUN (this agent does not compile)
Build result: NOT_RUN
Required config fragments, in this order:
1. kernel/configs/oneplus3-s6e3fa5.fragment
2. kernel/configs/oneplus3-vabits48.fragment
Optional 3. kernel/configs/oneplus3-early-pstore.fragment (for a later
ramoops check of the same image)

Owner build command (not run by this agent):
```bash
project=/home/kai/src/oneplus3-mainline
kernel="$project/source/linux-7.2"
output="$project/out/linux-7.2-oneplus3-s6e3fa5-vabits48"

mkdir -p "$output" && \
make -C "$kernel" O="$output" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 defconfig && \
"$kernel/scripts/kconfig/merge_config.sh" -m -O "$output" "$output/.config" \
  "$project/kernel/configs/oneplus3-s6e3fa5.fragment" \
  "$project/kernel/configs/oneplus3-vabits48.fragment" && \
make -C "$kernel" O="$output" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 olddefconfig && \
grep -qx 'CONFIG_ARM64_VA_BITS=48' "$output/.config" && \
grep -qx 'CONFIG_ARM64_PA_BITS=48' "$output/.config" && \
grep -qx 'CONFIG_ARM64_VA_BITS_48=y' "$output/.config" && \
grep -qx '# CONFIG_ARM64_VA_BITS_52 is not set' "$output/.config" && \
grep -qx 'CONFIG_DRM=y' "$output/.config" && \
grep -qx 'CONFIG_DRM_MSM=y' "$output/.config" && \
grep -qx 'CONFIG_DRM_PANEL_SAMSUNG_S6E3FA5=y' "$output/.config" && \
make -C "$kernel" O="$output" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  -j"$(nproc)" Image.gz qcom/msm8996-oneplus3.dtb
```

Artifacts and SHA256: pending owner build.

Device test run by project owner: NOT_RUN
Device test procedure: `fastboot boot` only; no flash.
Device test PASS condition: the device leaves fastboot (any observable
kernel presence, including a black screen with no fastboot return, a serial
line, a ramoops record, or a USB enumeration).  Returning to fastboot is FAIL.
Note: a black screen alone does not establish display; it only isolates the
early-boot failure point.

Conclusion: INCONCLUSIVE pending owner build and test.

Uncertainties:
- A PASS would prove the fastboot-return root cause; display, DRM/MSM probe,
  DSI attach, panel output, and rootfs still require later device evidence.
- If this A/B FAILs, the early failure is before or independent of VA sizing
  and the next step must be a physical MSM8996 UART trace; no further config
  substitution should be made blindly.

Recommended next experiment:
- Owner runs only the build command above, then only `fastboot boot`.
- Do not change DTB, initramfs, boot profile, DRM/MSM, GPU, PM, or panel
  as part of this A/B.
- After a PASS, promote 48-bit VA/PA to the default OnePlus 3 build path in
  a separate scoped task (the vabits48 fragment or a defconfig delta) so that
  future builds do not silently return to the failing 52-bit default.
```

## Kernel-side reference (read-only evidence)

- `arch/arm64/Kconfig` (v7.2): the "Virtual address space size" choice has
  `default ARM64_VA_BITS_52`; `ARM64_PA_BITS_52` depends on
  `ARM64_64K_PAGES || ARM64_VA_BITS_52`.
- `arch/arm64/mm/proc.S` `__cpu_setup()`: the initial TCR uses
  `TCR_T1SZ(VA_BITS_MIN)` (16 for 48-bit), then the
  `#ifdef CONFIG_ARM64_VA_BITS_52` block executes
  `tcr_set_t1sz tcr, #64 - VA_BITS` (=12) before
  `apply_boot_alternatives()` has run, i.e. the 52-bit T1SZ is written
  unconditionally on the primary CPU.  `tcr_compute_pa_size` reads the
  hardware `ID_AA64MMFR0_EL1.PARange` and caps IPS, but the T1SZ fault
  occurs at/after MMU enable regardless.
- `arch/arm64/kernel/head.S`: only the secondary path checks 52-bit VA
  support (`__cpu_secondary_check52bitva`); there is no primary equivalent.
