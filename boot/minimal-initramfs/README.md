# OnePlus 3 minimal initramfs

`init.c` is a temporary, static ARM64 PID 1 for the Linux 7.2 OnePlus 3 boot
diagnostic. It does not mount a root filesystem and does not use any pmOS root
UUID. It mounts `devtmpfs`, procfs and sysfs when available, emits an
`op3-minimal-init` marker to `/dev/kmsg`, then remains alive.

Create the ignored local test artifact with:

```bash
project=/home/kai/src/oneplus3-mainline
stage=$(mktemp -d /tmp/op3-minimal-initramfs.XXXXXX)
archive=$(mktemp /tmp/op3-minimal-initramfs.cpio.XXXXXX)
aarch64-linux-gnu-gcc-11 -static -Os -Wall -Wextra -Werror \
  -o "$project/artifacts/op3-minimal-init" \
  "$project/boot/minimal-initramfs/init.c"
install -D -m 0755 "$project/artifacts/op3-minimal-init" "$stage/init"
(cd "$stage" && find . -print0 | cpio --null -o -H newc -F "$archive")
gzip -n -f "$archive"
install -m 0644 "$archive.gz" "$project/artifacts/initrd-op3-minimal.cpio.gz"
```

Use the resulting `initrd-op3-minimal.cpio.gz` as the third argument to
`scripts/pack-boot.sh`.

This is not a Buildroot replacement and provides no interactive shell or
display userspace. Its only test variable is whether the temporary legacy
initrd was causing the device to return to fastboot.
