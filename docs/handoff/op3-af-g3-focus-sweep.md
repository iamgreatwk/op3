# OP3 自动对焦 G3：镜头位移扫焦证据

阶段：`OP3-AF-G3`  
日期：2026-09-13  
状态：**BUILD-PASS / I2C-PASS / LENS-MOTION-INCONCLUSIVE / AF-NOT-RUN**

## 交接

Task / GitHub Issue: `OP3-AF-G3` / [Issue #12](https://github.com/iamgreatwk/op3/issues/12)  
Role: Implementation  
Baseline commit: `67b0bbc3cbf46bae712a2606a43361756fcbd829`  
Working branch: `agent/implementation/recovery-browser-001`  
Changed files: `scripts/op3-v4l2-af-stream-test.c`  
Commit SHA: 本文提交所在 commit 见 Git 历史

Layer: 用户态 V4L2 连续采集与 VCM 控制验证  
Hypothesis tested: 在同一次活动采集会话中，合法位置命令能驱动镜头并改变成像清晰度  
Only variable changed: VCM position；内核、DTS、CCI、曝光、增益、采集模式保持不变

## 工具修正

第一轮扫焦初筛每个位置只等待约两帧、保存一帧，不满足执行计划规定的稳定性条件。
本次修正后的 helper：

- 位置写入后保持采集线程运行，焦点线程等待 `150000 us`；
- 每个位置保存连续三帧，而不是单帧；
- 继续使用同一个 `STREAMON` 和 lens fd，不重载驱动、不重复启动停止视频流；
- 记录位置命令的单调时钟、样本帧范围和稳定等待时间；
- 扫焦结果按 `G3` 标记，普通一次位置测试仍按 `G1` 标记。

主机静态构建使用：

```sh
aarch64-linux-gnu-gcc-11 -static -O2 -Wall -Wextra -Werror -pthread \
  scripts/op3-v4l2-af-stream-test.c \
  -o out/op3-af-g3-helper-20260913/op3-v4l2-af-stream-test.aarch64
```

本轮构建结果：`BUILD_PASS`。产物 SHA256：

```text
88ab4833562d8e6347420837cc21aea6e22f9f214de9186eb56c62872837c7a8  op3-v4l2-af-stream-test.aarch64
b7cea2c4b8e3b1bbb195a7b290f9b87e815c807d4417d5a15cbd5f9e6cc852ca  op3-v4l2-af-stream-test.host
```

## 先前设备初筛

设备使用临时镜像
`artifacts/boot-oneplus3-pmos612-recovery-imx298-afpwdm-20260909.img`，SHA256
`64ccf213644f517f8b8c8b512869f396cb75c90920f89163fb8385a244dce4e2`，相机内核
commit `004cda8e061387d07ab4dd08c474910eedd3eb24`，CCI 模块 SHA256
`1691a3a35b6e24145e705074a92b7641525e2909372bb25a65ad621049107d25`。

2026-09-13 的初筛命令为：

```text
op3-v4l2-af-g3-sweep-20260913 --frames 40 \
  --sweep 0,256,512,768,1023 --exposure 893 --gain 240
```

它在一次连续流中收到 40/40 个完整 RAW10 帧，五次 VCM ioctl 均返回 0，CCI
均记录地址 `0x0e`、长度 5 的事务，没有 CSI/CAMSS/SMMU 错误或重启。因此
`I2C_PASS` 和采集前提成立；但这轮等待/采样不足，只作为初筛，不能作为最终 G3
光学结论。

初筛样本位于：
`artifacts/op3-af-evidence/device-g3-sweep-20260913/tmp/`。
按正确的逐行 RAW10 解包、去除每行 3 字节 padding 后，中央 ROI 的 Tenengrad
约为：

```text
position  0       256     512     768     1023
score     1.928   1.927   1.918   1.917   1.922
```

最大差异约 0.6%，没有可重复的清晰度峰值。样本画面低纹理，不能区分“镜头没动”
和“目标不适合验焦”，所以 `LENS-MOTION` 暂为 `INCONCLUSIVE`，不得进入 G4。

## 当前设备测试状态

修正后的 helper 已通过 host/ARM64 编译，但本记录生成时 `fastboot devices` 未
发现手机，因此严格 G3 的 150 ms + 三帧复测为 `NOT_RUN`。不得把修正工具的构建
结果写成设备光学 PASS。

复测必须使用固定手机、光线充足的高对比文字/边缘目标，并运行：

```sh
/newroot/tmp/op3-v4l2-af-g3-sweep-20260913 \
  --frames 60 --sweep 0,256,512,768,1023 \
  --exposure 893 --gain 240 \
  --output-prefix /tmp/op3-af-g3-sweep-20260913
```

每个位置应有三个连续样本，位置写入到样本第一帧之间至少 `150000 us`；同时
收集完整 dmesg、逐帧元数据和五组 RAW。评分必须使用同一 ROI、同一 RAW10 解包
和同一缩放，不能逐帧自动拉伸。

## 判定

- `BUILD_PASS`: 修正后的静态 helper 构建通过。
- `I2C_PASS`: 初筛中五次合法位置写均 `ioctl_rc=0`，对应 CCI 事务完成。
- `LENS_MOTION_PASS`: **未通过**；初筛没有有效光学位移证据，严格复测尚未执行。
- `AF_PASS`: **NOT_RUN**；闭环自动对焦必须等待 G3 通过。

## 下一实验

先执行严格 G3 复测，不改内核或 DTS。若高对比目标下仍无清晰度变化，再进入
`OP3-AF-G2`，一次只处理一个有来源的原因：核对原厂 AF 初始化/servo 前置序列、
EEPROM infinity/macro 校准是否必须，以及 VAF/AF_PWDM 的实际电气状态。当前没有
足够来源安全猜写新的 VCM 寄存器；不得整包移植 OIS 固件或向 EEPROM 写入校准。
