# OP3 自动对焦 G3：镜头位移扫焦证据

阶段：`OP3-AF-G3`  
日期：2026-09-13  
状态：**BUILD-PASS / I2C-PASS / G3-CAPTURE-PASS / LENS-MOTION-INCONCLUSIVE / AF-NOT-RUN**

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
  -o out/op3-af-g3-helper-20260913-v2/op3-v4l2-af-stream-test.aarch64
```

本轮构建结果：`BUILD_PASS`。提交 `e3d0136` 将最大帧数提高到 256，并要求每个
扫焦位置预留至少 24 帧，避免在最后位置因采集帧数不足而提前结束。v2 产物
SHA256：

```text
e29d96f8ff44972ba5418ced1379fa83b90d9f448e4d699f40eea53c3e768b4f  op3-v4l2-af-stream-test.aarch64
bb3fd770f81deda2ea3f3f945c9596f9be700f5b794ca11bfec6f141cfa5ff47  op3-v4l2-af-stream-test.host
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

## 正式设备复测：G3 采集与 VCM 控制通过（2026-09-13）

设备使用临时启动镜像
`artifacts/boot-oneplus3-pmos612-recovery-imx298-afpwdm-20260909.img`，SHA256
为 `64ccf213644f517f8b8c8b512869f396cb75c90920f89163fb8385a244dce4e2`。这是
一次干净 `fastboot boot` 启动；相机模块没有写入 rootfs，也没有刷写分区。使用的
CCI、VCM、IMX298 模块 SHA256 分别为：

```text
1691a3a35b6e24145e705074a92b7641525e2909372bb25a65ad621049107d25  i2c-qcom-cci-g1-20260913.ko
cd638af89815c4967ba5e718dc4c4f20130a972da9ba3736bfe16b114fa0b063  bu63165gwl.ko
103fe40cad22972ed3408b76f997dc8a06e1c09a4b2dba03e8dc71874144e8f9  imx298.ko
e29d96f8ff44972ba5418ced1379fa83b90d9f448e4d699f40eea53c3e768b4f  op3-v4l2-af-stream-test.aarch64
```

正式命令为：

```sh
/newroot/tmp/op3-v4l2-af-g3-v2-20260913 \
  --frames 128 --sweep 0,256,512,768,1023 \
  --exposure 893 --gain 240 \
  --output-prefix /tmp/op3-af-g3-formal-v2-20260913
```

结果为 `helper-rc=0`、`frames_good=128`、`frames_error=0`、五次 VCM ioctl
均为 `ioctl_rc=0`，并且每个位置均保存了三个连续 RAW10 帧：

```text
position  first_frame  last_frame  settle_us
0         22           24          150000
256       43           45          150000
512       64           66          150000
768       85           87          150000
1023      106          108         150000
```

完整 helper 输出、dmesg 和 15 个 RAW 文件保存于被忽略的本地证据目录：
`artifacts/op3-af-evidence/device-g3-formal-v2-20260913/`。内核日志包含正常的
IMX298 上电、24 MHz MCLK、`0x0298` 芯片 ID、VCM `0x0e` 写事务和 stream
start/stop；本次正式采集没有 `VFE overflow`、SMMU fault、CCI 错误或重启。

用同一逐行 RAW10 解包、同一中央四分之一到四分之三 ROI 计算 Tenengrad，三帧均值为：

```text
position  0          256        512        768        1023
score     110.495    111.452    111.086    110.788    111.607
```

分数变化只有约 1%，而且当前场景均值约 74/1023、动态范围约 17，属于低纹理/偏暗
样本。因此本次已经证明“连续采集期间的五档 VCM 通信和样本收集”通过，但没有
证明镜头产生可观的光学位移或出现清晰度峰值。

早先使用 `--frames 60` 的运行只完成到第三档附近，属于参数不足的工具失败；另一次
在同一启动重复打开媒体链路触发了 VFE overflow，已通过重新 `fastboot boot` 清除
媒体状态，不能把它与本次正式 G3 结果混为一谈。

## 判定

- `BUILD_PASS`: 修正后的静态 helper 构建通过。
- `I2C_PASS`: 初筛中五次合法位置写均 `ioctl_rc=0`，对应 CCI 事务完成。
- `G3_CAPTURE_PASS`: **通过**；正式干净启动中 128/128 帧无错误，五档各有三帧，
  五次位置命令均完成。
- `LENS_MOTION_PASS`: **未通过**；正式样本仍没有足够的清晰度变化，当前场景
  低纹理且偏暗，不能区分镜头未动和目标不适合验焦。
- `AF_PASS`: **NOT_RUN**；闭环自动对焦必须等待 G3 通过。

## 下一实验

下一次设备实验先保持内核、DTS、CCI 和驱动不变，让镜头对准光线充足的高对比文字
或边缘目标，重复同一 G3 采集并比较清晰度峰值。若高对比目标下仍无清晰度变化，再进入
`OP3-AF-G2`，一次只处理一个有来源的原因：核对原厂 AF 初始化/servo 前置序列、
EEPROM infinity/macro 校准是否必须，以及 VAF/AF_PWDM 的实际电气状态。当前没有
足够来源安全猜写新的 VCM 寄存器；不得整包移植 OIS 固件或向 EEPROM 写入校准。
