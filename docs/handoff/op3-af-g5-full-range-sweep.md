# OP3 自动对焦 G5：全范围 VCM 扫焦

日期：2026-09-14  
阶段：`OP3-AF-G5`  
状态：**CAPTURE-PASS / VCM-IOCTL-PASS / OPTICAL-FOCUS-INCONCLUSIVE**

## 实验边界

本轮只改变 VCM position；内核、DTS、CCI、IMX298、BU63165GWL、曝光 `893`、增益
`240`、RAW10 模式和模块加载顺序均保持不变。使用临时镜像
`boot-oneplus3-pmos612-recovery-imx298-afpwdm-20260909.img`，SHA256：

```text
64ccf213644f517f8c8b8c512869f396cb75c90920f89163fb8385a244dce4e2
```

设备从 fastboot 干净启动后，通过 `scripts/op3-camera-load-modules.sh` 按完整依赖
链加载 `mc`、V4L2、VB2、CCI、CAMSS、BU63165GWL 和 IMX298。设备确认存在
`/dev/media0`、`/dev/video0..5`、`imx298 5-001a` 和 `bu63165gwl 5-000e`。

## 全范围结果

一次连续 `STREAMON` 中测试以下 8 个位置，每个位置等待 150 ms 并保存 3 帧：

```text
0, 128, 256, 384, 512, 768, 896, 1023
```

结果：

- helper 返回 `0`
- `frames_good=256`, `frames_error=0`
- 8 个 `VIDIOC_S_CTRL` 均返回 `ioctl_rc=0`
- CCI master 0 记录了地址 `0x0e` 的位置写事务
- 没有新的 CCI 超时、NACK、VFE overflow、SMMU fault 或重启

RAW10 证据和 dmesg：
`artifacts/op3-af-evidence/device-g5-full-sweep-20260914/`。

对每个位置的 3 帧计算中心/全幅 Tenengrad，均值如下：

```text
position       0      128      256      384      512      768      896     1023
Tenengrad   56.713   57.293   57.519   57.680   57.860   58.414   58.422   59.066
```

清晰度从 `0` 到 `1023` 呈上升趋势，但 `1023` 仍是边界点，没有形成可确认的峰值；
因此本轮证明了全范围控制和采集链路，但不能宣称已经合焦。下一轮应在干净启动后
只做 `768..1023` 的细扫，若仍在 `1023` 达到最高，则需要回到 VCM 协议/校准语义
审计，不能继续盲目扩大位置值。

## 无效的同启动重复开流

在上述成功轮次结束后，未重新启动设备就再次运行高端细扫
`768,800,832,864,896,928,960,1023`。该轮第一条操作返回 `-ETIMEDOUT`，随后
出现 `VFE0 rdi0 overflow`，没有生成 RAW 帧：

```text
G3 result=FAIL rc=-110 errno=110(Connection timed out)
result frames_good=0 frames_error=0
```

这轮不包含任何光学结论，仅作为故障证据保存于
`artifacts/op3-af-evidence/device-g5-high-repeat-fail-20260914/`。以后任何扫焦
实验都必须从干净 `fastboot boot` 开始；不能在同一启动中重复打开 CAMSS，也不能
通过卸载 `qcom-camss`/CCI 恢复状态。

## 结论

`VCM-IOCTL-PASS` 和 `CAPTURE-PASS` 已成立；`LENS-MOTION-PASS`、`AF-PASS` 尚未
成立。当前没有修改内核、DTS、Buildroot 或 EEPROM，也没有刷写分区。
