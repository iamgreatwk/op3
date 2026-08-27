# OnePlus 3 Linux 7.2 / v56-DTB cross A/B

```text
Task / GitHub Issue: Owner-authorized early-boot DTB isolation A/B; no GitHub Issue supplied
Role: Implementation agent
Baseline commit: d73a640c6af7ad461bce8f54967c0abdf44d1204 (Linux v7.2-3-gd73a640c6)
Working branch: agent/implementation/s6e3fa5-linux72-port
Changed files: docs/handoff/linux72-v56-dtb-ab.md; docs/test-matrix.md
Artifact-definition commit: pending

Layer: Boot / early boot kernel-versus-DTB isolation
Hypothesis tested: The actual v56 DTB (not the newer v100 DTB) provides a
legacy hardware description that changes Linux 7.2's early boot outcome.
Only variable changed: appended DTB changes from the Linux 7.2 DTB to the
raw DTB extracted from `boot_fa5_v56.img`. The Linux 7.2 pstore Image.gz,
v56 initramfs, boot header profile, and UUID-free command line remain fixed.

Build run by project owner: NOT_RUN (the already-built pstore Image.gz is reused)
Build result: NOT_RUN
Artifacts and SHA256:
- Linux 7.2 pstore Image.gz: `532f710146b7b7529ec36dddb0d577378094255cc19d993f37c216c299dfb8b9`
- v56 source image: `boot_fa5_v56.img`, SHA256
  `484a4e69884b42521dd53a44180cefa24c1adf23f52d056688b3fe6f4692eea9`
- raw v56 OnePlus 3 DTB: `artifacts/v56-reference-msm8996-oneplus3.dtb`,
  73,112 bytes, SHA256
  `31b698bb08c7a1971dfcbbadf478ca5453b18446a2d2763427babb4b321622f2`
- fixed v56 initramfs: `artifacts/reference-initrd-v56.img`, SHA256
  `69d6188bf9eafd46ece944ea88a2f2c06e94ee6f2edaee6e4da0ca91f4dfb843`
- output: `artifacts/boot-oneplus3-fa5-linux72-v56-dtb-v56-initramfs.img`,
  SHA256 `c0ec4ff4715bc726b25cffa41a2c9ca3f4e6de5ca8fc78fa4a22a083947245df`
- verified header: Android boot v0; page size 4096; kernel `0x80008000`;
  ramdisk `0x81000000`; tags `0x80000100`
- actual cmdline: `fbcon=nodefault console=tty0 pmos.debug-shell`

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: owner should retain `fastboot boot` output and
report fastboot return versus a visible v56 debug-shell display.

Conclusion: INCONCLUSIVE pending owner-run test.
Uncertainties: v56 DTB is legacy evidence only and must not be copied into the
Linux 7.2 DTS. This is a transient binary A/B to distinguish the v56 DTB from
the already-tested v100 DTB; it is not a proposal to replace upstream DTS.
Recommended next experiment: owner runs only `fastboot boot` of the output.
```
