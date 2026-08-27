# OnePlus 3 Linux 7.2 / v100-DTB cross A/B

```text
Task / GitHub Issue: Owner-authorized early-boot kernel-versus-DTB A/B; no GitHub Issue supplied
Role: Implementation agent
Baseline commit: d73a640c6af7ad461bce8f54967c0abdf44d1204 (Linux v7.2-3-gd73a640c6)
Working branch: agent/implementation/s6e3fa5-linux72-port
Changed files: docs/handoff/linux72-v100-dtb-ab.md; docs/test-matrix.md
Artifact-definition commit: 2c395613e0085a37660aac0b48e2a93e8520d16d

Layer: Boot / early boot kernel-versus-DTB isolation
Hypothesis tested: The Linux 7.2 DTB, rather than the Linux 7.2 Image.gz,
causes the pre-userspace return to fastboot.
Only variable changed: appended DTB changes from the Linux 7.2 OnePlus 3 DTB
to the known-good v100 DTB. Linux 7.2 Image.gz, v56 debug initramfs, boot
header profile, and UUID-free command line remain fixed.

Build run by project owner: NOT_RUN (this task does not compile)
Build result: NOT_RUN
Artifacts and SHA256:
- Linux 7.2 Image.gz: `4a636bc445a000f8f57208220472109752b0f8e3799e0c7778151d1700e48b56`
- v100 OnePlus 3 DTB: `21667a04bcbb8be4e9935968abd869cd549d88606b745779cba868ebef422926`
- fixed v56 debug initramfs: `69d6188bf9eafd46ece944ea88a2f2c06e94ee6f2edaee6e4da0ca91f4dfb843`
- output: `artifacts/boot-oneplus3-fa5-linux72-v100-dtb-v56-initramfs.img`
  SHA256 `dc64ca121b46e31c27567fe1e2aa89dfbd19d39bb44c6beefa63de3e9ec56372`
- verified header: Android boot v0; page size 4096; kernel `0x80008000`;
  ramdisk `0x81000000`; tags `0x80000100`
- actual cmdline: `fbcon=nodefault console=tty0 pmos.debug-shell`

Device test run by project owner: `fastboot boot` only; no flash
Device result: FAIL. The device remained/returned to fastboot mode; no
debug-shell display was observed.
Evidence links / log paths: owner report, 2026-08-27.

Conclusion: REJECTED. The known-good v100 DTB does not change the Linux 7.2
boot result when paired with the same Linux 7.2 Image.gz and v56 initramfs.
Uncertainties: The v100 DTB is legacy evidence only. It is intentionally used
as a transient binary diagnostic input and must not be copied into Linux 7.2 or
treated as an acceptable final DTS. USB remains outside this experiment's
acceptance condition.
Recommended next experiment: diagnose the Linux 7.2 Image.gz/configuration
layer using a separate, explicitly scoped early-boot observation mechanism.
Do not repeat DTB or initramfs substitutions, flash, or modify DRM/MSM, GPU,
PM, or legacy DTS wholesale.
```
