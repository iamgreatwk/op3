# OP3-INITRD-FW-001 — next-agent handoff

```text
Task / GitHub Issue: https://github.com/iamgreatwk/op3/issues/3
Role: Implementation
Baseline commit: pmOS MSM8996 6.12.1, 67b0bbc3cbf46bae712a2606a43361756fcbd829
Working branch: agent/implementation/op3-initrd-fw-001
Changed files: boot/base-initramfs/msm8996-oneplus3-firmware.tsv;
scripts/stage-msm8996-oneplus3-firmware.sh
Commit SHA: c05d56a0a60c600eccff01ce3980c6df76ba9f4c

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
Build result: NOT_RUN. The staging recipe requires owner-approved installation
of host tools `mtools` and `pil-squasher`; neither is presently available.
Artifacts and SHA256: manifest records six expected output files; NOT_RUN.

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: NOT_RUN

Conclusion: INCONCLUSIVE
Uncertainties: The OnePlus 3 firmware is proprietary and must not be copied
into Git. Source URLs are guarded by SHA512 and output hashes must equal the
historical control before an initramfs A/B. The ath10k files remain fixed and
out of scope for this task. `mtools` and `pil-squasher` are absent on the host;
the latter is pinned by the postmarketOS recipe to upstream commit
170b62d29faa7c1f54fc2a7718e4d0c912384ec1.
Recommended next experiment: Owner approves/install the two host tools, runs
the staging script, and verifies every generated output SHA256. Only then add
the one-group initramfs replacement/entry comparison and prepare a device test.
```
