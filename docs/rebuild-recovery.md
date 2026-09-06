# OnePlus 3 recovery 从零重建

本文是本项目从零重建 recovery 的唯一维护版本；桌面上的同名文件仅作
入口，不再维护第二份完整副本。

本流程对应当前正式整合线：

~~~text
GitHub 分支：agent/implementation/recovery-browser-001
内核基线：pmOS MSM8996 Linux 6.12.1
Buildroot：679b9ead7620bbf193620d1ebf56f53c1764d37a
~~~

正式 initramfs 由项目跟踪的源文件和 Buildroot 直接生成
images/rootfs.cpio.gz。它不读取、解包或追加历史 v100 ramdisk。
boot_fa5_v100_auto.img 和外部保存的 reference-initrd.img 只用于历史
溯源，不能出现在当前构建命令中。

项目所有者负责内核和 Buildroot 编译；准备脚本不会启动大规模编译，也
不会刷写设备。

## 1. 外部专有输入

以下目录必须独立于 GitHub 工作目录长期保留：

~~~text
/home/kai/op3-recovery-external-inputs/
├── ath10k/QCA6174/hw3.0/firmware-6.bin
├── ath10k/QCA6174/hw3.0/board-2.bin
├── qualcomm/NON-HLOS.bin
├── qualcomm/a530_zap.elf
├── qualcomm/a530/a530_pm4.fw
├── qualcomm/a530/a530_pfp.fw
├── qualcomm/a530/a530v3_gpmu.fw2
├── fonts/wqy-microhei.ttc                 # 仅可选浏览器 bundle
└── tools/bin/{mcopy,pil-squasher}
~~~

initrd/reference-initrd.img 和 archive/boot_fa5_v100_auto.img 可以保留作
历史对照，但不属于本流程输入。先校验外部目录：

~~~bash
export OP3_EXTERNAL_INPUTS=/home/kai/op3-recovery-external-inputs
./scripts/verify-op3-external-inputs.sh "$OP3_EXTERNAL_INPUTS"
export OP3_ATH10K_EXTFW_SOURCE="$OP3_EXTERNAL_INPUTS"
export OP3_MCOPY="$OP3_EXTERNAL_INPUTS/tools/bin/mcopy"
export OP3_PIL_SQUASHER="$OP3_EXTERNAL_INPUTS/tools/bin/pil-squasher"
~~~

## 2. 获取 GitHub 源码和 6.12.1 内核

~~~bash
mkdir -p /home/kai/op3-rebuild
cd /home/kai/op3-rebuild

git clone --branch agent/implementation/recovery-browser-001 \
  --single-branch https://github.com/iamgreatwk/op3.git .
./scripts/agent-start.sh

mkdir -p source
git clone --branch msm8996-stable-6.12.y --single-branch \
  https://gitlab.com/msm8996-mainline/linux.git \
  source/linux-pmos-msm8996-6.12-base

./scripts/restore-op3-recovery-kernel.sh \
  source/linux-pmos-msm8996-6.12-base \
  source/linux-pmos-msm8996-6.12-recovery-audio-full \
  agent/implementation/recovery-browser-audio-full-001
~~~

恢复脚本会应用 GitHub 中归档的 31 个补丁，并把外部 ath10k 文件安装
到内核要求的 extfw/ 路径。它不会编译内核。由于 patch 邮件不包含历史
提交者时间，新机器恢复后的提交 SHA 可能不同；脚本和校验器锁定的是
基线、补丁数量以及最终 tree hash，而不是不可重建的旧提交时间。

## 3. 内核编译（项目所有者执行）

~~~bash
set -e

project=/home/kai/op3-rebuild-clean-20260906
cd "$project"
kernel="$project/source/linux-pmos-msm8996-6.12-recovery-audio-full"
kout="$project/out/pmos-msm8996-6.12-recovery-audio-full-s1302-retry"
wifi_mods="$project/artifacts/op3-wifi-modules-root"

mkdir -p "$kout"
test -r "$kout/.config" || cp \
  "$project/kernel/configs/oneplus3-recovery-audio-full.config" \
  "$kout/.config"

# Keep the locked Image.gz reproducible across build hosts. Linux uses one
# timestamp for the empty default initramfs and a separate final UTS_VERSION.
# These are two intermediate values from the archived locked image.
export KBUILD_BUILD_USER=kai
export KBUILD_BUILD_HOST=AgentBuilder
unset KBUILD_BUILD_VERSION KBUILD_BUILD_TIMESTAMP

make -C "$kernel" O="$kout" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 olddefconfig
make -C "$kernel" O="$kout" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  -j"$(nproc)" Image.gz dtbs modules

# Recreate the empty default initramfs with its locked directory mtime. Use a
# numeric timezone: GNU date interprets the literal abbreviation CST as US
# Central time, while the archived mtime is 14:32:51 China Standard Time.
export KBUILD_BUILD_TIMESTAMP='Sun Sep  6 14:32:51 +0800 2026'
make -C "$kernel" O="$kout" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  usr/initramfs_data.cpio

# The final link must leave this CPIO untouched while Kbuild regenerates the
# timestamped version object. Remove only the timestamp option from the saved
# output command; the CPIO bytes and its locked mtime remain unchanged.
sed -i -E 's/[[:space:]]+-d "[^"]*"//' \
  "$kout/usr/.initramfs_data.cpio.cmd"

# Build init/version.o with Linux's normal temporary UTS_VERSION. The final
# version string is added later by init/version-timestamp.o.
printf '0\n' > "$kout/.version"
unset KBUILD_BUILD_VERSION KBUILD_BUILD_TIMESTAMP
make -C "$kernel" O="$kout" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  init/version.o

# Final link: leave KBUILD_BUILD_TIMESTAMP unset so the temporary version
# header stays the normal untimestamped value. Kbuild's date call is wrapped
# only for the final timestamp object, and the saved CPIO command above now
# matches the no-timestamp final invocation.
date_bin="$(mktemp -d /tmp/op3-repro-date.XXXXXX)"
ln -s "$project/scripts/op3-repro-date.sh" "$date_bin/date"
export OP3_REPRO_BUILD_TIMESTAMP='Sun Sep  6 14:36:13 CST 2026'
unset KBUILD_BUILD_TIMESTAMP MAKEFLAGS
PATH="$date_bin:$PATH" make -C "$kernel" O="$kout" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  Image.gz
unset OP3_REPRO_BUILD_TIMESTAMP

test ! -e "$wifi_mods/lib/modules"
mkdir -p "$wifi_mods"
make -C "$kernel" O="$kout" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  INSTALL_MOD_PATH="$wifi_mods" modules_install
~~~

## 4. 暂存启动固件

这些命令只生成经过哈希校验的固件目录，不生成 initramfs：

~~~bash
./scripts/prepare-a530-firmware.sh artifacts/a530-firmware

./scripts/stage-msm8996-oneplus3-firmware.sh \
  artifacts/msm8996-oneplus3-firmware-verified

./scripts/stage-op3-initramfs-firmware.sh \
  artifacts/op3-initramfs-firmware
~~~

最终目录至少包含：

~~~text
artifacts/op3-initramfs-firmware/lib/firmware/
├── ath10k/QCA6174/hw3.0/{firmware-6.bin,board-2.bin}
├── qcom/{a530_pm4.fw,a530_pfp.fw,a530v3_gpmu.fw2}
└── qcom/msm8996/oneplus3/{adsp.mbn,modem.mbn,slpi.mbn,venus.mbn,mba.mbn,a530_zap.mbn}
~~~

## 5. 准备并编译默认 recovery Buildroot

准备脚本必须对全新的、干净的 Buildroot 源树运行：

~~~bash
./scripts/prepare-op3-buildroot.sh source/buildroot recovery
~~~

它会安装项目跟踪的 op3-recovery 和 op3-initramfs 包、启动源文件、Wi-Fi
脚本和 post-build hook，并启用 op3_recovery_defconfig。默认配置包含
recovery、TinyALSA、Wi-Fi 工具和诊断工具；Mesa/Freedreno、Weston、
WPE WebKit、Cog、WPEWebDriver 均不启用。

如果主机的 `install --version` 显示 `uutils coreutils 0.8.0`，Buildroot
会主动停止。Ubuntu 当前可使用本地 GNU 工具优先路径，不必修改系统：

~~~bash
mkdir -p "$PWD/host-tools"
ln -sfn /usr/bin/gnuinstall "$PWD/host-tools/install"
export PATH="$PWD/host-tools:$PATH"
install --version | head -1
~~~

如果主机没有 `/usr/bin/gnuinstall`，先安装 GNU coreutils，或按 Buildroot
提示配置系统 alternatives。

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

`BR2_PRIMARY_SITE` 只改变下载候选顺序，Buildroot 仍会在该源缺少文件时
继续尝试包自身的上游地址；它不改变源码版本或校验值。构建中断后重新
执行同一命令即可从已有的 `dl/` 和 `output/` 继续。

Buildroot 输出有两个用途：

~~~text
out/buildroot-op3-recovery/images/rootfs.cpio.gz
    正式、自建 initramfs；直接作为 pack-boot.sh 的 ramdisk 输入

out/buildroot-op3-recovery/target/
    同一份 target 的持久化 /newroot 内容
~~~

确认 CPIO 是本次 Buildroot 生成的完整 rootfs，而不是旧 overlay：

~~~bash
mkdir -p artifacts
install -D -m 0644 \
  out/buildroot-op3-recovery/images/rootfs.cpio.gz \
  artifacts/initrd-op3-recovery-buildroot.cpio.gz
gzip -t artifacts/initrd-op3-recovery-buildroot.cpio.gz
sha256sum artifacts/initrd-op3-recovery-buildroot.cpio.gz
~~~

如需把同一 target 部署到持久化分区：

~~~bash
./scripts/stage-op3-audio-rootfs.sh \
  out/buildroot-op3-recovery/target \
  artifacts/op3-recovery-audio-rootfs.tar.gz
~~~

这个 tar 包不是 initramfs；它是可选的 /newroot 持久化内容。Wi-Fi 凭据
仍需在设备上通过 wifi connect 写入，默认 IPv6 关闭。

## 6. 打包 boot image

~~~bash
kout=out/pmos-msm8996-6.12-recovery-audio-full-s1302-retry

./scripts/pack-boot.sh \
  "$kout/arch/arm64/boot/Image.gz" \
  "$kout/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb" \
  artifacts/initrd-op3-recovery-buildroot.cpio.gz \
  artifacts/boot-oneplus3-pmos612-recovery-buildroot-initramfs.img
~~~

该步骤读取项目自己的 boot/oneplus3-fa5.env 参数，不读取历史 v100 镜像。
主机需要 mkbootimg 和 abootimg；后者这里只用于检查新生成的 Android
boot image。

## 7. 校验和设备测试

~~~bash
./scripts/verify-op3-recovery-manifest.sh --source
./scripts/verify-op3-recovery-manifest.sh --artifacts
~~~

artifacts 模式会打印本次 Buildroot initramfs 和 boot image 的实际
SHA256；新构建的 hash 由项目所有者在设备测试后回填 manifest/handoff。
在此之前它们显示为 OWNER_BUILD_REQUIRED，不能误认为旧测试 artifact。

授权测试时再执行设备侧 fastboot boot。收集至少：

~~~text
/root/boot_mainline.log
/tmp/op3-recovery.log
/tmp/op3-audio-init.log
wifi current
ip -4 addr show wlan0
cat /proc/asound/cards
cat /proc/bus/input/devices
~~~

## 8. 可选浏览器 bundle

浏览器不进入默认 recovery initramfs。需要测试 Cog/WPE 时使用独立的
Buildroot 源树和输出目录：

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

浏览器 bundle 使用外部 CJK 字体，部署到设备后才由 recovery 的 browser
命令调用；它不改变默认 initramfs。

## 9. 删除本地文件后的恢复

可以删除 checkout、source/、out/ 和 artifacts/。以后只需重新 clone 本文
第 2 节的 GitHub 分支，再保留并校验完整的：

~~~text
/home/kai/op3-recovery-external-inputs
~~~

不要删除该外部目录中的 ath10k、Qualcomm、A530 固件和工具。历史
archive/boot_fa5_v100_auto.img 与 initrd/reference-initrd.img 不参与当前
initramfs 生成；它们只在需要复核历史启动结果时使用。
