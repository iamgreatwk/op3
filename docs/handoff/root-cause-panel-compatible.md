# 根因确认：panel compatible (s6e3fa3 vs s6e3fa5) 导致卡 fastboot

## 结论（决定性）

项目 6.3.1 卡 fastboot 的根因是 **OnePlus 3 面板 compatible 不匹配**：

- **项目 6.3.1 DTS**（`msm8996-oneplus3.dts:48`）声明 `compatible = "samsung,s6e3fa3"`
- **v100 能启动的 DTB** 声明 `compatible = "samsung,s6e3fa5"`
- v74 配置只内置了 **S6E3FA5** 驱动（`CONFIG_DRM_PANEL_SAMSUNG_S6E3FA5=y`）

**机制**：DTB 声明 `s6e3fa3`，但内核只编译了 S6E3FA5 驱动 → DSI 面板 probe 无法匹配驱动 → 早期初始化挂起 → 看门狗复位 → 回 fastboot。v100 用 `s6e3fa5` 正确匹配 FA5 驱动 → 正常启动。

**验证（OP3-BOOT-019 → 用户实测 PASS）**：把项目 6.3.1 的 DTS 从 `s6e3fa3` 改为 `s6e3fa5`，重新编译 DTB，**能启动**。根因 100% 确认。

## 对 7.2 的意义

**7.2 的 panel compatible 已经是 `samsung,s6e3fa5`**（`msm8996-oneplus-common.dtsi:197`），所以 **7.2 卡 fastboot 不是 panel compatible 问题**。7.2 需要单独定位另一个早期挂死点。

## 关键对照（面板差异是项目 6.3.1 的根因，但 7.2 已排除）

| 变量 | 项目6.3.1 DTB (s6e3fa3) | v100 DTB (s6e3fa5) | 7.2 DTB (s6e3fa5) |
| --- | --- | --- | --- |
| panel compatible | s6e3fa3 ✗ | s6e3fa5 ✓ | s6e3fa5 ✓ |
| 启动 | 卡 fastboot | 能启动 | 卡 fastboot（另有原因） |

## 记录

- 项目 6.3.1 的 DTS 已改为 `s6e3fa5`（源码树被项目 .gitignore 忽略，未纳入项目版本控制；此处仅文档记录）
- 该修复解释了 OP3-BOOT-014/018 与 OP3-BOOT-019 的全部差异
