# 根因假设（二分定位）：`.idmap.text` section 权限标志 (awx → a)

## 状态

**假设已用二分法精确定位，尚未经设备验证。** 需要项目所有者修改 `proc.S` 并实测确认。

## 背景

Linux 7.2（及 6.5+ 所有新内核）在 OnePlus 3 / MSM8996 上返回 fastboot，且无任何早期输出
（ACM 无枚举、ramoops 空、无串口）。项目源码 6.3.1（`msm8996-stable-6.3.y`）是唯一能完整启动的
内核。通过二分 msm8996-mainline 的 stable/staging 分支，收敛出精确分界点。

## 二分测试矩阵（全部由项目所有者实测）

| 版本 | 源码 | soc 结构 | 启动 |
| --- | --- | --- | --- |
| **6.3.1** (`msm8996-stable-6.3.y`) | 项目 `linux-pmos-msm8996-6.3.1` | 旧式 `soc` | **能启动** |
| **6.4** (mainline v6.4) | `source/linux-mainline-6.4` | 旧式 `soc` | **卡** |
| **6.8** (`msm8996-stable-6.8.y`) | `source/linux-pmos-msm8996-6.8` | `soc@0` | 卡 |
| **6.12** (`msm8996-stable-6.12.y`) | `source/linux-pmos-msm8996-6.12` | `soc@0` | 卡 |
| **6.16** (`msm8996-staging`) | `source/linux-pmos-msm8996-latest` | `soc@0` | 卡 |
| **6.19.5** | `source/linux-pmos-msm8996-v6.19.5` | `soc@0` | 卡 |
| **7.2** | `source/linux-7.2` | `soc@0` | 卡 |

所有版本测试均使用统一方法：panel 修 `s6e3fa5` + `CONFIG_ARM64_VA_BITS=48` +
`# CONFIG_EFI is not set` + 全内置关键驱动（DRM_MSM/UFS/MMCC/WDT/PHY）+ 完整 initramfs。

**关键二分结果**：6.3.1（旧式 soc）能启动，但 **6.4（同为旧式 soc）卡**。因此
**soc@0 vs soc 不是根因**（两者 DTB 结构等价，仅节点名多 `@0`）。根因在 **mainline
v6.3 → v6.4 之间引入的内核改动**。

## 代码级根因定位：`.idmap.text` section flags

对比 mainline v6.3 / v6.4 / pmOS 6.3.1 的 `arch/arm64/mm/proc.S`：

| 版本 | `.pushsection ".idmap.text", ...` | 启动 |
| --- | --- | --- |
| mainline v6.3 | `"awx"` | — |
| **pmOS 6.3.1**（能启动） | `"awx"` | 能启动 |
| **mainline v6.4** | `"a"` | 卡 |
| **7.2** | `"a"` | 卡 |

**改动点**：mainline v6.3→v6.4 把 `.idmap.text` section 的链接标志从
`"awx"`（allocatable + writable + executable）改为 `"a"`（仅 allocatable，
**不可写、不可执行**）。这是 6.4 的内存保护强化（security hardening）。

**疑似机制**：`.idmap.text` 是**恒等映射（identity map）代码段**，在 MMU 使能前后、
relocation 之前运行（极早期）。改 `a` 后该段不可写/不可执行。若 MSM8996 早期路径
（`__cpu_setup`、早期 fixmap/页表初始化）在此段内写数据，触发写/执行权限 fault →
极早期崩溃 → 无输出 → LK 看门狗 → 回 fastboot。这解释了为何 6.3.1（`awx` 宽松权限）
能启动而 6.4+（`a` 严格权限）卡，且与二分分界点完全吻合。

## 待验证的修复（需项目所有者执行）

把 `source/linux-7.2/arch/arm64/mm/proc.S` 中 3 处
`.pushsection ".idmap.text", "a"` 改回 `.pushsection ".idmap.text", "awx"`
（与能启动的 6.3.1 一致），然后重新编译 Image.gz + DTB，用完整 initramfs 打包 `fastboot boot`。

若离开 fastboot → 根因确认，后续需评估是否以补丁形式固化（注意这是安全加固回退，
需单独 scoped 任务决定是否保留）。

## 相关文件 / 已提交内容

- 二分 fragment：`kernel/configs/pmos631/v64-msm8996-fullbuilt.fragment`,
  `v68-msm8996-fullbuilt.fragment`, `v612-msm8996-fullbuilt.fragment`,
  `v616-msm8996-fullbuilt.fragment`
- 克隆的二分源码（gitignore，不纳入版本控制）：
  `source/linux-mainline-6.4`, `source/linux-pmos-msm8996-6.8`,
  `source/linux-pmos-msm8996-6.12`
- 面板修复：6.8/6.12/6.16/6.3.1 的 `msm8996-oneplus3.dts`（`s6e3fa3`→`s6e3fa5`）

## 结论

6.3.1 能启动的**直接代码差异**（相对 6.4+）是 `.idmap.text` 的 `awx` vs `a`。
这是当前最有把握的早期挂死点，需设备实测确认。
