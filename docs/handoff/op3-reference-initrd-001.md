# OP3 standalone reference initrd handoff

```text
Task / GitHub Issue: owner-authorized retirement of boot_fa5_v100_auto.img as a rebuild input
Role: Implementation
Baseline commit: 03d3828bc54bbf742fe01561817613a9fc119930
Working branch: agent/implementation/recovery-browser-001
Changed files: scripts/extract-reference-initrd.sh;
  scripts/verify-op3-external-inputs.sh;
  boot/base-initramfs/reference-boot.sha256;
  boot/oneplus3-fa5.env; manifests/op3-recovery-audio-full.env;
  docs/rebuild-recovery.md; docs/boot-image-format.md;
  docs/handoff/latest.md
Commit SHA: 887357a9ec38fe2fa4e9007495bdcf7ddfd8ae19

Layer: external input and build provenance
Hypothesis tested: The current recovery rebuild needs only the historical
ramdisk from the v100 boot image, so a hash-pinned standalone initrd can
replace the full image without changing the recovery build inputs.
Only variable changed: reference-input representation and path; kernel, DTB,
boot cmdline, recovery runtime, Wi-Fi, audio, browser, and device behavior
were unchanged.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: The standalone external input
`/home/kai/op3-recovery-external-inputs/initrd/reference-initrd.img` was
extracted from the 29ccd3eb... v100 image, verified as gzip, counted as a
653-entry cpio archive, and matched byte-for-byte with the existing
`artifacts/reference-initrd.img`; SHA256 is
`c3358a1cadb747996ddaa492e636827f2d72974040e8fd40d81f8a213e676366`.

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: `scripts/verify-op3-external-inputs.sh` passed;
the standalone-initrd verification/copy script passed. The original image
was moved, not deleted, to the external `archive/` directory and is excluded
from the standard checksum manifest.

Conclusion: SUPPORTED for external-input canonicalization; build and device
behavior remain untested by this checkpoint.
Uncertainties: The external input directory remains outside GitHub and must
be backed up independently. The archive is retained only for provenance.

Recommended next experiment: from a fresh checkout, export
`OP3_EXTERNAL_INPUTS`, run `scripts/verify-op3-external-inputs.sh`, then run
`scripts/extract-reference-initrd.sh` with
`$OP3_EXTERNAL_INPUTS/initrd/reference-initrd.img` before the owner-run
Buildroot/kernel build sequence.
```
