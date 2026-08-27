# OnePlus 3 pmOS MSM8996 v6.19.5 reference boot test

```text
Task / GitHub Issue: Owner-authorized newer pmOS/MSM8996 reference boot test;
no GitHub Issue supplied
Role: Implementation agent
Baseline: Linux 7.2 upstream remains the formal target; this is legacy
reference evidence only.
Working branch: agent/implementation/s6e3fa5-linux72-port
Changed files:
- kernel/configs/pmos-msm8996-v6195-fa5-reference.fragment
- docs/handoff/pmos6195-final-boot-reference.md
Commit SHA: 9227022fbc36517df3797f76eb7aa8e8e816f098

Reference source (not an implementation target):
- source/linux-pmos-msm8996-v6.19.5
- tag: v6.19.5-msm8996
- commit: 1aed438cb5f49d7a61593f764d4a82f83b14114f

Purpose: establish whether a substantially newer MSM8996 reference kernel can
boot using the known historical FA5 DTB and complete pmOS initramfs. This is
not a Linux 7.2 change and must not be merged into the Linux 7.2 path.

Kernel-side variable: v6.19.5 kernel Image.gz with the display stack built-in:
DRM, DRM_MSM, BACKLIGHT_CLASS_DEVICE, and S6E3FA5 are `=y`.
Boot companions fixed:
- final FA5 DTB: artifacts/reference-final-msm8996-oneplus3.dtb
- final complete initramfs: artifacts/reference-initrd-final.img
- UUID-free cmdline: fbcon=nodefault console=tty0 pmos.debug-shell

Important: final DTB itself identifies the panel as samsung,s6e3fa5. Therefore
the v6.19.5 msm8996-oneplus3.dts is deliberately not modified: its compiled
DTB is not used in this cross test. The fragment makes the matched FA5 driver
built-in so no 6.3.1 module is needed.

Build run by project owner: NOT_RUN
Device test run by project owner: NOT_RUN
Conclusion: NOT_RUN
```
