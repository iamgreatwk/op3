# Bring-up status

| Field | Current value |
| --- | --- |
| Active branch | `agent/implementation/s6e3fa5-linux72-port` (agents commit only to `agent/*`; `bringup` is Integration-controlled) |
| Governance baseline commit | `b8ec809f5f97f9db9de5deaa3a4f75a200aa0659` |
| Project remote | `https://github.com/iamgreatwk/op3.git` |
| Project kernel baseline (2026-08-30) | **pmOS 6.12.1 LTS** (LTS EOL 2028-12-31) — validated end-to-end through layer 07. Linux 7.x line SHELVED (OP3-BOOT-040/041: 7.0-rc1 fails with both upstream and v74 DTB; regression window 6.19.5→7.0-rc1; blind testing without UART — resume criteria in `docs/handoff/linux70rc1-minimal-ab.md`). `source/` reduced to mainline-6.12.1, pmos-6.12, pmos-6.3.1; unpushed 7.x-tree work archived in `patches/shelved-7x/` |
| Previous PASS milestone | OP3-BOOT-036/037 (6.16/6.19 + v74 DTB), **OP3-BOOT-042 (pmOS 6.12 own RPM-GLINK DTB)**, **OP3-BOOT-043 (reproducible base-initramfs control)**, **OP3-BOOT-044 (explicit MSM8996 firmware provenance)**, OP3-DRM-005 (RGB sequence), OP3-GPU-001/002 (A530 firmware), OP3-EGL-001 (kmscube), OP3-WAYLAND-001 (weston + simple-egl), OP3-BROWSER-001 (cog/WPE fullscreen page), OP3-BROWSER-002 (6.3.1 kernel cross), OP3-BROWSER-003 (baseline close-out retest, first full ACM kernel-log capture from boot) — all evidence only; acceptance is Integration's decision |
| Current hypothesis | 7.x regression is inside the 6.19.5→7.0-rc1 merge window (config/DTB/initramfs/packing excluded by A/B); shelved pending a physical UART for early-boot visibility. On pmOS 6.12, the RPM topology is the resolved own-DTB boot factor: direct `rpm-glink` passes OP3-BOOT-042. The pmOS 6.12.1 control path (panel, DRM, GPU firmware, EGL, Wayland, browser) is fully validated and is the product path. |
| Next action | The own-DTB, initramfs-provenance and Cog/WPE browser path is complete through OP3-BROWSER-004. If needed, open separate branches for MSS/modem, Venus and ath10k driver-module tests; do not combine them with boot-image work. Separate open lines: GPU regulator nodes in DTB (hard-reset root cause) and pmOS 6.12 kernel rebuild with `CONFIG_U_SERIAL_CONSOLE=y`; 7.x resume requires physical MSM8996 UART. |
| Device state | OnePlus 3 boots pmOS 6.12.1 via `fastboot boot` with its own direct-RPM-GLINK DTB and explicit firmware provenance (OP3-BOOT-044); DSI panel (1080x1920, s6e3fa5) works through DRM→EGL→Weston→Cog/WPE on the same own-DTB Image/DTB combination (OP3-BROWSER-004, owner visually confirmed). |
| USB COM / USB network | RNDIS (SSH 172.16.42.1) verified; ACM debug console verified end to end including full boot-log capture (`artifacts/console-browser-retest-20260830.log`, 554 lines) — see `docs/known-issues.md` and `scripts/99-op3-acm.rules`. |

## Legacy reference source (do not build by default)

- Remote: `https://gitlab.com/msm8996-mainline/linux.git`
- Branch: `msm8996-stable-6.3.y`
- Commit: `9895e7e38b829a810b9f75d1f98c9e4349ae454a`
- Version: Linux 6.3.1
