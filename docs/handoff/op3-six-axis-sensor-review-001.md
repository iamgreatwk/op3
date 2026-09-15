# OP3 六轴传感器缺失：独立复盘交接文档

日期：2026-09-15  
用途：交给其它模型进行证据审查，检查是否遗漏了可验证的路径。  
范围：OnePlus 3（MSM8996）Linux 6.12.1 recovery 中，SLPI/Sensor Manager 已工作但加速度计和陀螺仪没有出现。

本文是诊断交接，不是“已修复”结论。除非获得新的硬件、固件或下游源码证据，不应继续盲改 DTS、传感器映射或 Buildroot。

## 1. 当前结论

目前最可靠的结论是：

1. SLPI 已启动，QRTR/IPCRTR 传输已工作，sns-reg 已加载，Sensor Manager 已创建 IIO 设备。
2. SLPI 当前只向 Linux 公布两个传感器：磁力计 MAG (0x14) 和接近/光线 PROX_LIGHT (0x28)。
3. 加速度计和陀螺仪的 registry group 请求可以成功返回，但这不能证明对应传感器已经被 SLPI 探测并加入 inventory。
4. 因此故障点目前更可能在“SLPI 传感器核心对六轴器件的探测、硬件总线、电源/复位/中断或固件配置”，而不是 Linux IIO 客户端、QRTR 传输或 registry 文件名。
5. 早期曾看到一次陀螺仪 200 Hz 的临时结果，但在断电后和后续干净启动中未复现；该结果只能作为未确认线索，不能作为当前基线。

当前状态：诊断中，未接受，未集成到最终 Buildroot。

## 2. 期望与实际结果

下游/厂商资料显示 OP3 可能使用以下器件组合，但型号仍应以本机硬件证据为准：

| 功能 | 可能的器件 | 当前结果 |
| --- | --- | --- |
| 加速度计 | LSM6DS3 或同类六轴 IMU 的 accel 部分 | 未出现在 SLPI inventory，也没有 IIO 设备 |
| 陀螺仪 | LSM6DS3 或同类六轴 IMU 的 gyro 部分 | 未出现在 SLPI inventory，也没有 IIO 设备 |
| 磁力计 | MMC3416PJ 或同类器件 | MAG (0x14)，IIO 已出现 |
| 接近/光线 | APDS9921/APDS-9922 或同类器件 | PROX_LIGHT (0x28)，IIO 已出现 |

“可能的器件”不是已确认硬件型号；不要仅凭这些名称新增 I2C 节点或修改地址。

## 3. 数据路径与已定位的断点

~~~
物理传感器
    │  由传感器硬件、总线、电源、复位和中断决定
    ▼
SLPI 传感器核心 / SSC
    │  负责探测器件并生成 sensor inventory
    ▼
slpi.mbn + sns.reg / sns-reg
    │
    ▼
QRTR / IPCRTR
    │
    ▼
Linux Qualcomm Sensor Manager (qcom_smgr)
    │
    ▼
IIO 设备与通道
    │
    ▼
recovery / userspace
~~~

目前确认到 qcom_smgr 的 inventory 只有两个条目，因此六轴传感器缺失发生在或早于“SLPI inventory 生成”这一层。Linux 侧不能凭空从一个没有公布的 inventory 条目创建真实传感器。

## 4. 已完成的实现与证据

### 4.1 Sensor Manager 内核线

独立内核 worktree：

~~~
source/linux-pmos-msm8996-6.12-sensor-smgr
~~~

分支：agent/implementation/op3-sensor-smgr-001  
最新已知提交：8b9430d14e2f  
该 worktree 当时为 clean；本交接任务不应修改它。

核心提交序列：

~~~
b853c4b962fa iio: Add Qualcomm Sensor Manager driver
e17f973f5fa8 soc: qcom: Add in-kernel sensors registry implementation
d19b3b6bddaa remoteproc: qcom: Enable in-kernel sns-reg
8c1093c17676 net: qrtr: Turn QRTR into a bus
a2af2e73df40 net: qrtr: Define macro to convert QMI version and instance to QRTR instance
603c36de0534 modpost: keep QRTR alias in current devtable API
f6d4b4706f83 iio: qcom: Adapt Sensor Manager to Linux 6.12 APIs
76822cd7b4ae net: qrtr: connect SMD service device callbacks
b93eacd248c3 net: qrtr: use token namespace for Linux 6.12 exports
23b5bfc59973 iio: qcom: Select kfifo buffer for Sensor Manager
9eb796adcee8 iio: qcom: log Sensor Manager inventory
8b9430d14e2f soc: qcom: log sensor registry group requests
~~~

### 4.2 registry 输入

需要的外部文件：

~~~
$OP3_EXTERNAL_INPUTS/sensors/sns.reg
~~~

目标暂存路径：

~~~
lib/firmware/qcom/sensors/sns.reg
~~~

manifest：manifests/op3-sensor-smgr.env  
锁定 SHA256：

~~~
2644c56bce535a7c8930e497d2f36601b302573a358493fad2732d1109518f06
~~~

暂存脚本：scripts/stage-op3-sensor-registry.sh

### 4.3 clean inventory 结果

inventory 诊断版本：

~~~
内核提交：9eb796adcee8
输出目录：out/pmos-msm8996-6.12-sensor-smgr-inventory
Image.gz：ac99cae3f8fafe562c3785a550fd44b64916d64720adc6afbb090c34848ebba9
DTB：acf85fd6ae148861374ec4d65feee0e3d909cce9b75e96d09c2f44a102914d1b
测试 boot 镜像：6864133e7f7b99fc1eb4b027ce469f3a7a79ebdcd965e0326b5b02824605e215
~~~

2026-09-15 两次干净启动都只得到：

~~~
qcom_smgr: available sensor count=2
sensor[0]: id=0x14 type=MAG parsed=3
sensor[1]: id=0x28 type=PROX_LIGHT parsed=4
~~~

对应 IIO：

~~~
iio:device0 qcom-smgr-mag
iio:device1 qcom-smgr-prox-light
~~~

给同一份 registry 增加 byte-identical 的板级名字 sns.reg-oneplus,oneplus3 后，board lookup 成功，但 inventory 仍然只有两个条目。因此“文件名/通用 fallback”已基本排除。

### 4.4 registry group audit

审计提交：8b9430d14e2f  
对应补丁：patches/pmos612-op3-sensor-smgr/0012-soc-qcom-log-sensor-registry-group-requests.patch  
补丁 SHA256：

~~~
2324e8ae58f0913a420bc22854d837e03588b25a79eac1e2a5c6a483d1e94530
~~~

测试结果：

~~~
registry group requests: 70
bad=0
每个请求 result=0
返回 data length 符合预期
send_ret=0
~~~

重点请求：

~~~
group id=2692 (DEVINFO ACCEL): result=0 data_len=256 send_ret=0
group id=2693 (DEVINFO GYRO):  result=0 data_len=256 send_ret=0
~~~

这只证明 registry group 读取路径和传输请求成功；它没有证明 ACCEL/GYRO 已被探测、已被 SLPI inventory 公布。`2900` 和 `2910` 不是 ACCEL/GYRO：它们分别返回 item `2800`（`basic ges`）和 item `2900`（`Facing`）的 4 字节 SAM 配置值。现有 inventory 日志也是 QMI 解码后的结果，不是线上的原始 inventory 报文。

### 4.5 firmware / HLOS A/B

本地所有已检查的 slpi.mbn 副本 byte-identical，SHA256：

~~~
5398c39071c4154cce71195848a522729ac1ff1c58f1e434bf2f821d62c2d902
~~~

旧传感器镜像重新启动后同样只出现 MAG/PROX_LIGHT；早期陀螺仪结果没有被复现。

对 HLOS I2C-0/1/2 以及 I2C-3/4 的少量候选地址进行只读 WHO_AM_I 读取，LSM6DS3/BMI160 候选均返回 -ENXIO。探测没有改变现有客户端；现有设备仍为：

~~~
I2C-3: s1302-capkey
I2C-4: rmi4-i2c / Synaptics touch
~~~

这不能证明六轴器件不存在，因为它很可能只由 SLPI 访问；它只说明当前 Linux HLOS 没有直接读到候选器件。

### 4.6 userspace sns-reg A/B

曾做过 kernel CONFIG_QCOM_SNS_REG 与 userspace sns-reg 的 A/B。使用 pmOS userspace registry 及由真实 sns.reg 生成的 numeric registry 后，结果仍为：

~~~
available sensor count=2
只有 MAG 和 PROX_LIGHT
~~~

因此“仅仅是 kernel sns-reg 与 userspace sns-reg 选错”已被 A/B 结果否定为主因。

### 4.7 当前 live 运行链路

最新只读检查得到：

~~~
/sys/class/remoteproc/remoteproc0/state = running
/sys/class/remoteproc/remoteproc0/firmware = qcom/msm8996/oneplus3/slpi.mbn
/sys/class/remoteproc/remoteproc1/state = running
/sys/class/remoteproc/remoteproc1/firmware = qcom/msm8996/oneplus3/adsp.mbn

qcom_smgr: available sensor count=2
sensor[0]: id=0x14 type=MAG parsed=3
sensor[1]: id=0x28 type=PROX_LIGHT parsed=4

/usr/bin/sns-reg 正在运行
iio:device0 qcom-smgr-mag
iio:device1 qcom-smgr-prox-light
~~~

这确认了 SLPI firmware -> QRTR/IPCRTR -> sns-reg -> Sensor Manager -> IIO 主链路，而不是一个完全没有启动的传感器服务。

## 5. 已排除或暂不应重复尝试的方向

以下结论是“当前证据下不应再作为第一猜测”，不是永远不可能：

- 不是因为 qcom_smgr、QRTR 或 IPCRTR 完全没有工作。
- 不是因为 SLPI 没有启动；remoteproc 状态为 running。
- 不是因为只缺少 sns.reg-oneplus,oneplus3 这类文件名 fallback；加上 byte-identical board 文件后 inventory 不变。
- 不是因为 registry group 请求整体失败；ACCEL/GYRO group audit 全部成功。
- 不是因为 userspace/kernel sns-reg 二选一导致当前两个设备消失；A/B 结果一致。
- 不能把当前两个 IIO 设备误判为六轴传感器已经注册；当前解码后的 inventory 根本没有 ACCEL/GYRO，尚未取得 wire-level 原始 inventory 报文。
- 不要凭 LSM6DS3、BMI160 或网络资料猜测新增 HLOS I2C 节点、地址、GPIO 或中断。
- 不要再次加入此前会导致启动重启的 lvs1 {} 电源节点；六轴传感器由 SLPI 访问时，盲改 HLOS regulator 不能证明正确性。
- 不要因为一次不可复现的 gyro 200 Hz 输出就修改 Linux 映射或采样率。
- 不要为这个诊断问题重编译 Buildroot、切换 DRM、修改 recovery UI 或刷新策略；这些不改变 SLPI inventory。
- 不要在运行中的 recovery 上反复 rmmod 传感器/QRTR/remoteproc 相关模块；此前摄像头和显示路径已有卸载导致崩溃的历史，传感器实验应使用一次性全新启动。

## 6. 仍需其它模型重点审查的遗漏项

请按以下优先级审查，先找证据，再提出代码改动。

### A. 确认本机实际六轴器件及物理连接

不要把“可能是 LSM6DS3”当成事实。应从 OnePlus 3 原理图、官方/Lineage 下游 DTS、下游 sensor 驱动和分区备份共同确认：

- 实际芯片型号和 WHO_AM_I 值；
- 由 AP 还是 SLPI 访问；
- 物理总线（I2C、SPI、SLIMbus 或 SSC 专用路径）；
- 7-bit 地址；
- 电源 rail、复位脚、中断脚和上拉；
- 是否有板级 variant 或不同批次器件。

如果硬件资料确认它只挂在 SLPI 专用总线上，就不应继续做 HLOS I2C 探测。

### B. 成组检查 SLPI 固件和配置，而不是只看单个 slpi.mbn

需要审计固件包的整体一致性：

- slpi.mbn 的来源、版本、时间、hash；
- 同目录或同一 vendor 固件包中的传感器配置、registry 和相关 blob；
- 是否还有 SSC/sns 配置文件没有被 staging；
- recovery 所用固件是否确实来自本机对应的 MSM8996/OP3 版本。

当前本地 slpi.mbn 副本相同，只能说明这些副本彼此相同，不能证明它们就是本机原厂运行过的完整固件组合。

### C. 审计保留分区中的 Android sensor 配置

在只读副本中查找并相互比对：

~~~
persist.img.gz
vendor/etc/sensors/
system/etc/sensors/
sensor_def_qcomdev.conf
hals.conf
sns.reg 或同类 registry
~~~

重点区分：

- Android HAL 配置可能只决定用户空间命名、校准或启用策略；
- sns.reg/SSC 配置才可能影响 SLPI 的 registry 或传感器发现；
- 找到一个文本配置文件，不代表将它复制进 recovery 就能创建 ACCEL/GYRO。

已知本机相关备份根目录：

~~~
/home/kai/下载/WorkBuddy-20260826/WorkBuddy/2026-08-04-10-03-12/agent-os/backups/partitions_20260818
~~~

审计时必须记录实际文件来源和 SHA256，不能只引用文件名。

### D. 获取 SLPI 侧的探测失败证据

Linux 当前只看到 inventory 数量和结果，未看到 SLPI 内部对六轴器件的 probe log。应检查：

- SLPI/SSC 是否有可读取的 trace、ramdump、service log 或 debug endpoint；
- firmware load 后是否报告 I2C timeout、WHO_AM_I mismatch、power/IRQ 错误；
- SLPI 是否需要特定启动顺序或等待时间；
- sns-reg 请求是否在 SLPI 完成 sensor probe 前过早发送。

如果要加 Linux 诊断，只允许在当前 transport response 边界增加只读日志，记录 wire-level response 的长度、sensor count 和 type/id；不要同时改 registry、映射和 DTS。

### E. 重新核对 wire-level sensor type/id 与 Linux 映射

只有在 wire-level inventory 中出现疑似 ACCEL/GYRO 条目时，才检查 Linux 的 type/id 映射和 parser。当前解码后的 inventory 没有这两个条目，且尚未取得 wire-level 原始 inventory 报文，因此“映射错误”不是首要假设。

同样，group id=2900/2910 不是 ACCEL/GYRO：`2900` 返回 item `2800`（`basic ges`），`2910` 返回 item `2900`（`Facing`），两者都是 SAM 配置 group，而不是“设备存在”标志。

### F. 检查是否缺少可选的 SLPI 服务依赖

审查 sns-reg、QRTR endpoint（当前为 9-17）、remoteproc 固件加载路径和请求时序是否与下游启动流程一致。注意：因为 MAG/PROX_LIGHT 已经出现，这一项更像是特定六轴 probe 的依赖，不像完整传输缺失。

## 7. 推荐下一步实验

### 实验 1：固件/配置身份审计（优先）

假设：当前 slpi.mbn 或配套 sensor 配置不是这台 OP3 所需的完整组合。  
唯一变量：只替换或只改变 SLPI 固件/配置身份。  
保持不变：内核、DTB、initramfs、recovery、registry staging 和启动参数。  
PASS：inventory 出现 ACCEL 与 GYRO。  
FAIL：仍只有 MAG/PROX_LIGHT，则转向硬件/SLPI probe 路径。

该实验必须先有可靠的新固件或原厂分区证据；没有新输入时不要盲换文件。

### 实验 2：SLPI response 原始 inventory 诊断

假设：SLPI 实际返回了更多条目，但 Linux parser 或消息边界只解析到两个。  
唯一变量：仅增加 response count/长度/type/id 的只读日志，不改变 parser 结果。  
PASS：wire-level response 含 ACCEL/GYRO，而解析结果丢失，才进入 parser 修复。
FAIL：wire-level response 本身只有两个，则 parser 方向停止。

### 实验 3：物理硬件证据后的 SLPI 电源/总线验证

假设：六轴器件因 SLPI 所需电源、复位、IRQ 或总线配置不正确而未被探测。  
唯一变量：依据确认过的下游/原理图证据，只修改一个电源、GPIO、IRQ 或时序字段。  
PASS：inventory 新增 ACCEL/GYRO 且能持续读数。  
FAIL：仍只有两个设备，恢复原值并转向 firmware/硬件 variant。

没有物理连接证据时，不执行该实验。

### 实验 4：若没有新证据则暂停六轴移植

如果无法获得本机硬件型号、原厂配套 SLPI 固件或 SLPI probe 日志，当前证据已经足以说明继续盲猜的收益很低。此时应把六轴任务标为“缺少外部证据”，先移植其它硬件，避免破坏已工作的 recovery。

## 8. 任何未来测试都必须收集的最小证据

每次只做一次全新启动，记录：

~~~sh
uname -a
cat /proc/cmdline
for r in /sys/class/remoteproc/remoteproc*; do
    echo "--- $r ---"
    cat "$r/state" "$r/firmware" 2>/dev/null
done
dmesg | grep -iE 'remoteproc|slpi|sns|smgr|qrtr|ipcrtr|sensor|iio'
find /sys/bus/iio/devices -maxdepth 1 -type l -printf '%f -> %l\n' 2>/dev/null
for d in /sys/bus/iio/devices/iio:device*; do
    [ -d "$d" ] || continue
    echo "--- $d ---"
    cat "$d/name" 2>/dev/null
    ls "$d" 2>/dev/null | grep -E 'in_(accel|anglvel|mag|proximity|illuminance)' || true
done
pidof sns-reg qcom_smgr 2>/dev/null
~~~

同时记录：

- top-level 项目 commit；
- nested kernel worktree commit；
- .config、Image.gz、DTB、initramfs 和外部 registry 的 SHA256；
- 使用的 slpi.mbn 及所有传感器配置的 SHA256；
- 启动次数、是否为 fresh boot、是否发生过异常重启；
- 完整 /tmp/fb.log、recovery log 和 dmesg。

不要把 SSH 可连接、/dev/iio 目录存在或两个 IIO 设备存在当成六轴成功标准。

## 9. 交给其它模型的审核问题

请审核者只根据证据回答：

1. 上述结论中哪些是事实、哪些仍是推断？
2. 是否还有一个未检查的固件、分区备份、下游源码或硬件连接路径，能解释“group 请求成功但 inventory 只有两个”？
3. 是否有证据表明 OP3 六轴器件由 HLOS 直接访问，而不是 SLPI 访问？
4. 在不混合 kernel/DTS/userspace 层的前提下，下一次最小实验的唯一变量是什么？
5. 该实验的明确 PASS/FAIL 日志是什么？
6. 如果没有新的固件或硬件证据，是否应该暂停六轴移植并记录为 blocked？
7. 请指出任何会导致启动重启、改变现有音频/按键/网络功能或污染正式 recovery 基线的危险操作。

审核者不得把猜测的传感器型号、I2C 地址、电源 rail 或 registry group 语义写成已确认事实；每一项都必须给出来源、命令和可复现的 PASS/FAIL 条件。

## 10. 相关文件

~~~
docs/handoff/op3-sensor-smgr-001.md
docs/handoff/latest.md
docs/test-matrix.md
manifests/op3-sensor-smgr.env
patches/pmos612-op3-sensor-smgr/README.md
scripts/stage-op3-sensor-registry.sh
~~~

传感器独立内核 worktree：

~~~
source/linux-pmos-msm8996-6.12-sensor-smgr
~~~

请注意：顶层项目仓库和 nested kernel 仓库是两个独立 Git 仓库；顶层分支名不能选择或修改 nested kernel 分支。
