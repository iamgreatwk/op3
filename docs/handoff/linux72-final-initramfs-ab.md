# OnePlus 3 Linux 7.2 / complete legacy-initramfs A/B

```text
Task / GitHub Issue: Owner-authorized early-boot initramfs isolation A/B; no GitHub Issue supplied
Role: Implementation agent
Baseline commit: d73a640c6af7ad461bce8f54967c0abdf44d1204 (Linux v7.2-3-gd73a640c6)
Working branch: agent/implementation/s6e3fa5-linux72-port
Changed files: docs/handoff/linux72-final-initramfs-ab.md; docs/test-matrix.md
Artifact-definition commit: ab6b96e167089ecaa851ce2919e9f8cd3632d25b

Layer: Boot / early boot initramfs isolation
Hypothesis tested: The complete, gzip-valid early pmOS initramfs from the
known historical boot_fa5_final.img changes the Linux 7.2 early-boot outcome
compared with the previously tested v56 ramdisk.
Only variable changed: ramdisk changes from reference-initrd-v56.img to the
complete gzip stream extracted from boot_fa5_final.img. The Linux 7.2 pstore
Image.gz, Linux 7.2 DTB, Android v0 header profile, and UUID-free debug-shell
command line remain fixed.

Build run by project owner: NOT_RUN (the already-built pstore Image.gz is reused)
Build result: NOT_RUN
Artifacts and SHA256:
- Linux 7.2 pstore Image.gz:
  `532f710146b7b7529ec36dddb0d577378094255cc19d993f37c216c299dfb8b9`
- Linux 7.2 OnePlus 3 DTB:
  `87963b9340d437abb6bfe387e05327bcb24766e81d515df9513ce0da1a4eea45`
- source reference boot_fa5_final.img:
  `f2c106a44a47781449a76790a73ecc2b2342e52319eb354fae4aac502dc069aa`
- extracted complete initramfs: `artifacts/reference-initrd-final.img`,
  8,880,818 bytes, gzip-valid, SHA256
  `3dba57f59ba1038f6f2b1f05da6324e6c0887953af936279b246cf6189919e37`
- output: `artifacts/boot-oneplus3-fa5-linux72-pstore-final-initramfs.img`,
  SHA256 `e82a00155e5e95002f80ae3154411c93227fedd720ce598f7f02e93fff4fca9f`
- verified header: Android boot v0; page size 4096; kernel `0x80008000`;
  ramdisk `0x81000000`; tags `0x80000100`
- actual cmdline: `fbcon=nodefault console=tty0 pmos.debug-shell`

The source image's old pmOS boot/root UUIDs were examined only as legacy
reference and are deliberately omitted. This initramfs contains legacy 6.3.1
modules, so a Linux 7.2 module or USB-gadget result is not an acceptance
criterion for this A/B.

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: owner should retain `fastboot boot` output and
report fastboot return versus visible pmOS debug-shell/display behaviour.

Conclusion: INCONCLUSIVE pending owner-run test.
Uncertainties: This is a transient boot binary A/B, not a proposal to adopt
the legacy initramfs, DTB, root UUIDs, or legacy kernel behaviour.
Recommended next experiment: owner runs only `fastboot boot` of the output.
```
