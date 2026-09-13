# OP3 自动对焦 G2：原厂资料约束审计

阶段：`OP3-AF-G2- vendor-constraints`  
日期：2026-09-13  
状态：**STATIC-EVIDENCE / NO-SAFE-CODE-CHANGE**

## 交接

Task / GitHub Issue: `OP3-AF-G2` / [Issue #12](https://github.com/iamgreatwk/op3/issues/12)  
Role: Implementation  
Baseline commit: `67b0bbc3cbf46bae712a2606a43361756fcbd829`  
Working branch: `agent/implementation/recovery-browser-001`  
Changed files: 仅新增本审计文件  
Commit SHA: 本文提交所在 commit 见 Git 历史

Layer: 原厂 AF/VCM 静态资料与 Linux 下游实现核对  
Hypothesis tested: G3 无清晰度变化是因为当前驱动遗漏了有来源的 AF 前置初始化或校准  
Only variable changed: 只补充证据，不改驱动、DTS、CCI、Buildroot 或手机状态

## 已确认

离线备份中的原厂资料：

- `camera_config.xml` 将后摄定义为 `imx298` + actuator
  `rohm_bu63165gwl` + OIS `rohm_bu63165gwl` + EEPROM `sony_imx298`；
- 同一配置给出 `TotalFocusDistance=1.9`、`MinFocusDistance=0.1`，但没有把物理
  距离映射为 VCM code；
- `libactuator_rohm_bu63165gwl.so` 是 stripped 32 位 ARM ELF。其 `.data` 中有
  8-bit 地址 `0x1c`，以及四个动态位置寄存器地址 `0xf0`、`0xf1`、`0xf2`、
  `0xf3`。结合下游 `i2c_addr >> 1`，Linux 7-bit 地址为 `0x0e`；
- actuator 库没有可读的独立初始化表、status/ready 寄存器、park code 或固件
  blob。位置数据是在运行时填入，不能从静态零值表猜出初始化序列；
- `sensor_modules.so` 包含通用的 `actuator_init_calibrate`、`lens_move` 和
  EEPROM AF calibration 调用路径；`imx298-eeprom.so` 包含
  `eeprom_autofocus_calibration` 以及 `orig macro`/`infinity` 字符串。这证明
  原厂栈支持校准，但不证明 BU63165GWL 必须先写哪组寄存器；
- persist 备份中没有可直接识别的 camera AF 校准文件。校准数据可能来自传感器
  EEPROM 或运行时原厂接口，当前没有可复现的字节表和校验协议。

同型号 Lineage 下游 `msm_actuator.c` 的 OP3 特殊路径与当前驱动一致：从
`0xf0` 开始顺序写 `0x90, 0x00, position_hi, position_lo`；Linux CCI 事务因此
表现为 5 个数据字节。当前设备 G1/G3 初筛也观察到地址 `0x0e`、长度 5 且返回
成功。没有证据支持修改地址、端序或 payload 宽度。

## 不能安全移植的内容

Lineage 的 `msm_actuator_init_focus()` 能执行一个由用户态提供的初始化表，但在
本机原厂资料中没有对应的 OP3 BU63165GWL 表。通用 OIS 库也不能当作 AF 必需
初始化的证明；整包移植 OIS 固件会同时改变变量并可能造成启动/总线风险。

因此本 G2 审计没有产生内核代码变更，也没有制造一个猜测的“init sequence”。
当前驱动已经有可追溯的 VAF 2.8 V、AF_PWDM、高电平和 2--3 ms 上电等待；下一
个代码变更只有在获得实际寄存器表、可靠 status/ACK 协议或电气测量后才允许。

## 判定

- 地址、事务布局：`SUPPORTED`，来源可追溯；
- 原厂存在 AF 校准调用：`SUPPORTED`，但校准字节/协议未知；
- 缺失初始化/servo 是唯一根因：`INCONCLUSIVE`；
- 可提交的安全初始化代码：`NOT AVAILABLE`；
- G2 不应通过猜测寄存器写入来“验收”。

## 下一实验

先完成严格 G3 复测（150 ms 稳定等待、每位置三帧、固定高对比目标）。若仍无
可重复的清晰度变化，必须获取新的证据：

1. 示波器/逻辑分析仪测 CCI `SCL/SDA`、VAF 2.8 V、AF_PWDM GPIO39 和传感器
   MCLK/RESET 的实际波形；或
2. 在原厂 Android 环境中记录 actuator/OIS 初始化及第一次 lens move 的完整
   CCI 事务；或
3. 解出本机 EEPROM AF 数据的读取地址、范围、校验和 infinity/macro 映射。

在此之前不改 `bu63165gwl.c` 的位置协议，不扫描未知地址，不向 OIS/EEPROM 写入
固件或校准，不进入 G4。
