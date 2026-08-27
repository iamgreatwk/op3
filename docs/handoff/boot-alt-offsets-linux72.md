# OnePlus 3 Linux 7.2 alternate-offset boot A/B

Task / GitHub Issue: Owner-authorized boot/early-boot A/B; no GitHub Issue supplied
Role: Implementation agent
Baseline commit: `f005db3fb5d5103c95a3b45f811d2e5431c55334`
Working branch: `agent/implementation/s6e3fa5-linux72-port`
Changed files:

- `boot/oneplus3-fa5-alt-offsets.env`
- `docs/handoff/boot-alt-offsets-linux72.md`

Commit SHA: pending

Layer: Boot / early boot
Hypothesis tested: The Linux 7.2 minimal-initramfs image returns to fastboot because it requires the proposed alternate OnePlus 3 Android boot offsets.
Only variable changed: the boot profile's `BOOT_RAMDISK_OFFSET` and `BOOT_TAGS_OFFSET`, treated as one atomic address-layout profile. Kernel, appended DTB, minimal initramfs, base, page size, kernel offset, second offset, and cmdline remain identical to OP3-BOOT-002.

## Inputs

- Linux 7.2 `Image.gz`: `out/linux-7.2-oneplus3-s6e3fa5/arch/arm64/boot/Image.gz`; SHA256 `4a636bc445a000f8f57208220472109752b0f8e3799e0c7778151d1700e48b56`
- Linux 7.2 OnePlus 3 DTB: `out/linux-7.2-oneplus3-s6e3fa5/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb`; SHA256 `87963b9340d437abb6bfe387e05327bcb24766e81d515df9513ce0da1a4eea45`
- Minimal UUID-free initramfs: `artifacts/initrd-op3-minimal.cpio.gz`; SHA256 `a107e55323dba324b28fa2185d1e92dcdb1ed666c94b49effafaa8feaa47763b`
- Cmdline: `fbcon=nodefault console=tty0 pmos.debug-shell`

## Address-layout delta

The validated default profile uses ramdisk address `0x81000000` and tags address `0x80000100`. This test only substitutes the owner-requested alternative offsets, producing ramdisk address `0x82200000` and tags address `0x82000000` from base `0x80000000`.

Build run by project owner: NOT_RUN (no compilation is part of this task)
Build result: NOT_RUN
Artifacts and SHA256:

- `artifacts/boot-oneplus3-fa5-linux72-minimal-init-alt-offsets.img`
- SHA256 `6507e222fd4d0d2024c7f34787eae0e1c8ea786893e53cbd0d01040bbb2633c4`
- Verified Android boot header v0, 4096-byte pages, kernel address
  `0x80008000`, ramdisk address `0x82200000`, tags address `0x82000000`,
  and the unchanged UUID-free cmdline.

Device test run by project owner: `fastboot boot` of the alternative-offset artifact
Device result: FAIL — owner reported that the device again remained/returned in fastboot mode.
Evidence links / log paths: owner report in this task; no fastboot terminal capture was supplied.

Conclusion: REJECTED — changing only to the alternative address-layout profile did not permit the Linux 7.2 minimal-initramfs image to boot.
Uncertainties: This profile contradicts the known-good v100 physical header, which uses `0x81000000` and `0x80000100`; this is therefore a low-probability, explicit A/B rather than a proposed default.
Recommended next experiment: do not make further boot-profile address changes. Stay at the Boot / early-boot layer and design one observable Linux 7.2 kernel-entry experiment with a single non-address variable.
