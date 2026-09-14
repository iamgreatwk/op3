# OP3 自动对焦 G4：EEPROM 只读标定采集

阶段：`OP3-AF-G4`  
日期：2026-09-14  
状态：**READ-PASS / G1-PASS / CAPTURE-PASS / OPTICAL-FOCUS-INCONCLUSIVE / NO-WRITE**

## 交接

Task / GitHub Issue: `OP3-AF-G4`  
Role: Implementation  
Baseline commit: `004cda8e061387d07ab4dd08c474910eedd3eb24`（相机内核）  
Working branch: `agent/implementation/recovery-browser-001`（工具）  
Changed files: `scripts/op3-i2c-eeprom-read.c`  
Commit SHA: 本文提交所在 commit 见 Git 历史

Layer: 用户态 I2C 只读诊断  
Hypothesis tested: 原厂后摄 EEPROM 含有可用于 VCM macro/infinity 映射的原始字段  
Only variable changed: 读取 EEPROM 的固定地址/偏移；不改变内核、DTS、VCM、曝光、增益或采集模式

## 来源约束

LineageOS 的 OP3 15801 相机 DTS 将 Sony IMX298 EEPROM 写地址列为 `0xa0`，对应
Linux 7-bit 地址 `0x50`，挂在 CCI master 0，原始读取从偏移 `0x0000` 开始：
[LineageOS 15801 camera DTS](https://raw.githubusercontent.com/LineageOS/android_kernel_oneplus_msm8996/b2a6b3b561067a4de54a0d60524659cb99bdcd3b/arch/arm/boot/dts/qcom/15801/msm8996-camera-sensor-mtp_15801.dtsi)。

离线原厂 EEPROM 库的静态反汇编还给出两个候选 little-endian 16-bit 字段
`raw+0x24` 和 `raw+0x26`，与 `orig macro`/`infinity` 字符串相关。它们目前只是
候选映射，必须由本机实际 EEPROM 数据和后续图像证据确认，不能直接当作最终 VCM
范围。

## 工具行为

`scripts/op3-i2c-eeprom-read.c` 只接受调用者指定的 `/dev/i2c-X`，固定访问地址
`0x50`，默认读取 `0x0000..0x002f`。由于 OP3 MSM8996 CCI 适配器限制单次读取最多
4 字节，工具将读取拆成连续的 4 字节 `I2C_RDWR` 事务。每次事务发送两字节 EEPROM
地址指针后立即读取数据；不发送 EEPROM 数据写入命令，不扫描其它地址。输出原始十六进制，
并在数据覆盖对应范围时打印 `0x24`/`0x26` 候选字段。

主机编译命令：

```sh
aarch64-linux-gnu-gcc-11 -static -O2 -Wall -Wextra -Werror \
  scripts/op3-i2c-eeprom-read.c \
  -o /tmp/op3-i2c-eeprom-read.aarch64
```

设备端运行前先只确认适配器名称，不执行地址扫描：

```sh
for d in /sys/class/i2c-adapter/i2c-*; do
    [ -e "$d/name" ] || continue
    printf '%s: ' "${d##*/}"
    cat "$d/name"
done
ls -l /dev/i2c-*
```

然后对 CCI0 对应的适配器执行：

```sh
/newroot/tmp/op3-i2c-eeprom-read.aarch64 /dev/i2c-X 0x0000 0x30 \
  | tee /tmp/op3-imx298-eeprom-20260914.txt
```

其中 `X` 必须由上一步的适配器名称确认，不能凭 `/dev/i2c-0` 猜测。

## 当前结果

设备已确认 CCI0 为 `/dev/i2c-5`，并已加载此前验证的 CCI、CAMSS、IMX298 和
BU63165GWL 模块。首次 48 字节单事务读取被内核拒绝，dmesg 报告 CCI 适配器的
`msg too long` 限制（最大读取长度为 4 字节）；这确认总线可访问，但工具随后改为分块。
工具已修正为 4 字节分块读取，并在相机保持连续采集、传感器电源和时钟开启时读取成功，
结果保存于 `artifacts/op3-af-evidence/device-g4-eeprom-20260914/op3-imx298-eeprom-20260914.txt`。
原始数据中候选字段为 `raw+0x24=0x0262=610`、`raw+0x26=0x013c=316`；同一轮相机
采集完成 `128/128` 帧且 `G1 PASS`。

注意：在传感器断电后执行首次失败读取时，CCI 进入 queue timeout；随后未重启就开始的
扫焦得到 0 帧并出现 VFE overflow。因此后续校准区间扫焦必须从干净启动开始，不能通过
`rmmod qcom_camss` 恢复状态。没有刷机、没有改 DTS、没有写 EEPROM，也没有重新编译内核
或 Buildroot。

## 干净启动后的校准区间扫焦

2026-09-14 在同一枚临时 AF_PWDM boot 镜像上重新执行了干净启动，保持内核、DTS、
CCI、IMX298、BU63165GWL、曝光 `893`、增益 `240` 和采集模式不变，只把 VCM 位置序列
设为 EEPROM 两个候选值包围的区间：

```text
316, 360, 400, 440, 480, 520, 560, 610
```

每个位置等待 150 ms 并保存 3 帧，整轮保持一次连续 `STREAMON`。结果为：

- `helper-rc=0`
- `frames_good=200`, `frames_error=0`
- 8 个位置的 `VIDIOC_S_CTRL` 均返回 `ioctl_rc=0`
- 内核记录 8 次 CCI master 0、地址 `0x0e` 的 VCM 位置事务
- 没有新的 CCI queue timeout、NACK、VFE overflow、SMMU fault 或重启

RAW10 帧已保存到
`artifacts/op3-af-evidence/device-g4-calibrated-sweep-20260914/`。中心/全幅
Tenengrad 结果如下（每个位置取 3 帧平均）：

```text
position       316    360    400    440    480    520    560    610
Tenengrad    50.655 50.938 50.648 50.624 50.666 50.697 50.717 50.532
```

曲线没有超过当前场景噪声的可重复峰值；因此本轮只能判定“校准区间采集和 VCM 控件
通信通过”，不能判定镜头已经移动或自动对焦成功。直接在传感器停止出帧后打开
`bu63165gwl` lens subdev 的独立测试还会在第一条位置写上返回 `-ETIMEDOUT`，进一步
确认 VCM 必须与活动的 IMX298 电源/连续采集会话一起测试。下一步应继续保持该干净
连续流路径，扩大到有明确纹理的远近目标并验证实际位置响应；在此之前不改 VCM 协议、
不猜写初始化寄存器、不移植 OIS 固件，也不固化到 Buildroot。
