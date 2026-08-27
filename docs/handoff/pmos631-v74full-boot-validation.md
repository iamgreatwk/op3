# 项目 6.3.1 + v74 完整配置 启动复现验证

## 背景与两个待解答问题

1. **为什么之前没按 v74 配置基线？**
   此前（OP3-BOOT-015/016/017）只做了"defconfig + 少量 fragment"，**没有使用 v74 完整配置**。这是错误的做法。`Image-v74`（WorkBuddy 文档钦定的"内核配置唯一基准"）提取的完整 IKCONFIG 配置，其内核版本正是 **6.3.1**（VERSION=6, PATCHLEVEL=3, SUBLEVEL=1），与项目里的
   `source/linux-pmos-msm8996-6.3.1` 源码**完全匹配**，本应直接作为该源码的 .config 使用。

2. **为什么用项目里的 6.3.1 编译仍卡 fastboot？**
   因为 OP3-BOOT-014 用的是 `defconfig + oneplus3-s6e3fa5.fragment`，**不是 v74 完整配置**。defconfig 与 v74 的关键差异（详见交接记录）：
   - defconfig 把 `SCSI_UFS_QCOM`、`PHY_QCOM_QMP_UFS`、USB gadget/configfs、QCOM remoteproc 都设为 `=m`；
   - defconfig **缺失** `MFD_QCOM_RPM`、`REGULATOR_QCOM_RPM`、`QCOM_CLK_RPM`（v74 里都是 `=y`）。
   这些是 MSM8996 早期平台 bring-up 必需的内置项。**defconfig 不等于能启动的 v74 配置。**

## 决定性验证：项目 6.3.1 + v74 完整配置 + 完整 initramfs

目标：用**能启动的 v74 完整配置**（而非 defconfig）编译**项目 6.3.1**，配 **v100 完整 initramfs（含全部 MSM8996 固件 a530_zap/adsp/slpi/mba/modem/venus）**，验证能否复现 v100 的启动。

- 若**能启动** → 证明问题在"配置完整度 + initramfs 固件"，不在内核代码；7.2 要启动也必须达到 v74 的配置完整度并携带固件。
- 若**仍卡 fastboot** → 才真正指向项目 6.3.1 源码与 Image-v74 内核的代码级差异（需进一步审计）。

## 已保存的配置

- 完整 v74 配置（IKCONFIG 提取，7682 行，2182 个选项）：
  `kernel/configs/pmos631/v74-full.config`
- 关键项确认（均在 v74 中，且项目 6.3.1 源码存在对应 Kconfig）：
  `CONFIG_DRM_PANEL_SAMSUNG_S6E3FA5=y`, `CONFIG_SCSI_UFS_QCOM=y`,
  `CONFIG_QCOM_Q6V5_ADSP=y`, `CONFIG_MFD_QCOM_RPM=y`,
  `CONFIG_REGULATOR_QCOM_RPM=y`, `CONFIG_QCOM_CLK_RPM=y`,
  `CONFIG_ATH10K=m`（ATH10K 保持模块，WorkBuddy 铁律 #2）

## 所有者构建命令（agent 不编译）

```bash
project=/home/kai/src/oneplus3-mainline
kernel="$project/source/linux-pmos-msm8996-6.3.1"
output="$project/out/pmos-msm8996-6.3.1-v74full"

mkdir -p "$output" && \
make -C "$kernel" O="$output" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  "$project/kernel/configs/pmos631/v74-full.config" && \
make -C "$kernel" O="$output" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 olddefconfig && \
grep -qx 'CONFIG_DRM_PANEL_SAMSUNG_S6E3FA5=y' "$output/.config" && \
grep -qx 'CONFIG_SCSI_UFS_QCOM=y' "$output/.config" && \
grep -qx 'CONFIG_QCOM_Q6V5_ADSP=y' "$output/.config" && \
grep -qx 'CONFIG_MFD_QCOM_RPM=y' "$output/.config" && \
grep -qx 'CONFIG_ATH10K=m' "$output/.config" && \
make -C "$kernel" O="$output" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  -j"$(nproc)" Image.gz qcom/msm8996-oneplus3.dtb
```

## 打包（用含全部固件的完整 initramfs）

```bash
./scripts/pack-boot.sh \
  "$output/arch/arm64/boot/Image.gz" \
  "$output/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb" \
  artifacts/reference-initrd.img \
  artifacts/boot-oneplus3-pmos631-v74full.img
```

> 注意：`artifacts/reference-initrd.img` 是 v100 完整 initramfs（50 MB，含 a530_zap/adsp/slpi/mba/modem/venus 全部固件）。**必须用它**——因为 v74 配置把 `QCOM_Q6V5_ADSP=y`（ADSP remoteproc 内置），probe 时需要 `adsp.mbn` 固件；minimal/ACM initramfs 无固件会挂。

## 判读

- **离开 fastboot（哪怕黑屏）** → 证明"完整配置 + 完整 initramfs（含固件）"是启动关键，问题不在内核代码。下一步把 v74 的完整配置思路（全内置关键驱动 + 固件）移植到 7.2。
- **仍卡 fastboot** → 指向 6.3.1 源码与 Image-v74 的代码级差异，需逐行审计 head.S/proc.S/早期 C 初始化。
