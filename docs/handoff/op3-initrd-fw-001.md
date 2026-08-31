# OP3-INITRD-FW-001 — next-agent handoff

```text
Task / GitHub Issue: https://github.com/iamgreatwk/op3/issues/3
Role: Implementation
Baseline commit: pmOS MSM8996 6.12.1, 67b0bbc3cbf46bae712a2606a43361756fcbd829
Working branch: agent/implementation/op3-initrd-fw-001
Changed files: boot/base-initramfs/msm8996-oneplus3-firmware.tsv;
scripts/stage-msm8996-oneplus3-firmware.sh
Commit SHA: a864e252452bce5834d1ee0d10d36cea5a846de4

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

Build run by project owner: NOT_RUN
Build result: NOT_RUN. This task has not produced an initramfs or boot image.
Host-side staging result: PASS. `mtools 4.0.49` was unpacked outside the
system directories and `pil-squasher` was built from the pmaports-pinned
upstream commit `170b62d29faa7c1f54fc2a7718e4d0c912384ec1`. Both SHA512-pinned
inputs downloaded successfully, and all six staged outputs match the checked-in
size and SHA256 manifest exactly.
Artifacts and SHA256: ignored host artifact
`artifacts/msm8996-oneplus3-firmware-verified/`; the six file hashes equal
`boot/base-initramfs/msm8996-oneplus3-firmware.tsv`.

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: NOT_RUN

Conclusion: SUPPORTED for deterministic host-side firmware staging; device
boot result remains INCONCLUSIVE.
Uncertainties: The OnePlus 3 firmware is proprietary and must not be copied
into Git. Source URLs are guarded by SHA512 and the reconstructed firmware has
not yet replaced the six fixed entries in a booted initramfs. ath10k and GPU
microcode are outside this provenance group.
Recommended next experiment: make one firmware-provenance initramfs A/B by
replacing only the six fixed files with these verified outputs, then have the
owner pack and boot it. After that result, open `OP3-INITRD-SPLIT-001` for the
separate location variable: retain ADSP, SLPI and small GPU firmware in the
initramfs; stage modem/MBA, Venus and ath10k on rootfs and defer the module
loads until after `/newroot` is available.
```
