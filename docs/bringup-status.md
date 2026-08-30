# Bring-up status

| Field | Current value |
| --- | --- |
| Active branch | `agent/implementation/s6e3fa5-linux72-port` (agents commit only to `agent/*`; `bringup` is Integration-controlled) |
| Governance baseline commit | `b8ec809f5f97f9db9de5deaa3a4f75a200aa0659` |
| Project remote | `https://github.com/iamgreatwk/op3.git` |
| Current layer | 05 EGL PASS on the pmOS 6.12 control (parallel diagnostic layers); Linux 7.2 clean-rebuild still blocked on early boot |
| Previous PASS milestone | OP3-BOOT-036/037 (6.16/6.19 + v74 DTB), OP3-DRM-005 (RGB sequence), OP3-GPU-001/002 (A530 firmware), OP3-EGL-001 (kmscube on FD530) — all evidence only; acceptance is Integration's decision |
| Current hypothesis | Linux 7.2 has an independent early-boot issue (fails with the v74 DTB); the pmOS 6.12 control path (panel, DRM, GPU firmware, EGL, ACM console) is fully validated and serves as the debugging baseline. |
| Next action | 1) Capture a Linux 7.2 early-boot log (ACM console workflow now proven on the 6.12 control); 2) optional parallel: 06 Wayland/compositor gate on the control image; 3) separate tasks: GPU regulator nodes in DTB (runtime-PM hang root fix), pmOS 6.12 kernel rebuild with `CONFIG_U_SERIAL_CONSOLE=y`. |
| Device state | OnePlus 3 boots the pmOS 6.12 + v74-DTB control images via `fastboot boot`; DSI panel (1080x1920, s6e3fa5) works through the full DRM→EGL stack. |
| USB COM / USB network | RNDIS (SSH 172.16.42.1) verified; ACM debug console (kernel log relay + interactive shell) verified end to end — see `docs/known-issues.md` and `scripts/99-op3-acm.rules`. |

## Legacy reference source (do not build by default)

- Remote: `https://gitlab.com/msm8996-mainline/linux.git`
- Branch: `msm8996-stable-6.3.y`
- Commit: `9895e7e38b829a810b9f75d1f98c9e4349ae454a`
- Version: Linux 6.3.1
