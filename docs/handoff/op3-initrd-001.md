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

Build run by project owner: 2026-08-30
Build result: PASS
Artifacts and SHA256:
- fixed Image.gz: 088d472f90f90388ee90426f190806f721e3e483777c3dfdc9992a4b1321a7ad
- fixed 6.12-built DTB: cb29ab658135cd0cfcde3b47c1e115b763f5dbd37b724554590b7a61afcbf32f
- historical input archive: c3358a1cadb747996ddaa492e636827f2d72974040e8fd40d81f8a213e676366
- generated archive: 61cf63388fbd1ef13dd6984e3eace85c9cdd8d7359d128c4c0cef29ce3358f79
  (`artifacts/initrd-op3-reproducible-base.cpio.gz`, 653 entries)
- generated manifest: c06aeba57c3fcb7af36e4269bff56b12cfa51b6d19d2c8a979ba92a6c24dbfcf
- boot image: 0b2e85ee019e7e3434f6d1fbe0245b09024a0dfefddb4d027c7777a4910b2801
  (`artifacts/boot-oneplus3-pmos612-own-dtb-repro-initrd.img`)

Device test run by project owner: 2026-08-30, `fastboot boot`
Device result: PASS for the initramfs control gate. The device reached
recovery, RNDIS, and SSH. Read-only SSH inspection confirmed the fixed
`qcom,glink-rpm` runtime DTB, DSI-1 `connected`, UFS `sda`, and ext4 `sda15`
mounted read-write at `/newroot`. dmesg records `Unpacking initramfs...`, UFS
SCSI host 0, and MSM DRM initialization.
Evidence links / log paths: Owner report plus read-only SSH/dmesg inspection
captured in the Codex task on 2026-08-30.

Conclusion: SUPPORTED. The generated gzip/newc archive passes the same
recovery/RNDIS/SSH/DSI/UFS gate as the historical control with the Image.gz,
DTB, cmdline, and profile fixed.
Uncertainties: This control still uses the historical archive as a verified
input. It makes its contents and output reproducible/auditable, but does not
yet establish independent provenance for every firmware and base-userland file.
Recommended next experiment: Create a separate firmware/input-migration task.
Copy no arbitrary binary into Git: establish a versioned source and SHA256 for
each required firmware/input group, then replace exactly one group while this
validated archive recipe and the fixed Image.gz/DTB remain controls.
```
