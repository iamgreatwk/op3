# OP3 自动对焦 G1：连续采集期间的一次 VCM 命令

阶段：`OP3-AF-G1`  
状态：**OWNER-TEST-PENDING**  
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
38622b9eed773371d1fddef49039cce508865848c36c9e07271558b18feb2894  op3-v4l2-af-stream-test.aarch64
```

主机端 `gcc -Wall -Wextra -Werror` 自检通过；没有 camera media 节点的主机运行
会按预期返回 `ENODEV`，这不是手机测试结果。

## 手机测试

本阶段不刷机。手机必须先启动已经验证的 AF_PWDM 临时 boot 镜像，并加载同一组
已验证的 CCI/CAMSS/V4L2/IMX298/BU63165GWL 模块。程序和日志使用版本化文件名，
避免覆盖旧实验：

```sh
project=/home/kai/src/oneplus3-mainline
helper=$project/artifacts/op3-af-evidence/op3-v4l2-af-stream-test.aarch64

scp -O "$helper" root@172.16.42.1:/newroot/tmp/op3-v4l2-af-g1-20260913

ssh root@172.16.42.1 '
set -e
chmod 0755 /newroot/tmp/op3-v4l2-af-g1-20260913
sha256sum /newroot/tmp/op3-v4l2-af-g1-20260913
rm -f /tmp/op3-af-g1-20260913.log /tmp/op3-af-g1-20260913-*.raw
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

