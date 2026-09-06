# OnePlus 3 recovery

这是当前分支 'agent/implementation/recovery-browser-001' 的完整重建入口。
项目目标是用 pmOS MSM8996 Linux 6.12.1、项目自己的 Buildroot 集成和自建
initramfs 启动 OnePlus 3 recovery。当前默认 recovery 包含 DRM 界面、触摸、
电源键、音量键、三段式按键、下巴电容键、振动、TinyALSA 音频和 Wi-Fi；
Mesa/Weston/WPE WebKit/Cog/WPEWebDriver 不进入默认镜像，浏览器仅保留为
可选的独立 Buildroot bundle。

详细背景、校验锁和历史输入边界见
[docs/rebuild-recovery.md](docs/rebuild-recovery.md)。所有构建输出均为
生成物；源代码、补丁、配置和构建脚本以本分支为准。

## 1. 当前构建锁

顶层项目和内核是两个独立 Git 仓库，必须分别检查：

~~~text
顶层项目分支：agent/implementation/recovery-browser-001
内核基线：    msm8996-stable-6.12.y
内核工作树：  source/linux-pmos-msm8996-6.12-recovery-audio-full
内核分支：    agent/implementation/recovery-browser-audio-full-001
Buildroot：   679b9ead7620bbf193620d1ebf56f53c1764d37a
~~~

唯一的源/产物锁是 [manifests/op3-recovery-audio-full.env](manifests/op3-recovery-audio-full.env)。
启动镜像使用 [boot/oneplus3-fa5.env](boot/oneplus3-fa5.env)；历史的
boot_fa5_v100_auto.img 和 reference-initrd.img 不参与当前构建。

当前集成线的预期内核产物为：

~~~text
out/pmos-msm8996-6.12-recovery-audio-full-s1302-retry/arch/arm64/boot/Image.gz
out/pmos-msm8996-6.12-recovery-audio-full-s1302-retry/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb
~~~

## 2. 主机依赖和外部输入

主机需要 Git、GNU coreutils、aarch64-linux-gnu-gcc-11、内核构建依赖、
Buildroot 构建依赖、mkbootimg、abootimg、fastboot、zstd、mtools 和
pil-squasher。Buildroot 如果报 uutils install，先确保当前 shell 使用
GNU install：

~~~bash
mkdir -p "$PWD/host-tools"
ln -sfn /usr/bin/gnuinstall "$PWD/host-tools/install"
export PATH="$PWD/host-tools:$PATH"
install --version | head -1
~~~

以下专有输入不能提交到 GitHub，必须长期放在项目目录之外：

~~~text
/home/kai/op3-recovery-external-inputs/
├── ath10k/QCA6174/hw3.0/firmware-6.bin
├── ath10k/QCA6174/hw3.0/board-2.bin
├── qualcomm/NON-HLOS.bin
├── qualcomm/a530_zap.elf
├── qualcomm/a530/a530_pm4.fw
├── qualcomm/a530/a530_pfp.fw
├── qualcomm/a530/a530v3_gpmu.fw2
├── fonts/wqy-microhei.ttc       # 仅可选浏览器 bundle 需要
└── tools/bin/{mcopy,pil-squasher}
~~~

外部目录必须同时有 SHA256SUMS 和 SHA512SUMS。每次新机器重建先校验：

~~~bash
cd /path/to/oneplus3-mainline
export OP3_EXTERNAL_INPUTS=/home/kai/op3-recovery-external-inputs
./scripts/verify-op3-external-inputs.sh "$OP3_EXTERNAL_INPUTS"
~~~

## 3. 从 GitHub 获取项目和恢复内核

在空目录执行：

~~~bash
mkdir -p /home/kai/op3-rebuild
cd /home/kai/op3-rebuild

git clone --branch agent/implementation/recovery-browser-001 \
  --single-branch https://github.com/iamgreatwk/op3.git .

export OP3_EXTERNAL_INPUTS=/home/kai/op3-recovery-external-inputs
./scripts/agent-start.sh
./scripts/verify-op3-external-inputs.sh "$OP3_EXTERNAL_INPUTS"
~~~

下载并恢复独立内核仓库：

~~~bash
mkdir -p source
git clone --branch msm8996-stable-6.12.y --single-branch \
  https://gitlab.com/msm8996-mainline/linux.git \
  source/linux-pmos-msm8996-6.12-base

./scripts/restore-op3-recovery-kernel.sh \
  source/linux-pmos-msm8996-6.12-base \
  source/linux-pmos-msm8996-6.12-recovery-audio-full \
  agent/implementation/recovery-browser-audio-full-001
~~~

恢复脚本会从
patches/pmos612-op3-recovery-audio-full/ 应用锁定的 31 个补丁，并校验
内核基线、补丁数量、最终 tree 和 ath10k 文件。它不会编译内核，也不会
刷写手机。恢复后检查源代码锁：

~~~bash
./scripts/verify-op3-recovery-manifest.sh --source
git status --short --branch
git -C source/linux-pmos-msm8996-6.12-recovery-audio-full status --short --branch
git -C source/linux-pmos-msm8996-6.12-recovery-audio-full log -1 --oneline
~~~

## 4. 获取本机 sda15 UUID

持久化 recovery rootfs 位于手机 /dev/sda15。UUID 属于该文件系统，换
手机或重新格式化分区后必须重新读取；不能照抄另一台手机的 UUID。

不格式化、只读取现有文件系统：

~~~bash
ssh root@172.16.42.1 '
set -e
part=/dev/sda15
part_name=$(basename "$part")
test -b "$part"
test "$(cat /sys/class/block/$part_name/partition)" = 15
blkid "$part"
blkid -s UUID -o value "$part"
'
~~~

如果刚刚明确授权格式化了该分区，格式化会删除原内容，完成后立即读取新
UUID：

~~~bash
ssh root@172.16.42.1 '
set -e
mkfs.ext4 -F /dev/sda15
blkid -s UUID -o value /dev/sda15
'
~~~

把输出写入启动 profile，再提交该修改：

~~~bash
uuid=替换为上一步输出的UUID
sed -i -E "s#pmos_root_uuid=[^ ]+#pmos_root_uuid=$uuid#" \
  boot/oneplus3-fa5.env
grep -n 'BOOT_CMDLINE' boot/oneplus3-fa5.env
~~~

如果只做一次临时测试，可以使用 BOOT_CMDLINE_OVERRIDE 传入完整命令行；
正式版本仍应把实际 UUID 写回 boot/oneplus3-fa5.env。打包前再次确认：

~~~bash
grep -nE 'BOOT_(CMDLINE|APPEND_DTB|RAMDISK_OFFSET)' boot/oneplus3-fa5.env
~~~

## 5. 暂存固件

这些步骤只准备经过哈希校验的固件目录，不编译内核、不生成 boot image：

~~~bash
./scripts/prepare-a530-firmware.sh artifacts/a530-firmware

./scripts/stage-msm8996-oneplus3-firmware.sh \
  artifacts/msm8996-oneplus3-firmware-verified

./scripts/stage-op3-initramfs-firmware.sh \
  artifacts/op3-initramfs-firmware
~~~

最终 initramfs 固件至少包含：

~~~text
artifacts/op3-initramfs-firmware/lib/firmware/
├── ath10k/QCA6174/hw3.0/{firmware-6.bin,board-2.bin}
├── qcom/{a530_pm4.fw,a530_pfp.fw,a530v3_gpmu.fw2}
└── qcom/msm8996/oneplus3/
    {adsp.mbn,modem.mbn,slpi.mbn,venus.mbn,mba.mbn,a530_zap.mbn}
~~~

## 6. 内核编译（大规模编译步骤）

内核使用独立工作树和独立输出目录。不要在顶层项目的
source/linux-pmos-msm8996-6.12 元数据工作树上直接编译，也不要覆盖其它
输出目录。以下命令由构建操作者执行：

~~~bash
set -e

project="$PWD"
kernel="$project/source/linux-pmos-msm8996-6.12-recovery-audio-full"
kout="$project/out/pmos-msm8996-6.12-recovery-audio-full-s1302-retry"
wifi_mods="$project/artifacts/op3-wifi-modules-root"

mkdir -p "$kout"
test -r "$kout/.config" || cp \
  "$project/kernel/configs/oneplus3-recovery-audio-full.config" \
  "$kout/.config"

export KBUILD_BUILD_USER=kai
export KBUILD_BUILD_HOST=AgentBuilder
unset KBUILD_BUILD_VERSION KBUILD_BUILD_TIMESTAMP

make -C "$kernel" O="$kout" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 olddefconfig
make -C "$kernel" O="$kout" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  -j"$(nproc)" Image.gz dtbs modules
~~~

如果需要复现当前锁定 Image.gz 的字节级 SHA256，还要按
docs/rebuild-recovery.md 第 4 节执行固定 initramfs mtime、init/version.o
和最终日期包装步骤。普通功能重建只需上面的编译命令；不要为了重现 hash
修改源文件或提交生成的 out/。

安装 Wi-Fi 内核模块到持久化 rootfs 的独立目录：

~~~bash
test ! -e "$wifi_mods/lib/modules"
mkdir -p "$wifi_mods"
make -C "$kernel" O="$kout" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  INSTALL_MOD_PATH="$wifi_mods" modules_install
~~~

编译结束后记录：

~~~bash
sha256sum \
  "$kout/.config" \
  "$kout/arch/arm64/boot/Image.gz" \
  "$kout/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb"
~~~

## 7. 准备并编译默认 recovery Buildroot

Buildroot 是独立仓库，准备脚本会自动 clone 并切换到 manifest 锁定的
679b9ead7620bbf193620d1ebf56f53c1764d37a。该脚本只修改干净的
source/buildroot，不会启动 Buildroot 编译；重复准备时应使用新的干净
Buildroot 源目录。

~~~bash
./scripts/prepare-op3-buildroot.sh source/buildroot recovery
~~~

该配置把 recovery、initramfs、TinyALSA、Wi-Fi 工具、模块和诊断支持集成
进 Buildroot，并默认关闭 Mesa/Freedreno、Weston、WPE WebKit、Cog 和
WPEWebDriver。

由构建操作者执行：

~~~bash
mkdir -p out/buildroot-op3-recovery

OP3_WIFI_MODULES_ROOT="$PWD/artifacts/op3-wifi-modules-root" \
OP3_INITRAMFS_FIRMWARE_ROOT="$PWD/artifacts/op3-initramfs-firmware" \
make -C source/buildroot O="$PWD/out/buildroot-op3-recovery" \
  op3_recovery_defconfig

OP3_WIFI_MODULES_ROOT="$PWD/artifacts/op3-wifi-modules-root" \
OP3_INITRAMFS_FIRMWARE_ROOT="$PWD/artifacts/op3-initramfs-firmware" \
make -C source/buildroot O="$PWD/out/buildroot-op3-recovery" \
  BR2_PRIMARY_SITE=https://sources.buildroot.net \
  BR2_JLEVEL=3 2>&1 | tee /tmp/buildroot-op3-recovery.log
~~~

Buildroot 完成后有两个输出：

~~~text
out/buildroot-op3-recovery/images/rootfs.cpio.gz  自建 recovery initramfs
out/buildroot-op3-recovery/target/                持久化 /newroot 内容
~~~

把本次 CPIO 和持久化 rootfs 导出到 artifacts：

~~~bash
mkdir -p artifacts
install -D -m 0644 \
  out/buildroot-op3-recovery/images/rootfs.cpio.gz \
  artifacts/initrd-op3-recovery-buildroot.cpio.gz
gzip -t artifacts/initrd-op3-recovery-buildroot.cpio.gz
sha256sum artifacts/initrd-op3-recovery-buildroot.cpio.gz

./scripts/stage-op3-audio-rootfs.sh \
  out/buildroot-op3-recovery/target \
  artifacts/op3-audio-rootfs.tar.gz
~~~

两个 stage 脚本默认拒绝覆盖已有目标。需要重建时使用新的输出目录/文件名，
或在确认目标是本次生成物后只删除那个精确的旧目标；不要用通配符清理整个
artifacts/。

## 8. 打包 boot image

确认 UUID 已写入 profile 后执行：

~~~bash
kout=out/pmos-msm8996-6.12-recovery-audio-full-s1302-retry

./scripts/pack-boot.sh \
  "$kout/arch/arm64/boot/Image.gz" \
  "$kout/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb" \
  artifacts/initrd-op3-recovery-buildroot.cpio.gz \
  artifacts/boot-oneplus3-pmos612-recovery-buildroot-initramfs.img
~~~

pack-boot.sh 自动读取 boot/oneplus3-fa5.env，把 DTB 按 profile 追加到
内核，并使用其中的 pmos_root_uuid。它不读取历史 v100 镜像。生成后检查：

~~~bash
./scripts/verify-op3-recovery-manifest.sh --source
./scripts/verify-op3-recovery-manifest.sh --artifacts
sha256sum \
  artifacts/initrd-op3-recovery-buildroot.cpio.gz \
  artifacts/boot-oneplus3-pmos612-recovery-buildroot-initramfs.img
~~~

如果只是临时换 UUID 测试，可以这样传完整命令行，不修改 profile：

~~~bash
BOOT_CMDLINE_OVERRIDE='完整的内核命令行' \
./scripts/pack-boot.sh \
  "$kout/arch/arm64/boot/Image.gz" \
  "$kout/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb" \
  artifacts/initrd-op3-recovery-buildroot.cpio.gz \
  artifacts/boot-oneplus3-pmos612-recovery-buildroot-initramfs.img
~~~

### 8.1 永久写入 boot 分区

`fastboot boot` 只在本次启动使用镜像；确认临时启动已经通过后，才执行
下面的永久刷写。`fastboot flash boot` 会覆盖手机当前 boot 分区，必须先
确认设备序列号和输入文件，不能把它误用于 `system` 或其它分区：

~~~bash
boot_image=artifacts/boot-oneplus3-pmos612-recovery-buildroot-initramfs.img
test -f "$boot_image"
sha256sum "$boot_image"
fastboot devices
fastboot getvar product 2>&1
fastboot getvar current-slot 2>&1 || true

fastboot flash boot "$boot_image"
fastboot reboot
~~~

刷写后仍需按第 10 节检查启动日志、DRM 界面、音频、实体按键、振动和
Wi-Fi。若只是验证新镜像，优先使用第 10 节的 `fastboot boot`，不要重复刷写。

## 9. 可选：部署持久化 /newroot

这一步会删除 /dev/sda15 上原有内容，只有在已经确认目标设备、目标分区
和备份状态后执行。先只读确认分区和挂载点：

~~~bash
ssh root@172.16.42.1 '
set -e
part=/dev/sda15
part_name=$(basename "$part")
test -b "$part"
test "$(cat /sys/class/block/$part_name/partition)" = 15
blkid "$part"
mountpoint -q /newroot || mount "$part" /newroot
test "$(findmnt -no SOURCE -T /newroot)" = "$part"
'
~~~

确认无误后，清空的范围严格限制为已确认的 /newroot 挂载点，再解包本次
Buildroot target：

~~~bash
ssh root@172.16.42.1 '
set -e
mountpoint -q /newroot
test "$(findmnt -no SOURCE -T /newroot)" = /dev/sda15
find /newroot -mindepth 1 -maxdepth 1 -exec rm -rf -- {} +
sync
'

scp -O artifacts/op3-audio-rootfs.tar.gz \
  root@172.16.42.1:/newroot/tmp/

ssh root@172.16.42.1 '
set -e
busybox gzip -dc /newroot/tmp/op3-audio-rootfs.tar.gz | \
  busybox tar -x -C /newroot
sync
rm -f /newroot/tmp/op3-audio-rootfs.tar.gz
'
~~~

解包后重新读取 UUID；如果分区曾格式化，回到第 4 节更新 profile 并重新
打包。/newroot 的 rootfs tar 是持久化内容，不是启动 initramfs。

## 10. 手机临时启动和验证

进入手机 bootloader/fastboot 后只做临时启动，不写入 boot 分区：

~~~bash
fastboot devices
fastboot boot \
  artifacts/boot-oneplus3-pmos612-recovery-buildroot-initramfs.img
~~~

启动后通过 USB 网络登录（默认 172.16.42.1），重点始终区分 USB 网卡
usb0 和无线网卡 wlan0：

~~~bash
ssh root@172.16.42.1 '
cat /root/boot_mainline.log
cat /tmp/op3-recovery.log
cat /tmp/fb.log
wifi current
iw dev wlan0 link
ip -4 addr show dev wlan0
ip -4 route
cat /proc/asound/cards
cat /proc/asound/pcm
cat /proc/bus/input/devices
'
~~~

Wi-Fi 运行约定：默认 IPv6 关闭；凭据由设备上的 wifi connect 写入：

~~~bash
ssh root@172.16.42.1 'wifi connect "SSID" "密码"'
ssh root@172.16.42.1 '
wifi current
iw dev wlan0 link
ip -4 addr show dev wlan0
ip -4 route
wifi ipv6 status
'
~~~

自动连接依赖设备持久分区中的一个默认 profile，而不是依赖镜像内的固定
密码。执行 `wifi connect` 会创建
`/newroot/etc/op3-wifi/profiles/*.conf` 和
`/newroot/etc/op3-wifi/default`；这两个文件按设计不会进入 GitHub、initramfs
或 rootfs tar。清空并重新部署 `/dev/sda15` 后必须重新执行一次：

~~~bash
ssh root@172.16.42.1 '
set +e
mount | grep "on /newroot"
ls -la /newroot/etc/op3-wifi /newroot/etc/op3-wifi/profiles
wifi list
'

ssh root@172.16.42.1 'wifi connect "SSID" "密码"'
ssh root@172.16.42.1 '
wifi list
wifi current
iw dev wlan0 link
ip -4 addr show dev wlan0
ip -4 route
'
~~~

如果 `wifi list` 显示 `no saved Wi-Fi profiles`，这就是自动连接未发生的
直接原因；不是缺少 `wifi_auto.sh` 或 ath10k 固件。若 profile 已存在但仍
未连接，再收集 `/root/boot_mainline.log` 中的 `wifi_auto`、`newroot`、
`wlan0` 行及 `dmesg | grep -iE 'ath10k|wlan|firmware|rfkill'`。

仅在按需测试 IPv6 时启用，验证完成后可关闭：

~~~bash
ssh root@172.16.42.1 'wifi ipv6 on'
ssh root@172.16.42.1 'wifi ipv6 off'
~~~

实体键、触摸、振动和音频的验证日志分别关注：

~~~bash
ssh root@172.16.42.1 '
grep -E "recovery fds|input:|vibration:" /tmp/fb.log
cat /proc/bus/input/devices
cat /proc/asound/cards
cat /proc/asound/pcm
ls -l /dev/snd
'
~~~

## 11. 可选浏览器 bundle

浏览器不属于默认 recovery 镜像。需要单独测试 Cog/WPE 时，在独立的干净
Buildroot 源目录准备 browser profile：

~~~bash
./scripts/prepare-op3-buildroot.sh source/buildroot-browser browser
make -C source/buildroot-browser O="$PWD/out/buildroot-op3-egl" \
  op3_browser_defconfig
make -C source/buildroot-browser O="$PWD/out/buildroot-op3-egl" \
  BR2_JLEVEL=3 2>&1 | tee /tmp/buildroot-op3-browser.log
./scripts/stage-browser-rootfs.sh \
  out/buildroot-op3-egl/target \
  artifacts/op3-browser-bundle.tar.gz
~~~

该 bundle 需要外部 CJK 字体，部署后由 recovery 的 browser 命令调用；它不
改变默认 initramfs，也不应被加入 recovery profile。手机测试时注意温度和
功耗，浏览器退出后应由 recovery 重新取得 DRM 并恢复界面。

## 12. 重建后的提交和清理规则

源码变更、配置变更、脚本变更和 UUID profile 变更都必须提交到当前项目
分支；内核提交只存在于独立内核仓库。构建前检查：

~~~bash
git status --short --branch
git -C source/linux-pmos-msm8996-6.12-recovery-audio-full \
  status --short --branch
./scripts/verify-op3-recovery-manifest.sh --source
~~~

可删除 out/、artifacts/ 和整个 checkout 后，从第 3 节重新获取；但不能
删除 /home/kai/op3-recovery-external-inputs。不要把编译输出、专有固件、
历史 boot 镜像或历史 reference initrd 当作 GitHub 源码依赖。重新生成的
initramfs 必须来自 out/buildroot-op3-recovery/images/rootfs.cpio.gz，而不
是历史 ramdisk 的解包或追加。
