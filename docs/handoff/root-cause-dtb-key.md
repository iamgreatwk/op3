# 根因确认：DTB 是关键——所有 6.x 用 v74 DTB 都能启动，7.2 仍阻塞

## 结论（决定性，2026-08-29）

**OnePlus 3 能启动所有 6.x pmOS 内核（6.3.1/6.12/6.16/6.19.5），只要使用 6.3.1-v74full 编译的 v74 DTB；每个内核用自己编译的 DTB 都卡 fastboot。**

这解释了此前所有"6.12 能启动、6.16/7.2 卡"的困惑——6.12 的"成功镜像"实际打包的是 **v74 DTB**（哈希 `463b2c72` 与 6.3.1-v74full 完全一致），并非 6.12 自己的 DTB。

## 测试矩阵（2026-08-29）

| ID | 内核 | DTB | 结果 |
| --- | --- | --- | --- |
| OP3-BOOT-035 | pmOS v6.16.12 stable（tag `v6.16.12-msm8996`） | 616 自己 | FAIL |
| OP3-BOOT-036 | pmOS v6.16.12 stable | **v74 DTB** | **PASS** |
| OP3-BOOT-037 | pmOS v6.19.5（UFS 修正 =y） | **v74 DTB** | **PASS** |
| OP3-BOOT-038 | Linux 7.2（torvalds mainline + 显示补丁） | **v74 DTB** | FAIL |
| OP3-BOOT-039 | Linux 7.2 + 关闭 `CONFIG_ARM64_LSUI` | **v74 DTB** | FAIL |

## DTB 结构差异（v74 vs 6.16/6.19 自己 DTB）

6.16/6.19 自己的 DTB 与 v74 DTB 最显著的结构差异是 **RPM 节点结构**：

- v74（6.3.1）：顶层 `rpm-glink { compatible="qcom,glink-rpm"; rpm-requests{ compatible="qcom,rpm-msm8996" } }`
- 6.16/6.19：新包装 `remoteproc { compatible="qcom,msm8996-rpm-proc","qcom,rpm-proc"; glink-edge{...} rpm-requests{ compatible="qcom,rpm-msm8996","qcom,glink-smd-rpm" } }`

6.16/6.19 内核的 RPM 驱动链完整（`CONFIG_QCOM_SMD_RPM=y` 编译 `rpm-proc.o`/`smd-rpm.o`，`CONFIG_RPMSG_QCOM_GLINK_RPM=y`），理论上支持新结构。**rpm-proc 结构是否就是导致"自己 DTB 卡"的根因，尚未验证（下一步建议：将 6.16 DTS 的 rpm-proc 回退为旧 rpm-glink 结构编译 DTB 测试）。**

其他 6.16 独有节点（非早期关键）：`fastrpc`、SMMU `cb@*`、`etm`、`camss@a34000`、PCIe `bus-range` 等。

## 7.2 为什么仍卡（已排除项）

7.2 = torvalds mainline 7.2 + 本地显示补丁（fa5 面板/dts/div1-clk 后被 revert），**非 msm8996-mainline 分支**（该上游最高只维护到 6.19.y，master=6.16-rc2）。

已排除：
- DTB（v74 DTB 也卡）→ 与 6.x 不同，7.2 问题在内核自身
- cmdline / initramfs（与 6.19 完全一致，同一个 v100 initramfs）
- `CONFIG_ARM64_LSUI=y`（7.2 唯一新增的 ARMv9 指令特性，A53 不支持；关闭后仍卡）
- 关键驱动配置（UFS/ADSP/MMCC/RPM/REMOTEPROC 全部 =y，SMEM/SMP2P/SMSM =y）

7.2 与 6.19.5 的 arch/arm64 配置差异仅 8 行（3 个 ERRATUM、LSUI、CNP workaround、LSE 改名），早期驱动配置一致。

## 下一步（7.2，未解决）

1. **获取 7.2 早期启动日志**：重建带 USB ACM gadget 或 earlycon 的 7.2（当前 `out/linux-7.2-oneplus3-usb-acm-debug` 是旧配置 VA52/EFI on/UFS=m，需按 v74 strict 重建），确定卡在哪一行代码。
2. **对比 6.19.5（能启动，msm8996-mainline）vs 7.2（mainline）早期代码**：head.S / clk-msm8996 / smem / pinctrl-msm8996 等。
3. **平行线（让 6.x 用自己的 DTB 也能启动）**：验证 rpm-proc 结构假设。

## 关键产物

- 6.16.12 worktree：`source/linux-pmos-msm8996-6.16`（tag `v6.16.12-msm8996`，2025-10-28）
- 6.19.5 构建：`out/pmos-msm8996-6.19.5-v74strict`（`CONFIG_SCSI_UFS_QCOM=y`）
- v74 DTB：`out/pmos-msm8996-6.3.1-v74full/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb`（73383 bytes，`463b2c72...`）
- 可启动镜像：`artifacts/boot-oneplus3-pmos616-v74strict-v74dtb.img`、`artifacts/boot-oneplus3-pmos6195-v74strict-v74dtb.img`
- 打包基线：`scripts/pack-boot.sh` + `boot/oneplus3-fa5.env`（cmdline `fbcon=nodefault console=tty0 pmos.debug-shell`）

## 记录

- 6.19.5 之前卡 fastboot 的部分原因：`CONFIG_SCSI_UFS_QCOM=m`（模块），v74 全内置配置修正为 `=y` 后与其他内核一样可用 v74 DTB 启动。
- 6.16/6.19 的 oneplus3.dts 面板修复：6.16.12 需要 fa3→fa5（本地补丁）；6.19.5 已是 fa5。
