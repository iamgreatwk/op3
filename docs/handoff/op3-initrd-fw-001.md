# OP3-INITRD-FW-001 — next-agent handoff

```text
Task / GitHub Issue: https://github.com/iamgreatwk/op3/issues/3
Role: Implementation
Baseline commit: pmOS MSM8996 6.12.1, 67b0bbc3cbf46bae712a2606a43361756fcbd829
Working branch: agent/implementation/op3-initrd-fw-001
Changed files: boot/base-initramfs/msm8996-oneplus3-firmware.tsv;
scripts/stage-msm8996-oneplus3-firmware.sh
Commit SHA: 5795815

Layer: initramfs firmware provenance
Hypothesis tested: Replacing only the declared MSM8996 firmware group with
explicitly sourced and SHA256-verified inputs preserves OP3-BOOT-043 boot.
Only variable changed: Firmware files in the generated initramfs. All other
archive entries, Image.gz, own-DTB, boot profile, cmdline, and sda15 contents
are fixed.

Known-good control: OP3-BOOT-043
  Image.gz: 088d472f90f90388ee90426f190806f721e3e483777c3dfdc9992a4b1321a7ad
  DTB: cb29ab658135cd0cfcde3b47c1e115b763f5dbd37b724554590b7a61afcbf32f
  initramfs: 61cf63388fbd1ef13dd6984e3eace85c9cdd8d7359d128c4c0cef29ce3358f79
  boot image: 0b2e85ee019e7e3434f6d1fbe0245b09024a0dfefddb4d027c7777a4910b2801

Build run by project owner: 2026-08-31, boot-image packaging only; no kernel or
Buildroot build.
Build result: The owner packed the supplied initramfs; no kernel or Buildroot
build was performed.
Host-side staging result: PASS. `mtools 4.0.49` was unpacked outside the
system directories and `pil-squasher` was built from the pmaports-pinned
upstream commit `170b62d29faa7c1f54fc2a7718e4d0c912384ec1`. Both SHA512-pinned
inputs downloaded successfully, and all six staged outputs match the checked-in
size and SHA256 manifest exactly.
Artifacts and SHA256: ignored host artifacts
`artifacts/msm8996-oneplus3-firmware-verified/`; the six file hashes equal
`boot/base-initramfs/msm8996-oneplus3-firmware.tsv`. The generated firmware
provenance initramfs is
`artifacts/initrd-op3-firmware-provenance-v2.cpio.gz`, SHA256
`38383fc04942e599d2c7e520ff1b85739bf4b76c516e00fd74c6a9243f279789`,
with the same 653 pathname list and the same normalized metadata/content
manifest as OP3-BOOT-043. Its archive bytes are not equal to the older control
archive because GNU cpio preserves newly-created temporary directory mtimes;
the six replaced entries retain the control timestamps and no longer introduce
additional metadata differences.

Device test run by project owner: 2026-08-31, `fastboot boot`, followed by
read-only SSH inspection over recovery RNDIS.
Device result: PASS for the firmware-provenance gate. Recovery started and SSH
was available. The device runs `6.12.1-msm8996+`; all six on-device firmware
SHA256 values equal the checked-in manifest. dmesg records SLPI loading at
2.788 s and ADSP loading at 2.801 s. `/dev/sda15` is mounted rw at `/newroot`.
Evidence links / log paths: owner report plus read-only SSH output captured in
the Codex task, 2026-08-31. Boot image
`artifacts/boot-oneplus3-pmos612-own-dtb-firmware-provenance.img`, SHA256
`38e59038daaed7a2726554c5cc56cf19438cf0aa3f72d1874b3a8ec0fa96651f`.

Conclusion: SUPPORTED for firmware provenance on the OP3-BOOT-043 recovery
gate; this is not an Integration acceptance or a placement/split result.
Uncertainties: The OnePlus 3 firmware is proprietary and must not be copied
into Git. ath10k and GPU microcode are outside this provenance group. The
provenance archive has normalized content/metadata equality with the control,
but not byte equality because directory mtimes are not normalized by this GNU
cpio environment.
Recommended next experiment: `OP3-INITRD-SPLIT-001`, a separate placement
variable: retain ADSP, SLPI and small GPU firmware in initramfs; stage
modem/MBA, Venus and ath10k on rootfs and defer their module loads until after
`/newroot` is available.
```
