# OP3-WIFI-001 — QCA6174 Wi-Fi integration

This directory replaces the legacy 6.3.1 Wi-Fi module path with the exact
modules produced for the pinned pmOS 6.12.1 kernel. The QCA6174 firmware
already belongs in the baseline initramfs; Buildroot's recovery post-build
hook installs the matching module closure and these scripts into the
persistent recovery target. The initramfs loads the modules only after
`/newroot` is mounted.

The initramfs BusyBox `modprobe` has no `-d` option, so `wifi-start` uses
absolute `insmod` paths in the dependency order installed by the Buildroot
post-build hook. This does not rely on the stale `/lib/modules/6.18.7` tree in
the initramfs.

The CLI writes a quoted WPA passphrase directly into the device-local profile;
`wpa_supplicant` derives the PSK. It deliberately does not invoke
`wpa_passphrase`, because an older `/newroot` binary may be dynamically linked
against a library absent from the initramfs runtime.

Before every connection attempt, the CLI terminates any earlier instance and
removes its own stale `/run/op3-wifi/wlan0` socket. This makes a failed first
association retryable without manual socket cleanup.

The CLI prefers the Buildroot `/newroot/usr/sbin/wpa_supplicant` and
`/newroot/usr/sbin/wpa_cli`, invoking them through the Buildroot dynamic loader;
older images fall back to the static initramfs copies. `wpa_cli` is also given
the matching custom control socket directory `/run/op3-wifi`; otherwise it
looks in its default directory and cannot observe an already-completed
association.

Recovery keeps IPv6 disabled by default. Use `wifi ipv6 status` to inspect the
policy, `wifi ipv6 on` to enable IPv6 on demand, and `wifi ipv6 off` to disable
it again. The setting applies to current interfaces and to interfaces created
later; it does not change the saved Wi-Fi profile or the IPv4 DHCP path.

The association wait is 180 seconds by default. This covers the observed cold
QCA6174 startup where the first successful authentication occurred about 160
seconds after the driver was initialized. Set `OP3_WIFI_ASSOC_TIMEOUT` only
for a controlled diagnostic override.

After association, recovery starts `udhcpc` in background retry mode and
observes the interface for 30 seconds. This preserves the historical recovery
behavior when the AP is associated before its first DHCP exchange succeeds;
`OP3_WIFI_DHCP_WAIT` is available only for controlled diagnostics.

## Persistent layout

The default recovery Buildroot target supplies:

```text
/newroot/opt/op3-wifi/wifi
/newroot/opt/op3-wifi/wifi-start
/newroot/lib/modules/$(uname -r)/...
/newroot/usr/bin/wifi -> /opt/op3-wifi/wifi
/newroot/usr/sbin/wpa_supplicant
/newroot/usr/sbin/wpa_cli
/newroot/usr/sbin/iw
/newroot/lib/firmware/regulatory.db
```

The initramfs overlay only replaces `/usr/bin/wifi_auto.sh`. The established
`init_mainline.sh` launches it after `/newroot` is mounted, so it calls the
persistent CLI without changing the known-good RNDIS/ACM/SSH boot chain.

## Local credential provisioning

Do this only on the device, over the already verified USB RNDIS/SSH link:

```sh
wifi connect '<SSID>' '<passphrase>'
# or: wifi connect '<SSID>' '<password>' '<PEAP-identity>'
wifi current
```

`connect` makes that profile the boot default. Profiles are stored under
`/newroot/etc/op3-wifi/` with mode 0600; they are not shipped in the bundle.
Use `wifi list`, `wifi reconnect <SSID>`, `wifi default <SSID>`,
`wifi forget <SSID>`, `wifi disconnect`, `wifi on`, `wifi off`, and
`wifi portal` for the legacy command workflow. `portal` opens the standard
HTTP connectivity check in `links`, which follows a captive-portal redirect.
Passwords are intentionally never displayed.

## Owner-run preparation and test

1. Build the pinned kernel's modules and install them to a clean staging root.
   This is owner-only because it requires a kernel build:

   ```sh
   make -C source/linux-pmos-msm8996-6.12-recovery-audio-full \
     O="$PWD/out/pmos-msm8996-6.12-recovery-audio-full-s1302-retry" \
     ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 modules
   make -C source/linux-pmos-msm8996-6.12-recovery-audio-full \
     O="$PWD/out/pmos-msm8996-6.12-recovery-audio-full-s1302-retry" \
     ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
     INSTALL_MOD_PATH="$PWD/artifacts/op3-wifi-modules-root" modules_install
   ```

2. Build the recovery Buildroot target with Wi-Fi integrated (no credential is
   included):

   ```sh
   OP3_WIFI_MODULES_ROOT="$PWD/artifacts/op3-wifi-modules-root" \
     make -C source/buildroot O="$PWD/out/buildroot-op3-recovery" \
     BR2_JLEVEL=3
   ```

   `scripts/stage-op3-audio-rootfs.sh` then packages the same Buildroot
   target; it checks that the Wi-Fi userspace and ath10k module are present, so
   the resulting persistent recovery payload contains both Wi-Fi and audio.
   `scripts/stage-op3-wifi-rootfs.sh` is retained only for older images.

3. Generate the small Wi-Fi overlay archive, pack it with the fixed
   OP3-BOOT-044 Image.gz, DTB, cmdline, and boot profile, then perform the
   owner-run `fastboot boot` test:

   ```sh
   scripts/make-op3-wifi-initrd.sh \
     artifacts/initrd-op3-firmware-provenance-v2.cpio.gz \
     artifacts/initrd-op3-wifi.cpio.gz
   ```

Collect `/root/boot_mainline.log` (including the `wifi_auto started` marker),
`/root/dmesg_early.txt` or `/root/dmesg_rootfs.txt`,
`dmesg | grep -iE 'ath10k|wlan|firmware|rfkill'`, `wifi current`, and the
Wi-Fi SSH result. Do not copy credentials into logs.
