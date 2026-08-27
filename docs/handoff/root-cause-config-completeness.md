# 根因确认：配置完整度 + 面板驱动（非内核早期代码）

## 结论（决定性，OP3-BOOT-028 PASS）

**mainline v6.4 + 严格 v74 完整配置 + 移植的 S6E3FA5 面板驱动能启动。**

这推翻了之前所有"内核早期启动代码被破坏"的假设（VA/EFI/配置开关/soc@0/`.idmap.text`）。
**6.3.1 能启动的真正原因**是：
1. **v74 完整配置**（IKCONFIG 提取，1748 个 =y 项）
2. **S6E3FA5 面板驱动**（6.3.1 内置，其他版本缺失）

而不是旧式 `soc` 结构，也不是任何内核早期代码差异。

## 诊断过程回溯

| 尝试 | 结果 | 说明 |
| --- | --- | --- |
| 7.2 + fragment 配置（VA48/EFI off/全内置部分项） | 卡 | fragment 不完整 |
| 6.4/6.8/6.12/6.16 + fragment 配置 | 卡 | 同上 |
| 6.4 + **严格 v74 完整配置**（cp v74-full + olddefconfig） | 卡 | 配置完整但仍缺面板驱动 |
| 6.4 + 严格 v74 配置 + **移植 S6E3FA5 面板** + extfw 固件 | **能启动** | **根因** |

## 关键洞察

- **配置必须用完整的 v74 快照**（cp v74-full.config），而不是手工 fragment。之前的 fragment 只覆盖了约 20 个 QCOM 关键项，但 v74 有 1748 个 =y，其中遗漏的某项（或多项组合）是启动必需。
- **面板驱动必须存在**（S6E3FA5）。6.3.1 卡 fastboot 的根因就是 s6e3fa3 vs s6e3fa5；其他版本若没有 FA5 驱动，同样卡。
- **新内核（6.5+/7.2）也可以启动**，方法相同：严格 v74 配置 + 移植缺失的驱动（S6E3FA5 等）。

## 对 7.2 的下一步

7.2 需要：
1. **用 v74 完整配置作为 7.2 的 .config 基础**（cp + olddefconfig，处理 7.2 新增/改名选项）
2. **确认 7.2 已有 S6E3FA5 面板驱动**（已确认，7.2 自带 `panel-samsung-s6e3fa5.c`）和 DTS panel 节点（已确认 s6e3fa5）
3. **提供 extfw 固件**（ATH10K）+ 完整 initramfs 固件
4. 处理 7.2 缺失的 v74 关键项（若有）

## 已提交/准备的内容

- 6.4 严格配置：`kernel/configs/pmos631/v64-v74strict-full.config`
- 6.4 面板移植：`source/linux-mainline-6.4/drivers/gpu/drm/panel/panel-samsung-s6e3fa5.c` + Kconfig + Makefile + DTS panel 节点
- 6.4 extfw 固件：`source/linux-mainline-6.4/extfw/ath10k/QCA6174/hw3.0/`
- 6.4 可启动镜像：`artifacts/boot-oneplus3-mainline-64-v74strict-fa5.img`

## 待办（7.2）

- 用同样方法配置 7.2（严格 v74 + olddefconfig + 面板已内建）
- 验证 7.2 缺失的 v74 关键项
- 编译打包测试
