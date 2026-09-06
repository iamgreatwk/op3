# OnePlus 3 boot-image format

## Reference image

The profile in `boot/oneplus3-fa5.env` was extracted from the historical
known-good v100 image. That image is provenance only; current rebuilds use the
standalone external ramdisk and separately built kernel/DTB.

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

## What is retained from the historical image

Only its gzip-compressed ramdisk is retained as the standalone external input
`$OP3_EXTERNAL_INPUTS/initrd/reference-initrd.img`, with SHA256
`c3358a1cadb747996ddaa492e636827f2d72974040e8fd40d81f8a213e676366`.
The old kernel payload, appended DTB, boot header, load addresses, and old
cmdline are not consumed by the current recovery build. The exact ramdisk
archive is retained as a whole because it is the historical userspace
baseline; selected Qualcomm, A530, and ath10k files are overlaid from their
separately pinned external inputs.

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
