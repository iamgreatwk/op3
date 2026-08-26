# OnePlus 3 boot-image format

## Reference image

The profile in `boot/oneplus3-fa5.env` was extracted from the known-good local
image `boot_fa5_v100_auto.img`.

| Field | Value |
| --- | --- |
| Reference SHA256 | `29ccd3eb8b093b29fc44435bd6e5f98367cf3794c117f9527a6bf3c1ebc5d781` |
| Image size | 62,226,432 bytes |
| Header version | 0 |
| Page size | 4096 |
| Base | `0x80000000` |
| Kernel address | `0x80008000` |
| Ramdisk address | `0x81000000` |
| Tags address | `0x80000100` |
| Kernel payload | gzip-compressed ARM64 Image followed by raw OnePlus 3 DTB |
| Ramdisk | gzip-compressed initramfs |

The extracted appended DTB identifies itself as `model = "OnePlus 3"` and is
compatible with `oneplus,oneplus3` and `qcom,msm8996`.

## Safe reuse rules

- Use `scripts/pack-boot.sh` with a built `Image.gz`,
  `msm8996-oneplus3.dtb`, and a gzip-compressed initramfs.
- The script appends the DTB to `Image.gz`, matching the reference layout.
- The old pmOS boot/root UUID parameters are retained only as reference data.
  They are not inherited by default because a new rootfs needs its own root
  selection.
- Use `BOOT_CMDLINE_OVERRIDE` only when the owner has selected the correct
  rootfs and debugging command line.
- Packaging is not flashing. The project owner alone may run fastboot or boot
  a produced image.

## Script validation

`scripts/pack-boot.sh` was validated by splitting the reference kernel payload
at its appended-DTB boundary, then repacking it with the extracted DTB and
reference gzip ramdisk. The resulting image retained the reference image size,
page size, kernel/ramdisk sizes, and load addresses. Its cmdline and image ID
intentionally differ because the old root UUIDs are not used by default.

## Example

```bash
BOOT_CMDLINE_OVERRIDE='fbcon=nodefault console=tty0 pmos.debug-shell [rootfs parameters] ' \
  ./scripts/pack-boot.sh \
  /path/to/Image.gz \
  /path/to/msm8996-oneplus3.dtb \
  /path/to/initrd.img \
  artifacts/boot-oneplus3.img
```
