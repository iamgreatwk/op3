# OnePlus 3 Linux 7.2 early pstore/ramoops diagnostic

```text
Task / GitHub Issue: Owner-authorized early-boot persistent-log diagnostic; no GitHub Issue supplied
Role: Implementation agent
Baseline commit: d73a640c6af7ad461bce8f54967c0abdf44d1204 (Linux v7.2-3-gd73a640c6)
Working branch: agent/implementation/s6e3fa5-linux72-port
Changed files: kernel/configs/oneplus3-early-pstore.fragment; docs/handoff/linux72-early-pstore.md; docs/test-matrix.md
Configuration commit: e58bad1d51d82e4d9842340ebf4f4cd4e524fe7e

Layer: Kernel configuration / early-boot observability
Hypothesis tested: Linux 7.2 fails after ramoops can initialize, and its console
or panic record can be recovered after the failed boot through a known-good
v100 boot.
Only variable changed: the diagnostic config fragment changes
`CONFIG_PSTORE_RAM` from module to built-in and enables `CONFIG_PSTORE_CONSOLE`.
The Linux 7.2 source, DTB, panel, DRM/MSM, GPU, PM, USB configuration, boot
parameters, and initramfs are unchanged.

Evidence for the change:
- Linux 7.2 DTB already contains `ramoops@ac000000`, 2 MiB, with 128 KiB
  crash records, 1 MiB console log, and 512 KiB pmsg storage.
- Current 7.2 config has `CONFIG_PSTORE=y` but `CONFIG_PSTORE_RAM=m`, so the
  ramoops driver cannot run before initramfs module loading.
- The known-good v100 kernel has `CONFIG_PSTORE_RAM=y` and
  `CONFIG_PSTORE_CONSOLE=y`.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Required config fragments, in this order:
1. `kernel/configs/oneplus3-s6e3fa5.fragment`
2. `kernel/configs/oneplus3-early-pstore.fragment`

Required owner build verification:
`CONFIG_PSTORE=y`, `CONFIG_PSTORE_RAM=y`, and `CONFIG_PSTORE_CONSOLE=y`.

Owner build command (not run by this agent):
```bash
project=/home/kai/src/oneplus3-mainline
kernel="$project/source/linux-7.2"
output="$project/out/linux-7.2-oneplus3-s6e3fa5-pstore"

mkdir -p "$output" && \
make -C "$kernel" O="$output" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 defconfig && \
"$kernel/scripts/kconfig/merge_config.sh" -m -O "$output" "$output/.config" \
  "$project/kernel/configs/oneplus3-s6e3fa5.fragment" \
  "$project/kernel/configs/oneplus3-early-pstore.fragment" && \
make -C "$kernel" O="$output" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 olddefconfig && \
grep -qx 'CONFIG_PSTORE=y' "$output/.config" && \
grep -qx 'CONFIG_PSTORE_RAM=y' "$output/.config" && \
grep -qx 'CONFIG_PSTORE_CONSOLE=y' "$output/.config" && \
make -C "$kernel" O="$output" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  -j"$(nproc)" Image.gz qcom/msm8996-oneplus3.dtb
```

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence collection after the failed Linux 7.2 `fastboot boot`: boot the known
working v100 image, then on the device run:
`mount -t pstore pstore /sys/fs/pstore 2>/dev/null || true; ls -al /sys/fs/pstore; cat /sys/fs/pstore/console-ramoops-* /sys/fs/pstore/dmesg-ramoops-* 2>/dev/null`

Conclusion: INCONCLUSIVE pending owner build and device test.
Uncertainties: ramoops can only capture execution after the Linux kernel maps
the reserved-memory node. An earlier bootloader/decompressor failure produces
no pstore record. The v100 boot must not erase the preserved region before the
owner collects it.
Recommended next experiment: owner builds only this config delta, packages it
with the existing Linux 7.2 DTB and v56 debug initramfs, runs `fastboot boot`,
then boots v100 and retrieves pstore before any further Linux 7.2 attempt.
```
