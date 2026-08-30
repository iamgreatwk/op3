# OP3-INITRD-001 — reproducible base-initramfs control

```text
Task / GitHub Issue: https://github.com/iamgreatwk/op3/issues/2
Role: Implementation
Baseline commit: pmOS MSM8996 6.12.1, 67b0bbc3cbf46bae712a2606a43361756fcbd829
Working branch: agent/implementation/op3-initrd-001
Changed files: boot/base-initramfs/reference-initrd.sha256;
boot/base-initramfs/README.md; scripts/make-reproducible-base-initrd.sh
Commit SHA: 41a596c3cfdd39f98f10cfb1bab3889892ac3e8c

Layer: initramfs provenance / boot
Hypothesis tested: Deterministic reserialization of the pinned complete
reference initramfs preserves the OP3-BOOT-042 boot result.
Only variable changed: The external gzip/newc ramdisk passed to mkbootimg.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: NOT_RUN

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: NOT_RUN

Conclusion: INCONCLUSIVE
Uncertainties: This control still uses the historical archive as a verified
input. It makes its contents and output reproducible/auditable, but does not
yet establish independent provenance for every firmware and base-userland file.
Recommended next experiment: Generate the archive, package it with the fixed
OP3-BOOT-042 Image.gz and DTB, then test recovery/RNDIS/SSH, DSI, and sda15.
```
