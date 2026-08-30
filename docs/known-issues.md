# Known issues

- **Linux 7.2 卡 fastboot（未解决）**：7.2（torvalds mainline + 本地显示补丁）即使用 v74 DTB 也卡 fastboot。已排除：DTB、cmdline/initramfs、`CONFIG_ARM64_LSUI`、关键驱动配置。7.2 有独立的内核早期问题，需早期日志定位（见 `docs/handoff/root-cause-dtb-key.md`）。
- **主线 v6.12.1 DSI 控制镜像无 USB 网卡/ACM（实测，2026-08-30）**：`artifacts/boot-oneplus3-mainline-6121-dsi-pm-v74dtb-full-initrd.img` 能跑到 `recovery.c`，但实测没有 USB RNDIS 网卡、也没有 ACM 串口，无法取日志。凡是需要日志证据的测试，基线改用 pmOS 6.12 的 `artifacts/boot-oneplus3-pmos612-v74dtb-full-initrd.img`（USB RNDIS/ACM/Dropbear 已验证可用）。
- **Buildroot 2025.02 在本机 GCC 15.2 上构建失败（2026-08-30）**：`host-m4 1.4.19` 内置的 gnulib 与 GCC 15 默认的 `-std=gnu23` 不兼容（`GL_OSET_INLINE _GL_ATTRIBUTE_NODISCARD int` → `expected identifier or '(' before 'int'`）。改用 **2026.02.x** 分支（m4 1.4.21）。注意：本机仓库无 gcc-14 可装，不能用降级编译器绕过；备选方案是 `HOST_CFLAGS="-O2 -std=gnu17" HOST_CXXFLAGS="-O2 -std=gnu++17"`（两个都要给，因为 `HOST_CXXFLAGS` 会继承 `HOST_CFLAGS`，而 `-std=gnu17` 对 g++ 非法）。
- **本机 coreutils 是 uutils 实现，Buildroot 会拒绝（2026-08-30）**：`install/stat/dd/mkdir/ln/sort/cut/tr/wc/od/split/uniq/basename/dirname` 都指向 `/usr/lib/cargo/bin/coreutils/`，Buildroot 的 `support/dependencies/dependencies.sh` 硬检查 `install` 版本并报错（uutils coreutils 0.8.0 issue 12166）。GNU 版以 `/usr/bin/gnu<name>` 形式存在（`gnuinstall` 为 GNU coreutils 9.7）。**构建时把 GNU 版前置到 PATH**：建 `~/gnubin` 软链后 `PATH="$HOME/gnubin:$PATH" make ...`。
- **改 Buildroot 工具链 C++ 选项后必须 dirclean 编译器（2026-08-30）**：开了 `BR2_TOOLCHAIN_BUILDROOT_CXX` 后，`gcc-final-dirclean` 只清目标侧的 `gcc-final` 包，**真正的编译器是 `host-gcc-final`**（`build/host-gcc-final-14.3.0/`），它的构建目录与完成戳还在就会被整体跳过，结果 g++ 依旧缺失、mesa3d 的 meson 报 "Unknown compiler(s): aarch64-buildroot-linux-gnu-g++"。正确做法：`make host-gcc-final-dirclean gcc-final-dirclean` 后重编。
- **ACM 串口日志为空（2026-08-30，已定位并规避，根治需内核重建）**：`cat /dev/ttyACM0` 抓不到任何内核输出。三个叠加原因：
  1. **主机端**：用户 `kai` 不在 `dialout` 组，`/dev/ttyACM0` 打开直接 `Permission denied`（已用 `sudo usermod -aG dialout kai` + `setfacl` 解决，重插设备后 ACL 需重设或等组生效）。
  2. **initramfs**：pmOS full-initrd 的 `/dev/ttyGS0` 是一个**普通文件占位符**（31 字节，非字符设备），用户空间写入被 tmpfs 吞掉。已确认真实主次设备号从 `/sys/class/tty/ttyGS0/dev` 读取后 `mknod` 重建即可用。
  3. **内核**：6.12 控制内核 `CONFIG_U_SERIAL_CONSOLE is not set`，`console=ttyGS0` 永远静默无效（`/proc/consoles` 只有 tty0 和 ramoops-1），init 输出和 panic 无法到达 ACM。**根治 = 重建 pmOS 6.12 内核加 `CONFIG_U_SERIAL_CONSOLE=y`（独立任务）**。
  - 当前规避（已固化进 EGL 启动器 `boot/egl-test/sbin/run_recovery.sh` 的 `start_acm_console`）：重建字符设备节点 + `cat /dev/kmsg > /dev/ttyGS0` 阻塞转发内核日志 + 在 ttyGS0 上起一个调试 shell。新镜像 `boot-oneplus3-pmos612-v74dtb-egl-acm.img`（`65cac825…`）开机即自动生效。注意：用 `make-drm-test-initrd.sh` 打 EGL 包必须带 `OVERLAY_SOURCE="$PWD/boot/egl-test"`，漏了会静默打入 drm-test 的 RGB 启动器（2026-08-30 实际踩坑：开机变成红绿蓝序列）；验证 initramfs 内容时 `zcat` 只解第一个 gzip 成员，必须用多成员迭代解压检查 overlay 段。
  - **主机侧用法（2026-08-30 全部实测）**：
    - 抓内核日志：`cat /dev/ttyACM0`（udev 规则 `scripts/99-op3-acm.rules` 保证权限；kmsg relay 连接时会先倒出整个环形缓冲再跟随新消息）。
    - 从开机抓全量：抓取循环必须在 `fastboot boot` **之前**启动，且失败要退避，如 `while [ $(date +%s) -lt $end ]; do timeout 3 cat /dev/ttyACM0 >> log; sleep 1; done`。固定次数无退避的循环会在设备离线时瞬间烧完（cat 打开失败立即返回）——本轮实际踩坑，导致抓到 0 字节。
    - relay 启动到主机连接之间几秒的消息会丢（u_serial 无缓冲）；补看用 ACM 上的调试 shell：`exec 3<>/dev/ttyACM0; printf '\ndmesg\n' >&3`（或 screen/minicom），命令在设备上真实执行、结果从同一端口返回（已验证：发 `echo MARKER-$((40+2))` 返回 `MARKER-42`）。注意 kmsg 转发流会与 shell 输出交叉，属预期。
- **GPU runtime PM 恢复路径会让整机重启（2026-08-30，EGL 层发现，未解决）**：开机后 30 秒内由启动器运行的 kmscube 一切正常（FD530/GLES 3.1），但稍后（GPU 已 runtime suspend，`control=auto`、250ms 自动挂起）再通过 SSH 手动运行 kmscube，设备直接重启。背景：DTB 的 GPU 节点缺少 vdd/vddcx 供电，驱动打印 `supply vdd not found, using dummy regulator`，runtime resume 时没有真实的电源序列。可疑方向：给 `gpu@b00000` 补上真实的 GPU 供电节点（需要 DTB 层改动，独立任务）。当前规避：EGL 启动器开机即把 `b00000.gpu/power/control` 设为 `on`（注意：GPU 已挂起时切 `on` 会立即触发 resume，同样有风险）。
- **6.12/6.16/6.19 用自己的 DTB 卡 fastboot（未解决）**：这些内核用 6.3.1-v74full 编译的 v74 DTB 都能启动，但用各自 DTB 卡。最可疑差异是新 `qcom,rpm-proc` DTS 结构（待验证）。
- 6.19.5 之前卡 fastboot 的原因是 `CONFIG_SCSI_UFS_QCOM=m`（模块），v74 全内置配置修正为 `=y` 后解决（OP3-BOOT-037 PASS）。
- Legacy 6.3.1 findings, including A530 runtime-PM and no-preempt behavior,
  are evidence only and require new Linux 7.2 A/B reproduction.
