# OnePlus 3 Linux 7.2 USB ACM diagnostic-console handoff

## Scope and hypothesis

This is an early-boot observability task, separate from the S6E3FA5 port.
The sole variable is a built-in USB CDC ACM gadget and a diagnostic initramfs.
The hypothesis is that, if Linux 7.2 reaches PID 1, the host will enumerate a
CDC ACM device and receive `/dev/kmsg` plus a small interactive console.

No panel, DRM/MSM, Adreno, PM, IOMMU, DTS, regulator, or clock source change
is included.

## Required kernel configuration

Apply this fragment in addition to `oneplus3-s6e3fa5.fragment`:

`kernel/configs/oneplus3-usb-acm-debug.fragment`

It sets only the parent gadget symbols that were modules in the owner build:

```text
CONFIG_USB_GADGET=y
CONFIG_USB_LIBCOMPOSITE=y
CONFIG_USB_CONFIGFS=y
CONFIG_USB_CONFIGFS_ACM=y
```

`USB_DWC3=y`, `USB_DWC3_QCOM=y`, `USB_DWC3_DUAL_ROLE=y`,
`USB_GADGET=y`, and `CONFIGFS_FS=y` were already built in. ACM selects its
`USB_F_ACM` and `USB_U_SERIAL` implementation. No USB host `CONFIG_USB_ACM`
setting is required on the phone.

## Diagnostic init

- Source: `boot/usb-acm-debug/init.c`
- Static compiler: `aarch64-linux-gnu-gcc-11 -static`
- Validated artifact (ignored): `artifacts/op3-usb-acm-debug-init`
- SHA256: `66df1b52d7a7ba48e3419d12797b267136e4820c953e3fa683736a6a144b913f`

PID 1 mounts devtmpfs, procfs, sysfs and configfs; creates one configfs ACM
function; binds the first UDC; waits for `/dev/ttyGS0`; then streams
`/dev/kmsg` and accepts the limited `help`, `cmdline`, and `udc` commands.
It does not mount a root filesystem, reuse a pmOS UUID, or carry pmOS shared
libraries. It is a diagnostic console, not a full BusyBox shell.

## Owner build boundary

The agent did not build the kernel, make an initramfs, pack a boot image, run
fastboot, or operate the device. The existing `Image.gz` cannot test this
console because its `USB_CONFIGFS` and `USB_LIBCOMPOSITE` are modules.

After an owner kernel build, merge both fragments and verify:

```bash
grep -qx 'CONFIG_USB_GADGET=y' "$output/.config"
grep -qx 'CONFIG_USB_LIBCOMPOSITE=y' "$output/.config"
grep -qx 'CONFIG_USB_CONFIGFS=y' "$output/.config"
grep -qx 'CONFIG_USB_CONFIGFS_ACM=y' "$output/.config"
```

## PASS/FAIL

- PASS: after the owner boots the resulting image, the host enumerates a CDC
  ACM device and receives the `op3 Linux 7.2 USB ACM diagnostic console`
  banner or kernel log lines.
- FAIL: no CDC ACM device appears. This is evidence only that the kernel did
  not reach gadget setup (or that the USB gadget path failed); it is not panel
  or GPU evidence.

## Remaining requirement

A formal interactive `/bin/sh` requires a static ARM64 shell, normally from
the future Buildroot output. The reference pmOS BusyBox is dynamically linked,
so it was deliberately not reused in this rootfs-free initramfs.
