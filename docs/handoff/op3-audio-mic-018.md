# OP3-AUDIO-018 — effective `NO_REWINDS` A/B

## Scope

This is a kernel-only audio capture test on the pmOS MSM8996 Linux 6.12.1
baseline. The sole changed variable is one capability bit:
`SNDRV_PCM_INFO_NO_REWINDS` is added to
`q6asm_dai_hardware_playback.info` in commit `57b9a78bdc2e`.

The choice of the playback descriptor is intentional. In the current 6.12
driver, `q6asm_dai_open()` first selects a direction-specific `runtime->hw`,
then unconditionally calls `snd_soc_set_runtime_hwparams()` with
`q6asm_dai_hardware_playback`. The capture-only flag changes tested in
OP3-AUDIO-012 and OP3-AUDIO-017 therefore never reached the active capture
runtime. The supplied 6.3.1 source also put `NO_REWINDS` on the playback
descriptor, which is the historical clue being isolated here.

## Evidence motivating the test

OP3-AUDIO-016 showed the first 3,840-frame ring progressing normally, then
`appl_ptr` being reset from 3,840 to zero before each transfer while
`status->hw_ptr` stayed at 3,840. This made ALSA report a full buffer and
TinyALSA copy it repeatedly. On ARM64, the non-coherent architecture path
disables status/control mmap regardless of `SYNC_APPLPTR`, so TinyALSA's
private `SNDRV_PCM_SYNC_PTR` record remains in use. OP3-AUDIO-017 removed
`SYNC_APPLPTR` from the capture descriptor, but the owner observed the same
0.17-second, 241,920-frame result because that descriptor was overwritten.

With `NO_REWINDS` on the effective runtime, `pcm_lib_apply_appl_ptr()` should
reject the stale backwards private-sync value instead of changing the kernel's
progressed application pointer. TinyALSA ignores the `SYNC_PTR` error in its
state-refresh path and continues with `READI`, so the next transfer should
observe the monotonic pointer.

## Owner build and device test

The owner must run the build and fastboot test; the agent does not compile or
flash. Starting from the kernel worktree at commit `57b9a78bdc2e`:

```sh
cd /home/kai/src/oneplus3-mainline

make -C /home/kai/src/oneplus3-audio-dts-001 \
  O=/home/kai/src/oneplus3-mainline/out/linux-pmos-msm8996-6.12-defconfig-audio \
  ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- \
  CC=aarch64-linux-gnu-gcc-11 \
  CONFIG_EXTRA_FIRMWARE_DIR=/home/kai/src/oneplus3-mainline/source/linux-pmos-msm8996-6.12/extfw \
  -j"$(nproc)" Image.gz

./scripts/pack-boot.sh \
  out/linux-pmos-msm8996-6.12-defconfig-audio/arch/arm64/boot/Image.gz \
  out/linux-pmos-msm8996-6.12-defconfig-audio/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb \
  artifacts/initrd-op3-audio-tx-hold-routefix-diagnostic.cpio.gz \
  artifacts/boot-oneplus3-pmos612-own-dtb-audio-effective-norewinds-diagnostic.img
```

After a clean transient boot, use the already verified route helper and the
MM1 capture front end:

```sh
mount | grep -q ' /sys/kernel/debug ' || mount -t debugfs none /sys/kernel/debug
/opt/op3-audio/route.sh mic
tinymix -D 0 set "MultiMedia3 Mixer SLIMBUS_0_TX" 0
tinymix -D 0 set "MultiMedia1 Mixer SLIMBUS_0_TX" 1

echo "before: $(cat /proc/uptime)"
tinycap /newroot/var/log/op3-amic4-effective-norewinds.wav \
  -D 0 -d 0 -c 1 -r 48000 -b 16 -p 480 -n 8 -t 5
echo "after:  $(cat /proc/uptime)"
ls -lh /newroot/var/log/op3-amic4-effective-norewinds.wav

dmesg | grep -E 'OP3 capture core|capture (READ_DONE|READ_SUBMIT|PERIOD|POINTER)'
```

Collect the first and a later 4 KiB payload hash as well. PASS for this A/B
requires approximately five seconds of elapsed uptime, approximately 240,000
reported frames, no repeated core `XFER` entries with `avail=3840`, and
period-paced READ_DONE events. A PASS here only fixes the ALSA accounting
cadence; microphone payload non-silence and non-repetition remain a separate
acceptance criterion.

## Archive

`patches/pmos612-op3-audio/0014-ASoC-q6asm-enforce-no-rewinds-on-effective-runtime.patch`

The archive is generated from the single source commit and passes the
one-line `git diff HEAD^ HEAD | checkpatch` check with zero errors and warnings.
No device test has occurred yet.
