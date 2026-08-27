# OnePlus 3 Linux 7.2 / v56 pmOS-initramfs A/B

```text
Task / GitHub Issue: Owner-authorized early-boot display/PID-1 observability A/B; no GitHub Issue supplied
Role: Implementation agent
Baseline commit: d73a640c6af7ad461bce8f54967c0abdf44d1204 (Linux v7.2-3-gd73a640c6)
Working branch: agent/implementation/s6e3fa5-linux72-port
Changed files: docs/handoff/v56-initramfs-linux72-ab.md; docs/test-matrix.md
Artifact-definition commit: e861fba7777f885d7cfbd419dca9f4966b0a9dba

Layer: Boot / early boot userspace observability
Hypothesis tested: Linux 7.2 reaches the known pmOS v56 debug-shell userspace
far enough to make its framebuffer debug shell or splash visible.
Only variable changed: the initramfs is the byte-preserved v56 pmOS ramdisk;
Linux 7.2 Image.gz, Linux 7.2 OnePlus 3 DTB, boot header profile, and the
UUID-free command line remain fixed.

Build run by project owner: NOT_RUN (this task does not compile)
Build result: NOT_RUN
Artifacts and SHA256:
- Linux 7.2 Image.gz: `4a636bc445a000f8f57208220472109752b0f8e3799e0c7778151d1700e48b56`
- Linux 7.2 msm8996-oneplus3.dtb: `87963b9340d437abb6bfe387e05327bcb24766e81d515df9513ce0da1a4eea45`
- v56 reference image:
  `/home/kai/下载/WorkBuddy-20260826/WorkBuddy/2026-08-04-10-03-12/agent-os/_mainline_test/boot_fa5_v56.img`
  SHA256 `484a4e69884b42521dd53a44180cefa24c1adf23f52d056688b3fe6f4692eea9`
- byte-preserved v56 initramfs: `artifacts/reference-initrd-v56.img`
  SHA256 `69d6188bf9eafd46ece944ea88a2f2c06e94ee6f2edaee6e4da0ca91f4dfb843`
- output: `artifacts/boot-oneplus3-fa5-linux72-v56-debug-initramfs.img`
  SHA256 `36eaf12394c6fec85dbed2e6241fb3d2075af1a2b4c53f54c818ab4f0dcc90a5`
- verified header: Android boot v0; page size 4096; kernel `0x80008000`;
  ramdisk `0x81000000`; tags `0x80000100`
- actual cmdline: `fbcon=nodefault console=tty0 pmos.debug-shell`

Device test run by project owner: `fastboot boot` only; no flash
Device result: FAIL. The device returned to fastboot mode; no pmOS splash or
debug-shell framebuffer was observed.
Evidence links / log paths: owner report, 2026-08-27.

Conclusion: REJECTED. Replacing the minimal diagnostic ramdisk with the
substantially fuller v56 pmOS initramfs did not let Linux 7.2 reach a visible
debug-shell userspace.
Uncertainties: This legacy initramfs includes 6.3.1 modules, so they are not
compatible with Linux 7.2 and must not be treated as 7.2 dependencies. In
particular, 7.2 currently has `CONFIG_USB_CONFIGFS=m`; the v56 `libcomposite`
and gadget modules cannot load into 7.2. USB ACM/RNDIS is therefore explicitly
not a PASS condition for this A/B. The v56 initramfs also normally knows the
old pmOS root UUID, but this image's command line deliberately omits every
`pmos_boot_uuid`, `pmos_root_uuid`, and rootfs option. `pmos.debug-shell`
should stop before rootfs handoff.
Recommended next experiment: do not make another ramdisk-only variation.
The unchanged failing variable across OP3-BOOT-001, OP3-BOOT-002, and this
test is the Linux 7.2 kernel/DTB payload. Diagnose that layer using an
independent early-boot observation path; do not alter GPU/DRM/PM merely in
response to a fastboot return.
```
