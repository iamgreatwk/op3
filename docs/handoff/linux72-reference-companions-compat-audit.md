# Linux 7.2 / reference initramfs and v74 DTB compatibility audit

```text
Task / GitHub Issue: Owner-authorized no-Issue read-only companion audit
Role: Research / review
Baseline commit: Linux v7.2 (`8d3ae59288f1e7d58d76558a6ee96d533bc5019f`)
Working branch: agent/implementation/s6e3fa5-linux72-port
Changed files: docs/handoff/linux72-reference-companions-compat-audit.md
Commit SHA: pending

Layer: Boot companions / userspace compatibility audit only
Hypothesis tested: The reference initramfs or v74 DTB itself causes Linux 7.2
to return to fastboot before a usable early userspace is reached.
Only variable changed: none; no image, kernel, DTB, initramfs or config changed.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: none created

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: fixed companion inputs used by OP3-BOOT-038/039
in docs/handoff/latest.md
```

## Inputs inspected

- Reference initramfs: `artifacts/reference-initrd.img`, gzip cpio archive,
  SHA256 `c3358a1cadb747996ddaa492e636827f2d72974040e8fd40d81f8a213e676366`.
- Fixed v74 DTB:
  `out/pmos-msm8996-6.3.1-v74full/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb`,
  SHA256 `463b2c7203e28359ce4039c8e4aa9b0d211171879db8bbea07834d3cb8b2bde3`.
- Linux 7.2 strict-config DTB only for static comparison, SHA256
  `87963b9340d437abb6bfe387e05327bcb24766e81d515df9513ce0da1a4eea45`.

## Initramfs findings

The archive's root `/init` is a shell script. It mounts proc/sys/devtmpfs and
then attempts `mount -t ext4 /dev/sda15 /newroot`. If `/newroot/sbin/init`
exists, it executes `switch_root /newroot /sbin/init`; otherwise it falls back
to `/sbin/init` in the archive.

It does **not** parse `pmos_root_uuid`, `pmos_boot_uuid`, or any other kernel
command-line UUID. The current UUID-free boot profile is therefore not blocked
by a missing legacy UUID. `/init` also contains no explicit reboot, poweroff,
or fastboot action.

After regular init, the non-3.18 branch selected by `sbin/init_choose.sh` runs
`sbin/init_mainline.sh`. That script is deliberately labelled for 6.x but uses
the catch-all branch for every non-3.18 release, including 7.2. It configures
the USB gadget, network/debug services and recovery only **after** PID 1 is
running. It can affect post-boot observability, but it is not executed before
kernel entry, `__cpu_setup`, MMU enablement, DTB unflattening, UFS discovery or
the first root-init handoff.

The archive contains `/lib/modules/6.18.7`, while the tested strict Linux 7.2
build reports `7.2.0-msm8996+`. Those modules must not be loaded into 7.2 and
make this archive unsuitable as a final 7.2 rootfs. There are no
`/etc/modules-load.d/*.conf` files in this archive, so its included
`S11modules` script has no configured module to load by itself. A module
mismatch can still break later optional features if a persistent rootfs or a
manual action requests modules; it cannot explain an early return before PID 1
without additional evidence.

The archive includes Qualcomm runtime firmware (`a530_zap.mbn`, ADSP, MBA,
modem, SLPI and Venus) and ath10k firmware. Firmware requests occur after the
corresponding drivers probe; they do not repair a failure before kernel entry.

## v74 DTB findings

The v74 DTB identifies `oneplus,oneplus3` / `qcom,msm8996`, has the expected
OnePlus board/MSM IDs, selects `serial1:115200n8`, and carries
`samsung,s6e3fa5`. Linux 7.2 contains matching support for the boot-critical
compatibles present in that DTB:

```text
qcom,glink-rpm       drivers/rpmsg/qcom_glink_rpm.c
qcom,rpm-msm8996     drivers/soc/qcom/smd-rpm.c
qcom,smem            platform population / qcom SMEM support
qcom,gcc-msm8996     drivers/clk/qcom/gcc-msm8996.c
qcom,msm8996-ufshc   qcom UFS binding/driver support
samsung,s6e3fa5      locally forward-ported panel driver
```

The principal compatible-set differences are non-boot peripherals. v74 has
`fusb301`, `pmi8994-haptics`, `pmi8996-fg`, `pmi8996-smbchg` and
`spmi-haptics`; those names are not present in the Linux 7.2 driver tree. The
7.2 DTB instead adds `qcom,msm8996-rpm-proc`, `qcom,rpm-proc`,
`qcom,glink-smd-rpm` and `coresight-remote-etm`. Absence of the v74 peripheral
drivers can leave charging/type-C/haptics unavailable, but static review does
not make them prerequisites for UFS root access or PID 1.

Thus v74 is an intentionally legacy DTB and cannot be promoted as the final
7.2 hardware description. However, its required RPM/SMEM/GCC/UFS/panel
compatibles are accepted by 7.2; no unsupported binding was found that would
by itself make the bootloader return to fastboot.

## Conclusion

**REJECTED as the primary explanation for the present fastboot result.** The
reference initramfs may cause a later user-space or module failure and must be
replaced by a 7.2-matched Buildroot/initramfs solution after kernel bring-up.
The v74 DTB may cause later device-probe gaps and also must not become the
formal DTS baseline. Neither contains a demonstrated pre-PID1 fastboot path,
and the existing 7.2 failure with both companions fixed remains an early-kernel
or early-config investigation.

The next owner build remains the already prepared, single-variable no-MTE A/B;
do not change these companions in that test.
