# Known issues

- **Linux 7.2 卡 fastboot（未解决）**：7.2（torvalds mainline + 本地显示补丁）即使用 v74 DTB 也卡 fastboot。已排除：DTB、cmdline/initramfs、`CONFIG_ARM64_LSUI`、关键驱动配置。7.2 有独立的内核早期问题，需早期日志定位（见 `docs/handoff/root-cause-dtb-key.md`）。
- **主线 v6.12.1 DSI 控制镜像无 USB 网卡/ACM（实测，2026-08-30）**：`artifacts/boot-oneplus3-mainline-6121-dsi-pm-v74dtb-full-initrd.img` 能跑到 `recovery.c`，但实测没有 USB RNDIS 网卡、也没有 ACM 串口，无法取日志。凡是需要日志证据的测试，基线改用 pmOS 6.12 的 `artifacts/boot-oneplus3-pmos612-v74dtb-full-initrd.img`（USB RNDIS/ACM/Dropbear 已验证可用）。
- **6.12/6.16/6.19 用自己的 DTB 卡 fastboot（未解决）**：这些内核用 6.3.1-v74full 编译的 v74 DTB 都能启动，但用各自 DTB 卡。最可疑差异是新 `qcom,rpm-proc` DTS 结构（待验证）。
- 6.19.5 之前卡 fastboot 的原因是 `CONFIG_SCSI_UFS_QCOM=m`（模块），v74 全内置配置修正为 `=y` 后解决（OP3-BOOT-037 PASS）。
- Legacy 6.3.1 findings, including A530 runtime-PM and no-preempt behavior,
  are evidence only and require new Linux 7.2 A/B reproduction.
