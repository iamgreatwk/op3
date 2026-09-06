#!/bin/sh
# ============================================================
# Agent OS 主线内核(6.x)启动脚本 - 独立于 3.18 版
# 原则（用户 2026-08-20 明确）：不修改 3.18 成熟脚本
# （init_boot.sh/init_audio.sh/init_wifi.sh/init_rndis.sh 全部原样保留），
# 主线版独立编写。当前脚本由 Buildroot 自建 initramfs 安装，负责：
#   基础挂载 + USB 网络 + SSH + 时间恢复 + Wi-Fi/音频后台初始化。
# 所有依赖均来自当前 Buildroot target 或项目跟踪的启动源文件。
# ============================================================

LOG_FILE=/root/boot_mainline.log
log() {
    echo "[init-mainline] $*" > /dev/console 2>/dev/null
    echo "[init-mainline] $*" >> "$LOG_FILE" 2>/dev/null
    [ -c /dev/ttyGS0 ] && echo "[init-mainline] $*" > /dev/ttyGS0 2>/dev/null   # ACM 串口同步输出
    echo "[init-mainline] $*" > /dev/kmsg 2>/dev/null     # v86: 进 dmesg → recovery 启动画面可见
}

log "=== Agent OS mainline (6.x) init ==="

# ---- 1. 基础文件系统 ----
mount -t proc proc /proc 2>/dev/null
mount -t sysfs sysfs /sys 2>/dev/null
mount -t devtmpfs devtmpfs /dev 2>/dev/null || { mdev -s 2>/dev/null; }
mkdir -p /dev/pts /dev/shm /tmp /run /var/log
mount -t devpts devpts /dev/pts 2>/dev/null
mount -t tmpfs tmpfs /tmp 2>/dev/null
mount -t tmpfs tmpfs /run 2>/dev/null

# ---- 1.5 mdev 热插拔（switch_root 后 /proc 重新挂载，hotplug 被重置）----
echo /sbin/mdev > /proc/sys/kernel/hotplug 2>/dev/null
mdev -s 2>/dev/null

# ---- 1.6 接入持久 rootfs（/newroot，v98：命令/recovery 放 rootfs 而非 initramfs）----
# OnePlus 3 的持久 rootfs 在 sda15；该机必须通过 pmos_root_uuid 选择它。
# /dev/disk/by-uuid 不是当前 initramfs 的必备节点，因此从所有已出现的
# block device 读取 UUID。UFS 分区节点出现和可读之间还有短暂窗口，最多重试
# 15 秒，避免第一次 mount 过早失败后永久降级到 initramfs。
# 内含全部用户命令（wifi/theme/fontsize/backlight/pkg）+ recovery。
# v96 只用 initramfs（内存系统，重启丢命令）→ v98 把 /newroot 接入 PATH + recovery。
root_uuid=
for arg in $(cat /proc/cmdline 2>/dev/null); do
    case "$arg" in
        pmos_root_uuid=*) root_uuid=${arg#pmos_root_uuid=} ;;
    esac
done

root_dev=/dev/sda15
if [ -n "$root_uuid" ]; then
    root_dev=
fi

newroot_mounted() {
    mount | grep -q 'on /newroot'
}

root_mount_try=0
while ! newroot_mounted; do
    if [ -n "$root_uuid" ]; then
        for candidate in /dev/sd* /dev/mmcblk* /dev/nvme*; do
            [ -b "$candidate" ] || continue
            if blkid "$candidate" 2>/dev/null | grep -q "UUID=\"$root_uuid\""; then
                root_dev=$candidate
                break
            fi
        done
    fi

    if [ -n "$root_dev" ] && [ -b "$root_dev" ]; then
        mount -t ext4 "$root_dev" /newroot 2>/dev/null || \
            mount "$root_dev" /newroot 2>/dev/null || true
    fi
    newroot_mounted && break

    root_mount_try=$((root_mount_try + 1))
    [ "$root_mount_try" -ge 15 ] && break
    sleep 1
done
if mount | grep -q 'on /newroot'; then
    export PATH=/newroot/usr/bin:/newroot/usr/sbin:/newroot/bin:/newroot/sbin:$PATH
    log "newroot mounted ($root_dev UUID=$root_uuid) + PATH 接入: $(ls /newroot/usr/bin/wifi 2>/dev/null && echo 'wifi OK' || echo 'wifi 缺失')"
    # ---- 1.6b 自动遍历 /newroot/usr/bin 建命令 symlink（v100：加命令零重打包）----
    # v99 用显式列表（wifi/theme/...）→ 新增命令还要改列表重打包。
    # v100 自动遍历：任何放进 /newroot/usr/bin 的可执行文件，重启自动接入。
    # 以后加命令 = 传文件到 /newroot/usr/bin + chmod +x，不用重刷镜像！
    N=0
    for c in /newroot/usr/bin/*; do
        [ -L "$c" ] && continue
        base=$(basename "$c")
        [ -x "$c" ] && [ ! -e "/usr/bin/$base" ] && { ln -sf "$c" "/usr/bin/$base" 2>/dev/null && N=$((N+1)); }
    done
    log "newroot cmd symlinks: $N 个命令自动接入（/newroot/usr/bin → /usr/bin）"
    for c in wifi theme fontsize backlight pkg; do
        [ -e "/usr/bin/$c" ] && log "  cmd $c: $(readlink /usr/bin/$c 2>/dev/null)"
    done
else
    log "WARN: root UUID=$root_uuid ($root_dev) 挂载失败，/newroot 不可用（命令/恢复降级为 initramfs 版）"
fi

# ---- 1.7 喂熵（v87 关键修复）----
# 内核无 qcom 硬件 RNG + 无 bootloader rng-seed → 启动早期 CRNG 未就绪
# → getrandom() 阻塞 → dropbear 卡 "waiting for kernel randomness"、wpa 卡。
# feed_entropy 用 RNDADDENTROPY ioctl 添加熵 → crng_init 推进 → 全部解锁。
if [ -x /usr/bin/feed_entropy ]; then
    /usr/bin/feed_entropy >> "$LOG_FILE" 2>&1
    log "feed_entropy done (CRNG should be ready)"
else
    log "WARN: feed_entropy 不存在"
fi

# ---- 1.8 dropbear 目录修复（v87）----
# Buildroot 的 /etc/dropbear 是 symlink → /var/run/dropbear；initramfs 里
# /var/run 是 tmpfs 但子目录未建 → dropbear 写 hostkey tmp 文件报
# "No such file or directory"。先建真实目标目录。
mkdir -p /var/run/dropbear 2>/dev/null
chmod 700 /var/run/dropbear 2>/dev/null
mkdir -p /etc/dropbear 2>/dev/null

# ---- 2. USB gadget 网络（usb0，主线 configfs gadget）----
# initramfs 阶段已配好 gadget；switch_root 后接口保留，这里补 IP。
# 若 usb0 不存在则尝试用 configfs 重新配置（libcomposite 需内置）。
mount -t configfs configfs /sys/kernel/config 2>/dev/null
if [ ! -e /sys/class/net/usb0 ]; then
    # v82+：configfs 自动创建 RNDIS + ACM 串口（内核 CONFIGFS + F_RNDIS/ACM=y）
    UDC=$(ls /sys/class/udc/ 2>/dev/null | head -1)
    if [ -n "$UDC" ] && [ -d /sys/kernel/config/usb_gadget ]; then
        mkdir -p /sys/kernel/config/usb_gadget/op3
        cd /sys/kernel/config/usb_gadget/op3 2>/dev/null || true
        echo 0x1d6b > idVendor 2>/dev/null
        echo 0x0104 > idProduct 2>/dev/null
        mkdir -p functions/rndis.usb0 2>/dev/null
        echo "02:11:22:33:44:55" > functions/rndis.usb0/host_addr 2>/dev/null
        echo "02:11:22:33:44:66" > functions/rndis.usb0/dev_addr 2>/dev/null
        mkdir -p functions/acm.usb0 2>/dev/null   # v84 调试串口
        mkdir -p configs/c.1 2>/dev/null
        ln -sf functions/rndis.usb0 configs/c.1/ 2>/dev/null
        ln -sf functions/acm.usb0 configs/c.1/ 2>/dev/null
        echo "$UDC" > UDC 2>/dev/null && log "USB gadget rndis+acm created (UDC=$UDC)"
        cd /
        sleep 2
    else
        log "WARN: 无 UDC 或 configfs（USB gadget 不可用）"
    fi
fi
if [ -e /sys/class/net/usb0 ]; then
    ip link set usb0 up 2>/dev/null
    ip addr add 172.16.42.1/24 dev usb0 2>/dev/null || \
        ifconfig usb0 172.16.42.1 netmask 255.255.255.0 up 2>/dev/null
    log "usb0 up: 172.16.42.1"
else
    log "WARN: usb0 not present (USB gadget 未配置)"
    ls /sys/class/net/ >> "$LOG_FILE" 2>/dev/null
fi
# ACM 串口调试 shell（v84：Windows 串口终端直连，不依赖 ssh）
# v85：轮询等待 ttyGS0（UDC 绑定后 tty 创建有延迟）+ 欢迎信息验证通道
TTYGS0=""
for i in 1 2 3 4 5 6 7 8 9 10; do
    [ -e /dev/ttyGS0 ] && { TTYGS0=yes; break; }
    sleep 1
done
if [ -n "$TTYGS0" ]; then
    ( /bin/sh -i < /dev/ttyGS0 > /dev/ttyGS0 2>&1 & ) 2>/dev/null
    log "ACM serial shell on /dev/ttyGS0"
    ( sleep 6; echo "[serial] AgentOS console ready" > /dev/ttyGS0 2>/dev/null ) &
else
    log "WARN: /dev/ttyGS0 不存在（ACM 串口未生效）"
    ls /dev/ttyG* >> "$LOG_FILE" 2>/dev/null
fi

# ---- 3.9 自动诊断推送（v89：独立 netcat，Buildroot busybox 无 nc）----
# v87 教训：busybox 无 nc applet → 之前 nc 推送全废（手机实测 'nc' 不存在）。
# v89：静态编译 nc 打进 /usr/bin/nc；诊断写 /root/diag.txt 后 ①主动推电脑
# ②nc -l 保底（sleep 90 保持 stdin 打开）。
NC=/usr/bin/nc
( sleep 12; {
    echo "=== feed_entropy ==="
    grep -iE "feed_entropy|CRNG" "$LOG_FILE" 2>/dev/null | tail -3
    echo "=== DROPBEAR.ERR ==="
    cat /tmp/dropbear.err 2>/dev/null | head -10
    echo "=== DROPBEAR_F.ERR ==="
    cat /tmp/dropbear_F.err 2>/dev/null | head -10
    echo "=== BOOT LOG tail 30 ==="
    tail -30 "$LOG_FILE" 2>/dev/null
    echo "=== /etc/dropbear ==="
    ls -la /etc/dropbear/ 2>&1
    echo "=== entropy ==="
    cat /proc/sys/kernel/random/entropy_avail 2>/dev/null
    echo "=== nets ==="
    ls /sys/class/net/ 2>&1
    echo "=== dmesg random/crng/dropbear ==="
    dmesg 2>/dev/null | grep -iE "random|crng|dropbear" | tail -12
    echo "===END==="
} > /root/diag.txt 2>&1
[ -x "$NC" ] && cat /root/diag.txt 2>/dev/null | "$NC" 172.16.42.2 19999 ) &
( sleep 30; { [ -x "$NC" ] && cat /root/diag.txt 2>/dev/null; sleep 90; } | "$NC" -l -p 19999 ) &
log "diag push armed (nc active + listen)"

# ---- 3.10 dropbear 错误送屏幕（v91：kmsg → dmesg → recovery 启动画面显示）----
( sleep 20; {
    echo "=== DROPBEAR.LOG ===" > /dev/kmsg
    cat /tmp/dropbear.log 2>/dev/null > /dev/kmsg
    echo "=== SEGFAULT ===" > /dev/kmsg
    dmesg 2>/dev/null | grep -iE "segfault|signal" | tail -5 > /dev/kmsg
    echo "=== DROPBEAR PROC ===" > /dev/kmsg
    pidof dropbear >> /dev/kmsg 2>/dev/null
} ) &
log "dropbear screen diag armed"

# ---- 3. SSH（dropbear，密码 1234）----
DB=""
for p in /usr/sbin/dropbear /sbin/dropbear /usr/bin/dropbear; do
    [ -x "$p" ] && DB="$p" && break
done
DBKEY=""
for p in /usr/bin/dropbearkey /usr/sbin/dropbearkey /sbin/dropbearkey; do
    [ -x "$p" ] && DBKEY="$p" && break
done
if [ -n "$DB" ] && ! pidof dropbear >/dev/null 2>&1; then
    # v83：hostkey 已预生成打进 rootfs（/etc/dropbear/dropbear_rsa_host_key，PEM）。
    # 原因：内核无 qcom 硬件 RNG → 启动早期 getrandom() 阻塞 → dropbearkey/-R 生成 hostkey 挂死。
    # 保底：hostkey 缺失时才后台 dropbearkey 生成（不阻塞启动链）。
    mkdir -p /etc/dropbear
    # v90：dropbearkey 无条件生成原生 key（ssh-keygen PEM 导致 dropbear 报
    # "early exit: string too long"；3.18 的 key 是 dropbearkey 生成的所以正常）。
    # 喂熵后 CRNG 已就绪，dropbearkey 秒完成。
    if [ -x /usr/bin/dropbearkey ]; then
        rm -f /etc/dropbear/dropbear_rsa_host_key
        /usr/bin/dropbearkey -t rsa -s 2048 -f /etc/dropbear/dropbear_rsa_host_key \
            >> /tmp/dropbearkey.log 2>&1
        log "dropbearkey gen: $(tail -1 /tmp/dropbearkey.log 2>/dev/null | tr '\n' '|')"
    fi
    # v92：dropbear daemonize（fork 父进程退出）—— 用 pidof 检查真实存活，
    # 不能用 kill -0 $DPID（父进程已退出会误判 DEAD → 起第二个实例 → 22 冲突）
    "$DB" -E -r /etc/dropbear/dropbear_rsa_host_key > /tmp/dropbear.log 2>&1 &
    sleep 3
    if pidof dropbear >/dev/null 2>&1; then
        log "dropbear alive: $(pidof dropbear | tr '\n' ' ')"
        ls -la /etc/dropbear/ >> "$LOG_FILE" 2>/dev/null
    else
        log "dropbear DEAD; log: $(head -5 /tmp/dropbear.log 2>/dev/null | tr '\n' '|')"
        # 保底：删 key 让 dropbear -R 自生成（仅确认无实例才起）
        if ! pidof dropbear >/dev/null 2>&1; then
            rm -f /etc/dropbear/dropbear_rsa_host_key
            "$DB" -E -R > /tmp/dropbear.log 2>&1 &
            sleep 3
            if pidof dropbear >/dev/null 2>&1; then
                log "dropbear -R alive: $(pidof dropbear | tr '\n' ' ')"
            else
                log "dropbear -R DEAD; log: $(head -5 /tmp/dropbear.log 2>/dev/null | tr '\n' '|')"
            fi
        fi
    fi
else
    log "WARN: dropbear not found (SKIP SSH)"
fi

# ---- 3.5 调试通道：busybox telnetd（检查 applet 是否存在，Buildroot busybox 可能没编）----
if busybox --list 2>/dev/null | grep -q telnetd; then
    busybox telnetd -l /bin/sh 2>/dev/null &
    log "telnetd started (debug channel)"
else
    log "WARN: busybox has NO telnetd applet (SKIP)"
fi

# ---- 3.6 存档 rootfs 阶段 dmesg（断网后重启可读，定位网卡消失）----
( sleep 30; dmesg > /root/dmesg_rootfs.txt 2>/dev/null ) &
( sleep 90; dmesg >> /root/dmesg_rootfs.txt 2>/dev/null; ls /sys/class/net/ >> /root/dmesg_rootfs.txt 2>/dev/null ) &
log "dmesg archive armed"
# 立即存档一次（init_mainline 跑完时系统必活着，一定能写——30s 版可能因系统早死而写不了）
dmesg > /root/dmesg_early.txt 2>/dev/null
log "dmesg early saved: $(wc -c < /root/dmesg_early.txt 2>/dev/null) bytes"
# 心跳日志：每 3 秒记录存活状态，定位系统死亡时刻
( while true; do echo "HB $(date +%s) nets=$(ls /sys/class/net/ 2>/dev/null | tr '\n' ',') db=$(pidof dropbear | wc -w) tel=$(pidof telnetd | wc -w)" >> /root/heartbeat.log; sleep 3; done ) &
log "heartbeat armed"

# ---- 3.7 WiFi 自动连接（后台，不阻塞启动）----
if [ -x /usr/bin/wifi_auto.sh ]; then
    nohup /usr/bin/wifi_auto.sh > /dev/null 2>&1 &
    log "wifi_auto started (后台)"
else
    log "WARN: wifi_auto.sh 不存在，跳过 WiFi"
fi

# ---- 3.8 音频初始化（后台，ADSP 固件加载 20-40s，不阻塞启动）----
# v52 验证逻辑：等声卡 → tinymix 扬声器/录音路由。详见 mainline/init_audio_mainline.sh
if [ -x /usr/bin/init_audio_mainline.sh ]; then
    nohup /usr/bin/init_audio_mainline.sh > /dev/null 2>&1 &
    log "audio init started (后台)"
else
    log "WARN: init_audio_mainline.sh 不存在，跳过音频"
fi

# ---- 4. 时间恢复（RTC 只读，文件持久化）----
# / 是 initramfs；持久文件必须从已挂载的 sda15 (/newroot) 读取。
TIME_FILE=/newroot/root/.last_time
[ -f "$TIME_FILE" ] || TIME_FILE=/root/.last_time
if [ -f "$TIME_FILE" ]; then
    date -s "@$(cat "$TIME_FILE")" 2>/dev/null && log "time restored from $TIME_FILE"
fi

# ---- 5. 图形显示探针（仅浏览器显示验证镜像启用）----
# /etc/browser-display-probe 只会被临时测试镜像加入；正常 v100 不存在此标记，
# 仍由 inittab respawn recovery_mainline。这里在 SSH/WiFi 已启动后 exec kmscube，
# 让它独占 DRM/KMS，避免与 framebuffer 控制台争夺 KMS master。
if [ -f /etc/browser-display-probe ]; then
    log "browser display probe: starting kmscube on /dev/dri/card0"
    echo "=== kmscube display probe ===" > /dev/kmsg 2>/dev/null
    exec /usr/bin/kmscube -D /dev/dri/card0 > /root/kmscube.log 2>&1
fi

# ---- 5. 显示：recovery_mainline 由 inittab respawn（run_recovery.sh）----

log "=== mainline init done ==="

# 服务状态存档（重启后可读，不依赖 SSH）
{
    echo "boot_status $(date +%s)"
    echo "  dropbear: $(pidof dropbear | wc -w) pid(s)"
    echo "  telnetd:  $(pidof telnetd | wc -w) pid(s)"
    echo "  nets:     $(ls /sys/class/net/ 2>/dev/null | tr '\n' ',')"
    echo "  usb0:     $(ip addr show usb0 2>/dev/null | grep 'inet ' | head -1)"
    echo "  wlan0:    $(ip addr show wlan0 2>/dev/null | grep 'inet ' | head -1)"
} > /root/boot_status.txt 2>/dev/null
log "boot_status saved"
exit 0
