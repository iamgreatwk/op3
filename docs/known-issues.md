# Known issues

- **Linux 7.2 卡 fastboot（未解决）**：7.2（torvalds mainline + 本地显示补丁）即使用 v74 DTB 也卡 fastboot。已排除：DTB、cmdline/initramfs、`CONFIG_ARM64_LSUI`、关键驱动配置。7.2 有独立的内核早期问题，需早期日志定位（见 `docs/handoff/root-cause-dtb-key.md`）。
- **6.12/6.16/6.19 用自己的 DTB 卡 fastboot（未解决）**：这些内核用 6.3.1-v74full 编译的 v74 DTB 都能启动，但用各自 DTB 卡。最可疑差异是新 `qcom,rpm-proc` DTS 结构（待验证）。
- 6.19.5 之前卡 fastboot 的原因是 `CONFIG_SCSI_UFS_QCOM=m`（模块），v74 全内置配置修正为 `=y` 后解决（OP3-BOOT-037 PASS）。
- Legacy 6.3.1 findings, including A530 runtime-PM and no-preempt behavior,
  are evidence only and require new Linux 7.2 A/B reproduction.
