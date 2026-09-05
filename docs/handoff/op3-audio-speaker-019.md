# OP3-AUDIO-019 — QUAT MI2S speaker clock-provider/IBIT

## Scope

This is a kernel-only speaker-output experiment on the pmOS MSM8996 Linux
6.12.1 baseline. The only source file changed is
`sound/soc/qcom/apq8096.c`. DTS topology, the `route.sh` mixer helper,
`tfa989x.c`, q6asm capture changes, firmware, initramfs and the playback test
fixture remain fixed.

The OnePlus 3 speaker path already enumerates as card 0/device 5
(`Speaker tfa989x-hifi-5`) and is wired by the DTS as
`MultiMedia3 -> q6routing -> QUATERNARY_MI2S_RX -> tfa9890_amp`. Before this
task, `route.sh speaker` only enabled the observed
`QUAT_MI2S_RX Audio Mixer MultiMedia3` control; `tinyplay` then printed
`error playing sample`.

## Agent handoff

```text
Task / GitHub Issue: OP3-AUDIO-019
Role: Implementation agent
Baseline commit: 67b0bbc3cbf46bae712a2606a43361756fcbd829
Working branch: agent/implementation/op3-audio-mic-001
Changed files: sound/soc/qcom/apq8096.c
Commit SHA: 6901bbd86f7b6de14af36b170196ed403ef52e2e

Layer: audio kernel / ASoC machine-driver clock setup
Hypothesis tested: the TFA9890 is silent because the QUAT MI2S AFE backend
  leaves BCLK/LRCLK ownership external and never enables its IBIT clock.
Only variable changed: QUAT MI2S clock-provider format plus the
  Q6AFE_LPASS_CLK_ID_QUAD_MI2S_IBIT enable/disable calls in apq8096.c.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: pending owner build

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: pending owner report

Conclusion: INCONCLUSIVE
Uncertainties: no 6.12 device result yet; tinyplay prints an error but its
  current implementation can still return zero after a failed pcm_writei, so
  the text output and acoustic result must be checked separately.
Recommended next experiment: build this commit and test a known 48 kHz,
  S16_LE, stereo WAV on card 0/device 2. If the backend starts, the IBIT
  clock error is absent, and the owner hears the phone speaker, record the
  TFA9890 clock-status result if available. If it remains silent or fails,
  keep this commit as rejected evidence and isolate TFA989x runtime
  configuration as a separate task.
```

## Why this variable

The preserved 6.3.1 source audit and v23 patch are explicit: the downstream
speaker backend enables `Q6AFE_LPASS_CLK_ID_QUAD_MI2S_IBIT` at
`48,000 * 16 * 2 = 1,536,000` Hz and makes the DSP the BCLK/LRCLK provider.
The same audit recorded TFA9890 status `0x0b5d` with `NOCLK` set before v23,
then `0x089d` with `NOCLK` clear during v23 playback, together with audible
phone output. Current 6.12 `q6afe-dai.c` implements both `set_fmt` and the
matching clock-ID `set_sysclk` operation, but `apq8096.c` did not call them
for the QUAT backend.

This port keeps the two calls under one falsifiable clock hypothesis:

* `apq8096_init()` sets `SND_SOC_DAIFMT_BP_FP` for the QUAT CPU DAI, so
  `q6afe_i2s_port_prepare()` selects the internal WS source.
* The backend `startup` enables the 1.536 MHz QUAT IBIT clock.
* The backend `shutdown` disables that clock.

No claim of acceptance is made until the owner performs the device test.

## Owner build and package commands

The owner must build and boot the committed kernel; the agent does not run a
large kernel build or flash the phone. The source worktree must report
`6901bbd86f7b6de14af36b170196ed403ef52e2e` before building:

```sh
cd /home/kai/src/oneplus3-mainline
git -C /home/kai/src/oneplus3-audio-dts-001 rev-parse HEAD

make -C /home/kai/src/oneplus3-audio-dts-001 \
  O=/home/kai/src/oneplus3-mainline/out/linux-pmos-msm8996-6.12-defconfig-audio \
  ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- \
  CC=aarch64-linux-gnu-gcc-11 \
  CONFIG_EXTRA_FIRMWARE_DIR=/home/kai/src/oneplus3-mainline/source/linux-pmos-msm8996-6.12/extfw \
  -j"$(nproc)" Image.gz

sha256sum \
  out/linux-pmos-msm8996-6.12-defconfig-audio/arch/arm64/boot/Image.gz

./scripts/pack-boot.sh \
  out/linux-pmos-msm8996-6.12-defconfig-audio/arch/arm64/boot/Image.gz \
  out/linux-pmos-msm8996-6.12-defconfig-audio/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb \
  artifacts/initrd-op3-audio-tx-hold-routefix-diagnostic.cpio.gz \
  artifacts/boot-oneplus3-pmos612-own-dtb-audio-speaker-clock-diagnostic.img

sha256sum \
  artifacts/boot-oneplus3-pmos612-own-dtb-audio-speaker-clock-diagnostic.img
```

The DTB and initramfs are deliberately reused from the last microphone
checkpoint; rebuilding them would introduce a second variable.

## Owner device test

Use a known 48 kHz, S16_LE, **stereo** WAV. A WAV made by the mono capture
path may need to be converted to stereo on the host before copying it to
`/newroot/var/log/`; the Quaternary backend is fixed to two channels. Then,
after `fastboot boot` and SSH login:

```sh
mount | grep -q ' /sys/kernel/debug ' || mount -t debugfs none /sys/kernel/debug

/opt/op3-audio/route.sh speaker
tinymix -D 0 get "QUAT_MI2S_RX Audio Mixer MultiMedia3"

wav=/newroot/var/log/op3-speaker-48k-stereo.wav
tinyplay "$wav" -D 0 -d 2 -p 480 -n 8 > /tmp/op3-speaker-tinyplay.log 2>&1 &
play_pid=$!
sleep 1

echo '=== active ASoC states ==='
find /sys/kernel/debug/asoc/OnePlus3 -maxdepth 2 -type f -name state \
  -print -exec cat {} \;

echo '=== speaker-related kernel log ==='
dmesg | grep -Ei 'QUAT|MI2S|tfa989|Speaker|IBIT|NOCLK|DATA_WRITE_DONE|error|fail' \
  | tail -n 120

wait "$play_pid"
echo "tinyplay_exit=$?"
cat /tmp/op3-speaker-tinyplay.log
```

The result is **PASS for this hypothesis only** when the log contains no
`error playing sample` (or QUAT IBIT setup error), the active backend reaches
`start` with S16_LE/48 kHz/stereo, the finite WAV drains, and the owner hears
the phone's external speaker. A zero process exit alone is not sufficient,
because the current TinyALSA `tinyplay` can print `error playing sample` and
still return zero after its write loop breaks. A silent speaker rejects this
clock-only hypothesis and must not be used to claim that the TFA989x runtime
configuration is complete.

## Archive

`patches/pmos612-op3-audio/0015-ASoC-apq8096-enable-QUAT-MI2S-speaker-clock.patch`

The patch is generated from the single source commit and changes no DTS or
userspace file. The historical reference is
`/home/kai/下载/Documents-Codex-20260826/Codex/2026-08-20/3-pcm-tone-c-0-7/work/handoff/apq8096_v23_quat_ibit_clock.patch`.
