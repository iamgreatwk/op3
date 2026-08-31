# OP3-WIFI-001 — QCA6174 Wi-Fi integration

This directory replaces the legacy 6.3.1 Wi-Fi module path with the exact
modules produced for the pinned pmOS 6.12.1 kernel. The QCA6174 firmware
already belongs in the baseline initramfs; this task keeps it there and loads
the matching modules only after `/newroot` is mounted.

The initramfs BusyBox `modprobe` has no `-d` option, so `wifi-start` uses
absolute `insmod` paths in the dependency order recorded by this bundle. This
does not rely on the stale `/lib/modules/6.18.7` tree in the initramfs.

The CLI writes a quoted WPA passphrase directly into the device-local profile;
`wpa_supplicant` derives the PSK. It deliberately does not invoke
`wpa_passphrase`, because an older `/newroot` binary may be dynamically linked
against a library absent from the initramfs runtime.

Before every connection attempt, the CLI terminates any earlier instance and
removes its own stale `/run/op3-wifi/wlan0` socket. This makes a failed first
association retryable without manual socket cleanup.

The CLI invokes the static initramfs `/usr/sbin/wpa_supplicant` and
`/usr/sbin/wpa_cli` explicitly. `wpa_cli` is also given the matching custom
control socket directory `/run/op3-wifi`; otherwise it looks in its default
directory and cannot observe an already-completed association.

## Persistent layout

The owner stages `op3-wifi-bundle.tar.gz` into `/newroot`. It supplies:

```text
/newroot/opt/op3-wifi/wifi
/newroot/opt/op3-wifi/wifi-start
/newroot/lib/modules/$(uname -r)/...
/newroot/usr/bin/wifi -> /opt/op3-wifi/wifi
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
   make -C source/linux-pmos-msm8996-6.12 O="$PWD/out/pmos-msm8996-6.12-rpm-glink-own-dtb" \
     ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 modules
   make -C source/linux-pmos-msm8996-6.12 O="$PWD/out/pmos-msm8996-6.12-rpm-glink-own-dtb" \
     ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
     INSTALL_MOD_PATH="$PWD/artifacts/op3-wifi-modules-root" modules_install
   ```

2. Generate and deploy the bundle (no credential is included):

   ```sh
   scripts/stage-op3-wifi-rootfs.sh artifacts/op3-wifi-modules-root artifacts/op3-wifi-bundle.tar.gz
   scp artifacts/op3-wifi-bundle.tar.gz root@172.16.42.1:/newroot/tmp/
   ssh root@172.16.42.1 'tar -xzf /newroot/tmp/op3-wifi-bundle.tar.gz -C /newroot'
   ```

3. Generate the small Wi-Fi overlay archive, pack it with the fixed
   OP3-BOOT-044 Image.gz, DTB, cmdline, and boot profile, then perform the
   owner-run `fastboot boot` test:

   ```sh
   scripts/make-op3-wifi-initrd.sh \
     artifacts/initrd-op3-firmware-provenance-v2.cpio.gz \
     artifacts/initrd-op3-wifi.cpio.gz
   ```

Collect `/root/wifi_auto.log`, `dmesg | grep -iE 'ath10k|wlan|firmware'`,
`wifi current`, and the Wi-Fi SSH result. Do not copy credentials into logs.
