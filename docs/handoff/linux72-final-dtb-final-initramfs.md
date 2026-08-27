# OnePlus 3 Linux 7.2 / final-DTB + final-initramfs cross test

```text
Task / GitHub Issue: Owner-authorized historical DTB and initramfs cross test; no GitHub Issue supplied
Role: Implementation agent
Baseline commit: d73a640c6af7ad461bce8f54967c0abdf44d1204 (Linux v7.2-3-gd73a640c6)
Working branch: agent/implementation/s6e3fa5-linux72-port
Changed files: docs/handoff/linux72-final-dtb-final-initramfs.md; docs/test-matrix.md
Artifact-definition commit: 046a623135e2a0145debcf275898698d8a297253

Layer: Boot / early boot historical boot companion cross test
Hypothesis tested: Linux 7.2 can enter a different early-boot state when used
with the exact historical DTB and complete early initramfs that accompanied
boot_fa5_final.img.
Variables changed: the appended DTB and ramdisk both change from the Linux 7.2
DTB / prior v56 reference ramdisk to the raw DTB and complete gzip initramfs
extracted from boot_fa5_final.img. Linux 7.2 pstore Image.gz, Android v0
header profile, and UUID-free debug-shell command line remain fixed. This is a
requested compatibility cross test, not a one-variable causal A/B.

Build run by project owner: NOT_RUN (the already-built pstore Image.gz is reused)
Build result: NOT_RUN
Artifacts and SHA256:
- Linux 7.2 pstore Image.gz:
  `532f710146b7b7529ec36dddb0d577378094255cc19d993f37c216c299dfb8b9`
- source reference boot_fa5_final.img:
  `f2c106a44a47781449a76790a73ecc2b2342e52319eb354fae4aac502dc069aa`
- extracted raw OnePlus 3 DTB: `artifacts/reference-final-msm8996-oneplus3.dtb`,
  73,383 bytes, SHA256
  `463b2c7203e28359ce4039c8e4aa9b0d211171879db8bbea07834d3cb8b2bde3`
- extracted complete initramfs: `artifacts/reference-initrd-final.img`,
  8,880,818 bytes, gzip-valid, SHA256
  `3dba57f59ba1038f6f2b1f05da6324e6c0887953af936279b246cf6189919e37`
- output:
  `artifacts/boot-oneplus3-fa5-linux72-pstore-final-dtb-final-initramfs.img`,
  SHA256 `ee7b325517139a9705cd612204a5de6241ded399712b3acafc27280f9551faca`
- verified header: Android boot v0; page size 4096; kernel `0x80008000`;
  ramdisk `0x81000000`; tags `0x80000100`
- actual cmdline: `fbcon=nodefault console=tty0 pmos.debug-shell`

The reference DTB identifies `model = "OnePlus 3"` and compatibles
`oneplus,oneplus3`, `qcom,msm8996`. The source image's old pmOS boot/root UUIDs
were examined only as legacy reference and are deliberately omitted.

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: owner should retain `fastboot boot` output and
report fastboot return versus visible pmOS debug-shell/display behaviour.

Conclusion: INCONCLUSIVE pending owner-run test.
Uncertainties: Both DTB and initramfs are legacy evidence only. They must not
be copied into Linux 7.2 DTS or used as a permanent rootfs/boot design.
Recommended next experiment: owner runs only `fastboot boot` of the output.
```
