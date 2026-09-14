# OP3 自动对焦 G4：EEPROM 只读标定采集

阶段：`OP3-AF-G4`  
日期：2026-09-14  
状态：**TOOL-READY / DEVICE-NOT-RUN / NO-WRITE**

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
`msg too long` 限制（最大读取长度为 4 字节）；这确认总线可访问，但工具需要分块。
工具已修正为 4 字节分块读取，等待重新测试。没有刷机、没有改 DTS、没有写 EEPROM，
也没有重新编译内核或 Buildroot。下一 PASS 条件是：在 CCI0/地址 `0x50` 上完成
所有分块只读事务，保存完整原始字节和 dmesg，并确认没有 CCI 错误、相机重启或异常温升。
