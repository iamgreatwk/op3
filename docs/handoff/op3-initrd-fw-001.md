# OP3-INITRD-FW-001 — next-agent handoff

```text
Task / GitHub Issue: https://github.com/iamgreatwk/op3/issues/3
Role: Implementation
Baseline commit: pmOS MSM8996 6.12.1, 67b0bbc3cbf46bae712a2606a43361756fcbd829
Working branch: agent/implementation/op3-initrd-fw-001
Changed files: NOT_STARTED
Commit SHA: NOT_STARTED

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
Build result: NOT_RUN
Artifacts and SHA256: NOT_RUN

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: NOT_RUN

Conclusion: INCONCLUSIVE
Uncertainties: The OP3-BOOT-043 archive is reproducible but its contents still
derive from a pinned historical archive. Do not put undocumented firmware or
large binaries in Git; use a source/provenance and SHA256 manifest.
Recommended next experiment: Inventory every firmware entry first. Then stage
only the required MSM8996 firmware under a deterministic script and prove that
every non-firmware archive entry remains unchanged before an owner boot test.
```
