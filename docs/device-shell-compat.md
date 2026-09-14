# Device-side shell and environment compatibility rules

Canonical checklist for ANY command or script that runs on the OnePlus 3
recovery/browser environment (BusyBox initramfs + Buildroot bundle). Every
rule below was learned from a real incident - re-check scripts against this
list before running them. The PC and the device are DIFFERENT worlds.

## BusyBox applet gaps (v1.38, this project config)

| Need | NOT available | Use instead |
|---|---|---|
| process match/kill | pgrep/pkill | /proc cmdline scan (rule 1) |
| gz tar extract | tar -z, tar -a, `tar -x -a` | `busybox gzip -dc x.tar.gz \| busybox tar -x -C DEST` |
| per-file sizes | find -printf | find . -type f | sort | xargs sha256sum |
| clock sync | ntpd | HTTP Date header + manual parse (browser run.sh) |
| RFC1123 dates | date -s "Mon, 01 ..." | convert to YYYY-MM-DD hh:mm:ss |
| HTTP fetch | curl | busybox wget |
| telnet | telnetd | dropbear SSH (already running) |
| sftp | sftp-server | scp -O or ssh cat |

## Scripting rules (all learned from real incidents)

1. Self-kill trap: never inline a /proc/[0-9]*/cmdline process sweep in a
   remote one-liner - the remote shell own cmdline CONTAINS your match
   patterns, so it kills itself (SSH dies with 255). Push the sweep as a
   script FILE and run it; exclude /proc/$$.
2. tar extraction: the device BusyBox tar does not support GNU `-z`, and its
   `-a` behavior is not a reliable substitute for gzip extraction. ALWAYS use
   `busybox gzip -dc ARCHIVE.tar.gz | busybox tar -x -C DEST`, then verify
   afterwards - full manifest (find . -type f | sort | xargs sha256sum |
   sha256sum) or at minimum spot-check executables. Never continue after a
   tar/gzip error or silently reuse a partially extracted bundle.
3. Never extract over the live bundle: use versioned directories plus a
   symlink switch (tests/browser/deploy-bundle.sh) - extractions clobber
   runtime assets (run.sh, fonts, udev rules, helper wrappers).
4. Loopback is DOWN by default: anything binding 127.0.0.1 fails with
   EADDRNOTAVAIL. Run: ip link set lo up
5. dbus-daemon needs --config-file=<bundle>/usr/share/dbus-1/session.conf
   and must NOT get --session (they conflict); the initramfs root has no
   /usr/share/dbus-1 at all.
6. busybox date -s parses YYYY-MM-DD hh:mm:ss (use -u for UTC).
7. No RTC: clock is epoch-based; step it from an HTTP Date header before
   any TLS operation (browser run.sh does this).

## Kernel and driver rules

8. msm/MDP5 cannot be rebound at runtime: unbind leaks CTL/SMP resources
   (rebind fails -ENOSPC, mdp5_ctl.c:710 / mdp5_smp.c:84) and destroys
   fbdev. Do not hot-rebind; firmware must be inside the initramfs at boot
   probe time (~2 s), never loaded later.
9. GPU runtime PM: control=on before rendering (resume hard-reset,
   docs/known-issues.md); after the DTB regulator fix, restore control=auto
   when the session ends so the GPU re-suspends.
10. v100 (6.3.1) caveat: ~450 GPU SMMU context faults per minute under
    browser load - cosmetic, do not chase in test logs.

## Buildroot traps (host side)

11. Package sub-option changes do NOT rebuild an already-built package; use
    make <pkg>-dirclean for a consistent build. wpewebkit-reconfigure
    produced a mixed-object library whose renderer crashed on heavy pages.
12. BR2_JLEVEL must be set in the config (defconfig has it); buildroot
    ignores a command-line -j and defaults to nproc+1 (13 here) - OOM.
    Swap must be ~20 GB for wpewebkit.
13. defconfig exists in TWO places: repo buildroot/*.defconfig and
    source/buildroot/configs/. make reads ONLY source/buildroot/configs/ -
    a stale copy there silently applies an old configuration.
14. WebKit binary naming: WPEWebDriver (WebKitWebDriver is the GTK name).

## Remote-access rules (host to device)

15. Host key changes on every image switch: ssh-keygen -R <ip> first, then
    reconnect with -o StrictHostKeyChecking=accept-new.
16. scp needs -O (no sftp-server on the device).
17. Multi-line heredocs/inline python over the agent tool channel have been
    mangled before - prefer script files transferred with scp.
18. weston: custom weston.ini must be passed with -c/--config on the command
    line; main() ignores the WESTON_CONFIG_FILE env var during discovery
    (frontend/main.c load_configuration). Output rotation for the portrait
    DSI-1 panel: [output] name=DSI-1 transform=rotate-90/270 = landscape
    (1920x1080); rotate-180 = upside-down portrait.

## Camera temporary-module checklist

The recovery initramfs and the Buildroot rootfs are separate filesystems. A
camera `.ko` copied below `/newroot/tmp` is not loaded merely because it is
present, and `modprobe` is only valid when the matching running-kernel
`modules.dep` is available. For a temporary camera experiment:

1. After every `fastboot boot` image change, remove the old host key before
   SSH/scp reconnect:

   ```sh
   ssh-keygen -f "$HOME/.ssh/known_hosts" -R 172.16.42.1
   scp -O bundle.tar.gz root@172.16.42.1:/newroot/tmp/
   ```

2. Extract the gzip archive with the device-compatible pipeline, never with
   `tar -x -a` or `tar -xzf`:

   ```sh
   ssh root@172.16.42.1 \
     'busybox gzip -dc /newroot/tmp/bundle.tar.gz | busybox tar -x -C /newroot'
   ```

   Check the archive and each selected module SHA256 before loading it.

3. A clean camera test must explicitly load this complete dependency chain
   (using the exact modules built for the running 6.12.1 kernel):

   ```text
   mc
   videodev
   v4l2-async
   v4l2-fwnode
   videobuf2-common
   videobuf2-memops
   videobuf2-dma-sg
   videobuf2-v4l2
   i2c-qcom-cci
   qcom-camss
   bu63165gwl
   imx298
   ```

   The repository helper `scripts/op3-camera-load-modules.sh` implements this
   exact ordered absolute-path `insmod` sequence and prints every module SHA.
   Do not use a bare `modprobe` from an unknown `modules.dep`. Verify every
   return code, then verify `/sys/class/video4linux`, `/dev/media0`,
   `/dev/video*`, and the two named lens/sensor subdevices. If the loaded set
   or module provenance is uncertain, reboot through the same clean
   `fastboot boot` path; do not unload CAMSS/CCI to recover a failed
   experiment.

4. For autofocus, the VCM position command is valid only while the IMX298
   sensor is powered and the same continuous `STREAMON` is producing frames.
   Opening `bu63165gwl` alone is not a valid communication test and previously
   returned CCI `-ETIMEDOUT`.
