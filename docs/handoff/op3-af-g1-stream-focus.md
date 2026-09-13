# OP3 自动对焦 G1：连续采集期间的一次 VCM 命令

阶段：`OP3-AF-G1`  
状态：**EVIDENCE-PASS / INTEGRATION-REVIEW-PENDING**
日期：2026-09-13

G0 审计已经把当前可用的 VCM 协议固定为 CCI0、Linux 7 位地址 `0x0e`、
`f0 90 00 position_hi position_lo`。G1 只验证一个变量：发送同一条位置命令
时，IMX298 是否正在同一次连续 `STREAMON` 会话中正常出帧。G1 不改内核、DTS、
CCI、模块或 Buildroot。

## 测试程序

源码：`scripts/op3-v4l2-af-stream-test.c`

程序会：

1. 扫描 `/dev/mediaN`，通过 media topology 找到同时包含 `imx298` 和
   `msm_vfe0_rdi0` 的图；
2. 通过 `/sys/class/video4linux/*/name` 找到 IMX298、BU63165GWL 和 VFE RDI，
   不依赖固定的 subdev、video 或 I2C 编号；
3. 协商 1476x834 RAW10，申请并映射 4 个 MMAP buffer；
4. `STREAMON` 后由采集线程持续 `DQBUF/QBUF`，至少收到 4 个无错误帧后才
   唤醒对焦线程；
5. 对焦线程打开 lens fd，读取一次缓存位置以保证请求值不同，然后只发送一次
   `V4L2_CID_FOCUS_ABSOLUTE`；lens fd 保持到采集结束；
6. 每帧保存为 `<prefix>-NNNN.raw`，记录 sequence、timestamp、buffer、
   bytesused、flags、data_offset；结束时按 `STREAMOFF`、解除映射、关闭 lens
   和 video 的顺序清理。

默认参数是 30 帧、曝光 893、增益 240、首个位置 640；首个位置若等于驱动缓存
值会自动改为相邻位置。默认输出前缀是 `/tmp/op3-af-g1`。这里的
`VIDIOC_G_CTRL` 只用来避开相同值，仍然是缓存值，不能证明镜头已经移动。

## 主机构建

在项目根目录执行：

```sh
project=/home/kai/src/oneplus3-mainline
src=$project/scripts/op3-v4l2-af-stream-test.c
mkdir -p "$project/artifacts/op3-af-evidence"

aarch64-linux-gnu-gcc-11 -std=gnu11 -O2 -static -Wall -Wextra -Werror -pthread \
  "$src" -o "$project/artifacts/op3-af-evidence/op3-v4l2-af-stream-test.aarch64"

file "$project/artifacts/op3-af-evidence/op3-v4l2-af-stream-test.aarch64"
sha256sum "$project/artifacts/op3-af-evidence/op3-v4l2-af-stream-test.aarch64"
```

本次主机编译得到的 SHA256 是：

```text
eb825ce009e5168dc599ce11d5e158fe2d393415c023a92d6a7c38db71792157  op3-v4l2-af-stream-test.aarch64
```

主机端 `gcc -Wall -Wextra -Werror` 自检通过；没有 camera media 节点的主机运行
会按预期返回 `ENODEV`，这不是手机测试结果。

本次 helper 修正已提交为 `4837c51`：修复 topology ready 位被覆盖、只配置
实际选中的五个 pipeline entity，并保持旧的已知成功顺序，先打开 VFE video
节点再建立 media links。当前 CCI 模块由相机分支
`004cda8e061387d07ab4dd08c474910eedd3eb24` 单独构建，wrapper 是
`scripts/op3-cci-module.mk`（项目提交 `26b2c41`），模块 SHA256 为：

```text
1691a3a35b6e24145e705074a92b7641525e2909372bb25a65ad621049107d25  i2c-qcom-cci-g1-20260913.ko
```

本次只使用该 CCI 模块和已验证的 CAMSS/V4L2 依赖；没有加载旧实验中的
`i2c-qcom-cci.ko`。临时 VCM/传感器模块分别为
`bu63165gwl.ko` SHA256 `cd638af89815c4967ba5e718dc4c4f20130a972da9ba3736bfe16b114fa0b063`
和 `imx298.ko` SHA256
`103fe40cad22972ed3408b76f997dc8a06e1c09a4b2dba03e8dc71874144e8f9`。

## 手机测试

本阶段不刷机。手机必须先启动已经验证的 AF_PWDM 临时 boot 镜像，并加载同一组
已验证的 CCI/CAMSS/V4L2/IMX298/BU63165GWL 模块。程序和日志使用版本化文件名，
避免覆盖旧实验：

```sh
project=/home/kai/src/oneplus3-mainline
helper=$project/artifacts/op3-af-evidence/op3-v4l2-af-stream-test.aarch64

scp -O "$helper" root@172.16.42.1:/newroot/tmp/op3-v4l2-af-g1-20260913
scp -O "$project/artifacts/op3-af-evidence/i2c-qcom-cci-g1-20260913.ko" \
  root@172.16.42.1:/newroot/tmp/i2c-qcom-cci-g1-20260913.ko
scp -O "$project/out/pmos-msm8996-6.12-camera-imx298-cci100/vcm-af-pwdm-module/bu63165gwl.ko" \
  root@172.16.42.1:/newroot/tmp/bu63165gwl-g1-20260913.ko
scp -O "$project/out/pmos-msm8996-6.12-camera-imx298-cci100/imx298-af-pwdm-module/imx298.ko" \
  root@172.16.42.1:/newroot/tmp/imx298-g1-20260913.ko

ssh root@172.16.42.1 '
set -e
chmod 0755 /newroot/tmp/op3-v4l2-af-g1-20260913
sha256sum /newroot/tmp/op3-v4l2-af-g1-20260913
busybox gzip -dc /newroot/tmp/op3-imx298-probe-modules.tar.gz | busybox tar -x -C /newroot
module_root=/newroot/lib/modules/6.12.1-msm8996+/kernel
insmod "$module_root/drivers/media/mc/mc.ko"
insmod "$module_root/drivers/media/v4l2-core/videodev.ko"
insmod "$module_root/drivers/media/v4l2-core/v4l2-async.ko"
insmod "$module_root/drivers/media/v4l2-core/v4l2-fwnode.ko"
insmod "$module_root/drivers/media/common/videobuf2/videobuf2-common.ko"
insmod "$module_root/drivers/media/common/videobuf2/videobuf2-memops.ko"
insmod "$module_root/drivers/media/common/videobuf2/videobuf2-dma-sg.ko"
insmod "$module_root/drivers/media/common/videobuf2/videobuf2-v4l2.ko"
insmod /newroot/tmp/i2c-qcom-cci-g1-20260913.ko
insmod "$module_root/drivers/media/platform/qcom/camss/qcom-camss.ko"
insmod /newroot/tmp/bu63165gwl-g1-20260913.ko
insmod /newroot/tmp/imx298-g1-20260913.ko
uname -a
cat /proc/cmdline
cat /proc/sys/kernel/random/boot_id
cat /sys/class/video4linux/*/name 2>/dev/null
dmesg > /tmp/op3-af-g1-20260913-before.log
/newroot/tmp/op3-v4l2-af-g1-20260913 \
  --frames 30 --focus 640 --exposure 893 --gain 240 \
  --output-prefix /tmp/op3-af-g1-20260913 \
  > /tmp/op3-af-g1-20260913.log 2>&1
rc=$?
echo helper-rc=$rc
cat /tmp/op3-af-g1-20260913.log
dmesg > /tmp/op3-af-g1-20260913-after.log
grep -iE "imx298|bu63165|camss|cci|csiphy|csid|vfe|v4l|fault|reset|timeout|smmu|reboot" \
  /tmp/op3-af-g1-20260913-before.log \
  /tmp/op3-af-g1-20260913-after.log
sha256sum /tmp/op3-af-g1-20260913-*.raw
'
```

若 SSH 地址是 Wi-Fi 地址，把上述两个 `root@172.16.42.1` 替换为当前手机的
Wi-Fi 地址即可；USB RNDIS 和 Wi-Fi 是不同网卡，不要用 USB 地址判断 `wlan0`
是否联网。

## G1 实际结果（2026-09-13）

在重启清空上一次 CAMSS 状态后，使用临时 boot 镜像
`artifacts/boot-oneplus3-pmos612-recovery-imx298-afpwdm-20260909.img`，SHA256
`64ccf213644f517f8c8b8c512869f396cb75c90920f89163fb8385a244dce4e2`。依赖模块
按 `mc -> videodev -> v4l2-async -> v4l2-fwnode -> videobuf2-* -> 当前 CCI
-> qcom-camss -> bu63165gwl -> imx298` 顺序加载，全部 `insmod-rc=0`。

设备执行的 helper 为 `eb825ce009e5168dc599ce11d5e158fe2d393415c023a92d6a7c38db71792157`，
结果为：

- `discover summary media=/dev/media0 sensor=/dev/v4l-subdev17 lens=/dev/v4l-subdev18 video=/dev/video0`；
- `1476x834` packed RAW10，`bytesperline=1848`，`sizeimage=1541232`；
- 一次连续 `STREAMON` 收到 `30` 帧，`frames_good=30 frames_error=0`，sequence `0..29` 连续；
- 第 4 个好帧后打开 `/dev/v4l-subdev18`，缓存位置 `0`，发送位置 `640`，
  `focus write position=640 ioctl_rc=0`；
- 内核同时记录 `AF_PWDM asserted logical=1`、`VAF enabled voltage=2800000`，
  以及 CCI 地址 `0x0e` 的 5 字节写事务；无新的 CSI/CAMSS/SMMU/重启错误。

完整设备日志和 30 个 RAW 文件哈希清单已保存到被忽略的本地证据目录：
`artifacts/op3-af-evidence/device-g1-20260913/`。

这使 G1 的两个条件均为 PASS，但只证明 VCM 控制 ioctl 能在活动采集期间成功
发出；`G_CTRL` 仍是驱动缓存，尚未证明镜头发生了光学位移。

## G1 判定

- 前提 PASS：日志出现 `capture ready good_frames=4`，至少 4 个连续无
  `V4L2_BUF_FLAG_ERROR` 的帧，所有帧有递增 sequence/timestamp，且一次
  `STREAMON` 没有 VFE/SMMU/CCI 错误。
- 命令 PASS：日志出现 `focus write position=... ioctl_rc=0`，并且内核日志能
  对应到这一次位置写事务。
- 若前提成立但位置写仍 `-ETIMEDOUT`，G1 只能排除“活动 IMX298 采集会话本身
  足以解决超时”，进入 G2；不能把它解释成地址或初始化已经确定错误。
- 若采集未建立、runtime 状态不可读、lens 打开失败或手机重启，结果是
  `INCONCLUSIVE`/`FAIL`，先保存日志，不重复启动/停止流来碰运气。
- 即使命令返回 0，也不能直接宣布镜头移动成功；需进入 G3，用固定场景和多
  位置 RAW 清晰度变化证明光学位置确实改变。
