# OP3 BU63165GWL / IMX298 自动对焦协议审计

日期：2026-09-13  
阶段：`OP3-AF-G0`  
结果：**INCONCLUSIVE（静态证据已完成，硬件协议仍缺少若干关键项）**

本记录只做主机端源码和离线 vendor 镜像审计，没有修改内核、没有编译、
没有操作手机。它是 `docs/op3-autofocus-luna-plan.md` 中 G0 的执行结果，
下一阶段应是 G1 的“保持 IMX298 连续出帧时发送同一条 VCM 命令”对照。

## 固定输入

- 项目仓库分支：`agent/implementation/recovery-browser-001`
- 相机内核仓库：`source/linux-pmos-msm8996-6.12-camera-imx298`
- 相机内核分支：`agent/implementation/op3-camera-imx298-001`
- 审计时内核 HEAD：`004cda8e061387d07ab4dd08c474910eedd3eb24`
- Linux 正式基线仍由 `BASELINE.env` 锁定为 pmOS MSM8996 Linux 6.12.1
- 同机型下游参考：
  [LineageOS commit b2a6b3b5](https://github.com/LineageOS/android_kernel_oneplus_msm8996/tree/b2a6b3b561067a4de54a0d60524659cb99bdcd3b)
- 离线镜像：`/home/kai/下载/WorkBuddy-20260826/WorkBuddy/2026-08-04-10-03-12/agent-os/backups/partitions_20260818/android/vendor.img`

从 vendor 镜像只读提取了两个相关库到被 `.gitignore` 忽略的
`artifacts/op3-af-evidence/`，没有提交专有库或整个 vendor 镜像：

| 文件 | 大小 | SHA256 |
| --- | ---: | --- |
| `/lib/libactuator_rohm_bu63165gwl.so` | 23044 | `1ef67290abb3b017a8ab688dcf15c0a39f64bb836f7c72a574452f4d5fe0adf9` |
| `/lib/libois_rohm_bu63165gwl.so` | 315408 | `7afafc91554b3ffeb676740eabbcae00b79d7ea4f3cc29fdeb4760d84f5dc52f` |

两个库都是 Android 28、32 位 ARM、EABI5、stripped ELF。动态符号表只保留
`actuator_driver_open_lib` 或 `ois_driver_open_lib`，没有导出寄存器/位置/状态
函数；依赖也只是 Android libc/libc++/libcutils/libm/libdl。对库执行字符串和
重定位审计没有找到可追溯的 BU63165GWL 寄存器表、状态寄存器名、park 值或
固件文件名。因此，库的存在只能证明原厂 HAL 有对应插件，不能把缺失字段猜成
协议事实。

## 已确认的静态证据

### 地址和位置写入

同机型下游 DTS 的 actuator 和 OIS 都是 `reg = <0x1c>`、`qcom,cci-master = <0>`。
下游运行时把用户态 `i2c_addr` 转成 CCI SID：

```c
cci_client->sid = set_info->actuator_params.i2c_addr >> 1;
```

下游针对 OP3 actuator 的位置写入为：

```c
data[0] = 0x90;
data[1] = 0x00;
data[2] = next_lens_position >> 8;
data[3] = next_lens_position & 0xFF;
msm_actuator_write_sequence(a_ctrl, 0xF0, data, 4);
```

所以当前 Linux 7 位候选地址 `0x0e`（`0x1c >> 1`）以及五字节总线数据
`f0 90 00 position_hi position_lo` 有同机型下游源码支持。它解释了当前 CCI
日志中 `addr=0x0e` 和 `cmd=0x0090f059` 的对应关系，但还没有证明这次手机上的
VCM 已经 ACK；此前实际位置写仍返回 `-ETIMEDOUT`。

参考：

- [同机型相机 DTS](https://github.com/LineageOS/android_kernel_oneplus_msm8996/blob/b2a6b3b561067a4de54a0d60524659cb99bdcd3b/arch/arm/boot/dts/qcom/15801/msm8996-camera-sensor-mtp_15801.dtsi)
- [下游 actuator 写入和 SID 转换](https://github.com/LineageOS/android_kernel_oneplus_msm8996/blob/b2a6b3b561067a4de54a0d60524659cb99bdcd3b/drivers/media/platform/msm/camera_v2/sensor/actuator/msm_actuator.c)
- 当前实现：[bu63165gwl.c](../../source/linux-pmos-msm8996-6.12-camera-imx298/drivers/media/i2c/bu63165gwl.c)

### 供电、使能和时钟

同机型下游相机节点给出以下资源：

| 资源 | 下游证据 | 当前结论 |
| --- | --- | --- |
| VAF | `cam_vaf-supply = <&pm8994_l23>`，2.8 V | VAF 2.8 V 有证据；actuator 节点本身的 VAF 注释掉，OIS/相机节点声明了同一资源，存在共享关系 |
| AF_PWDM | actuator 上电时把有效 GPIO 置高，随后 `usleep_range(2000, 3000)` | 当前 GPIO39 高电平和约 2 ms 延时方向正确 |
| IMX298 VDIG/VIO/VANA/custom1 | 相机节点分别为 1.1 V、0、2.6 V、2.15 V | 属于传感器电源，不等于 VCM 已上电 |
| MCLK | 相机节点 `qcom,clock-rates = <24000000 0>` | IMX298 连续采集需要 24 MHz；没有证据表明 BU63165GWL 自身需要 MCLK |

当前 mainline DTS 使用 `vreg_l23a_2p8` 和 GPIO39；当前 VCM 驱动的顺序是先
enable VAF、再拉高 AF_PWDM、等待 2--2.5 ms。当前 IMX298 驱动在实际
`s_stream(1)` 中才建立 runtime power 会话并打开 24 MHz MCLK，不能用“只打开
sensor subdev”的旧实验替代连续采集会话。

## 必要但尚未被证明的项目

| 项目 | 审计结果 | 原因/处理 |
| --- | --- | --- |
| AF 初始化写表 | `UNKNOWN` | 下游 `msm_actuator_init_focus()` 接受用户态 settings；未在可追溯源码中找到 BU63165GWL 实际 settings，Android actuator 库已 stripped |
| AF servo 启动或初始化延迟 | `UNKNOWN` | 只有 AF_PWDM 后 2--3 ms 的通用上电延迟有证据，不能推断这就是芯片就绪时间 |
| AF 是否需要 OIS 初始化 | `UNKNOWN` | 下游 OIS 代码有独立 settings、`0x8200` 状态轮询和 `.prog/.coeff` 下载路径，但这不证明 AF 位置写必须先执行 OIS 流程 |
| AF 固件是否必须下载 | `UNKNOWN` | `msm_ois_download()` 可加载 `<ois_name>.prog`/`.coeff`，但它属于 OIS 配置路径；两个 stripped 库没有提供 AF 固件依赖证据 |
| AF 安全状态读寄存器 | `UNKNOWN` | 当前 VCM 仅有位置写；`G_CTRL` 返回缓存值，不是硬件读回。OIS 的 `0x8200` 不能直接当作 AF 状态寄存器 |
| 有效位置范围 | `UNKNOWN`（当前驱动候选为 0..1023） | 当前 `0..1023` 是为 V4L2 控件选择的候选范围，尚未由 BU63165GWL 校准表或原厂 HAL 字段证明 |
| infinity/macro 校准 | `UNKNOWN` | vendor EEPROM/actuator 数据尚未解出，不能把 0 或 1023 命名为无穷远/微距 |
| park 位置和安全退电 | `UNKNOWN` | 下游通用 park 算法依赖用户态 tuning；没有本机 BU63165GWL 的 park 参数 |

下游 OIS 实现的位置在
[msm_ois.c](https://github.com/LineageOS/android_kernel_oneplus_msm8996/blob/b2a6b3b561067a4de54a0d60524659cb99bdcd3b/drivers/media/platform/msm/camera_v2/sensor/ois/msm_ois.c)。
它可以作为下一步“是否存在共享初始化”的证据来源，但不允许直接把整套 OIS
固件或写表移植进 AF 驱动，更不允许在未知寄存器上试写。

## G0 结论和下一步

G0 不能判定当前位置事务错误的根因。能够固定下来的实验输入是：

1. 保持 CCI0、Linux 7 位地址 `0x0e`、一字节寄存器地址 `0xf0`、四字节数据和
   高字节在前不变。
2. 保持 VAF 2.8 V、GPIO39 高电平、2--3 ms 上电等待不变。
3. 不加入 `lvs1`、不移植 OIS 固件、不改 CCI queue/EXEC 逻辑。
4. 进入 G1：同一次 `STREAMON` 中先确认至少四帧连续 `DQBUF`，保持 lens fd 和
   sensor stream 活跃，再发送一次不同于当前缓存值的位置命令。

G1 的判定不能把 `VIDIOC_G_CTRL` 成功当作镜头移动。若连续出帧时仍然
`-ETIMEDOUT`，只能说明“活动传感器会话不足以解决超时”，再进入 G2 的单变量
通信/初始化定位；若位置命令成功，仍必须进入 G3，用固定场景的图像清晰度变化
证明镜头确实移动。

