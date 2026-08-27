# OnePlus 3 Linux 7.2 / disable-EFI-stub early-boot A/B

```text
Task / GitHub Issue: Owner-authorized early-boot kernel-configuration A/B;
no GitHub Issue supplied
Role: Implementation agent
Baseline commit: 8d3ae59288f1e7d58d76558a6ee96d533bc5019f (Linux v7.2-3-gd73a640c6)
Working branch: agent/implementation/s6e3fa5-linux72-port
Changed files:
- kernel/configs/oneplus3-noefi.fragment
- docs/handoff/linux72-noefi-ab.md
- docs/test-matrix.md
Commit SHA: pending

Layer: Kernel configuration / early boot (single variable: EFI stub presence)

Hypothesis tested:
Linux 7.2 defaults CONFIG_EFI=y ("UEFI runtime support"), which selects
EFI_STUB and EFI_GENERIC_STUB and therefore builds the ARM64 Image as an EFI
application: it begins with the "MZ" PE header and carries
entry_offset = 0x4000 in the Image header at +0x38.  The OnePlus 3 MSM8996
has no UEFI firmware; its LK bootloader boots a plain ARM64 Image
(entry_offset = 0, first word is a kernel instruction, header flags show no
EFI).  When handed the EFI-stub Image, LK enters at the wrong address and the
kernel fails before any console/ramoops/USB output, producing the observed
immediate return to fastboot.

Only variable changed:
CONFIG_EFI is disabled (# CONFIG_EFI is not set), so the Image is rebuilt in
the plain, non-EFI ARM64 format.  This fragment is applied together with
oneplus3-vabits48.fragment so VA/PA (48) and every other option remain
identical to OP3-BOOT-015.  Kernel source, DTB, boot profile, initramfs and
cmdline are unchanged.

Evidence supporting the mechanism:
- Linux v7.2 arch/arm64/Kconfig: "config EFI" is `bool "UEFI runtime support"`
  with `default y`, and it `select EFI_STUB` and `select EFI_GENERIC_STUB`.
- Extracted vabits48 Image: first 4 bytes = `4d 5a` ("MZ"), Image header at
  +0x38 shows entry_offset = 0x4000, flags = 0x00004550.
- Known-good pmOS 6.3.1 v74/v100 Images: first word = `1f 20 03 d5` (a kernel
  instruction, no EFI stub), entry_offset = 0.  Their configs have
  "# CONFIG_EFI is not set".
- Image size correlation: every bootable kernel (v56/v74/v96/v100, 6.3.1) is
  a non-EFI Image of ~31 MiB; every failing kernel (7.2 and 6.19.5) is an
  EFI-stub Image of ~41-43 MiB.  The larger size is exactly the added EFI
  stub/code, confirming the format difference.
- OP3-BOOT-003 proved the packer, boot profile and header are not the cause:
  the same packer boots v100.  OP3-BOOT-015 (48-bit VA/PA) narrowed the
  remaining kernel payload to this EFI-stub format difference.

Build run by project owner: NOT_RUN (this agent does not compile)
Build result: NOT_RUN
Required config fragments, in this order:
1. kernel/configs/oneplus3-s6e3fa5.fragment
2. kernel/configs/oneplus3-vabits48.fragment
3. kernel/configs/oneplus3-noefi.fragment

Owner build command (not run by this agent):
```bash
project=/home/kai/src/oneplus3-mainline
kernel="$project/source/linux-7.2"
output="$project/out/linux-7.2-oneplus3-s6e3fa5-vabits48-noefi"

mkdir -p "$output" && \
make -C "$kernel" O="$output" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 defconfig && \
"$kernel/scripts/kconfig/merge_config.sh" -m -O "$output" "$output/.config" \
  "$project/kernel/configs/oneplus3-s6e3fa5.fragment" \
  "$project/kernel/configs/oneplus3-vabits48.fragment" \
  "$project/kernel/configs/oneplus3-noefi.fragment" && \
make -C "$kernel" O="$output" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 olddefconfig && \
grep -qx '# CONFIG_EFI is not set' "$output/.config" && \
grep -qx 'CONFIG_ARM64_VA_BITS=48' "$output/.config" && \
grep -qx 'CONFIG_ARM64_PA_BITS=48' "$output/.config" && \
grep -qx 'CONFIG_DRM=y' "$output/.config" && \
grep -qx 'CONFIG_DRM_MSM=y' "$output/.config" && \
grep -qx 'CONFIG_DRM_PANEL_SAMSUNG_S6E3FA5=y' "$output/.config" && \
make -C "$kernel" O="$output" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  -j"$(nproc)" Image.gz qcom/msm8996-oneplus3.dtb
```

Post-build verification (agent-inspectable, before packing):
```bash
gzip -dc "$output/arch/arm64/boot/Image.gz" | head -c 8 | xxd
# Expect the first word to be a kernel instruction, NOT "4d 5a" ("MZ").
```

Packing (unchanged, validated packer):
```bash
./scripts/pack-boot.sh \
  "$output/arch/arm64/boot/Image.gz" \
  "$output/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb" \
  artifacts/initrd-op3-minimal.cpio.gz \
  artifacts/boot-oneplus3-fa5-linux72-vabits48-noefi.img
```

Artifacts and SHA256: pending owner build.

Device test run by project owner: NOT_RUN
Device test procedure: `fastboot boot` only; no flash.
Device test PASS condition: the device leaves fastboot (any observable kernel
presence: black screen without fastboot return, serial line, ramoops record,
or USB enumeration).  Returning to fastboot is FAIL.
Note: a black screen alone does not establish display; it only isolates the
early-boot failure point.

Conclusion: INCONCLUSIVE pending owner build and test.

Uncertainties:
- A PASS would prove the EFI-stub format was the fastboot-return root cause;
  display, DRM/MSM probe, DSI attach, panel output and rootfs still require
  later device evidence.
- If this A/B FAILs too, the early failure is earlier than or independent of
  the Image format, and the next step is a physical MSM8996 UART trace; do
  not keep substituting config options blindly.

Recommended next experiment:
- Owner runs only the build command above, checks the Image first word is not
  "MZ", packs, then runs only `fastboot boot`.
- Do not change DTB, initramfs, boot profile, DRM/MSM, GPU, PM or panel as
  part of this A/B.
- After a PASS, promote disabling EFI (plus 48-bit VA/PA) into the default
  OnePlus 3 build path in a separate scoped task so future builds do not
  silently return to the failing EFI-stub default.
```

## Kernel-side reference (read-only evidence)

- `arch/arm64/Kconfig` (v7.2): `config EFI` = `bool "UEFI runtime support"`,
  `depends on OF && !CPU_BIG_ENDIAN`, `default y`, `select EFI_STUB`,
  `select EFI_GENERIC_STUB`.  Help text: "This is only useful on systems that
  have UEFI firmware."
- Image header at +0x38: `ARM64_IMAGE_MAGIC` ("ARMd"); the 32-bit field after
  the magic is the entry offset.  EFI-stub images carry a "MZ" PE header and
  a non-zero entry offset; plain images start with a kernel instruction and
  entry offset 0.
- The known-good pmOS 6.3.1 tree (`source/linux-pmos-msm8996-6.3.1`) sets
  `# CONFIG_EFI is not set`; its 6.19.5/7.2 counterparts keep EFI enabled and
  produce the larger EFI-stub images.
