# OP3-AUDIO-021 — configure QUAT MI2S SD lines

## Scope

This is a DTS-only speaker-prepare follow-up on the pmOS MSM8996 Linux 6.12.1
baseline. The sole changed file is
`arch/arm64/boot/dts/qcom/msm8996-oneplus-common.dtsi`; the q6asm ACK
implementation, QUAT clock-provider/IBIT machine-driver code, mixer helper,
TFA989x driver, firmware and initramfs remain fixed.

The OP3-AUDIO-020 device run failed before q6asm playback scheduling:
`pcm-tone` returned `cannot prepare channel: Invalid argument`,
`MultiMedia3` stayed `close`, and `div1-clk` stayed disabled. In 6.12,
`q6afe_i2s_port_prepare()` rejects a zero SD-line mask. The OnePlus DTS did
not define one for `QUATERNARY_MI2S_RX`, while the historical 6.3.1 DTS
defines SD0/SD1 (`qcom,sd-lines = <0 1>`).

## Agent handoff

```text
Task / GitHub Issue: OP3-AUDIO-021
Role: Implementation agent
Baseline commit: f95f3b02ec1e770ff50604a252a310c443620006
Working branch: agent/implementation/op3-audio-mic-001
Changed files: arch/arm64/boot/dts/qcom/msm8996-oneplus-common.dtsi
Commit SHA: 1d6cdbb49ce8b2d4fd9d0888704097a797cc3f4e

Layer: device tree / q6afe QUAT MI2S port configuration
Hypothesis tested: PCM prepare returns -EINVAL because the QUAT AFE DAI has
  no configured SD data lines (`sd_line_mask=0`). Declaring SD0 and SD1 lets
  the two-channel speaker backend prepare the I2S port.
Only variable changed: one `q6afedai` child node with
  `qcom,sd-lines = <0 1>`. Kernel q6asm/clock code and userspace are fixed.

Build run by project owner: NOT_RUN
Build result: the owner must rebuild `Image.gz` and `dtbs` from commit
  `1d6cdbb49ce8b2d4fd9d0888704097a797cc3f4e`.
Artifacts and SHA256: pending owner build

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: pending owner report

Conclusion: INCONCLUSIVE
Uncertainties: no DTB rebuild or device test has occurred yet. A prepare
  pass only proves that the AFE port can start; q6asm playback scheduling,
  QUAT clock status, TFA989x configuration and audible output remain to be
  checked.
Recommended next experiment: rebuild the DTB plus Image.gz, boot the new
  image, and repeat the five-second `pcm-tone` test from OP3-AUDIO-020.
```

## Why this change

`of_q6afe_parse_dai_data()` defaults each MI2S private `sd_line_mask` to zero
when no `qcom,sd-lines` property is present. In
`q6afe_i2s_port_prepare()`, `hweight_long(0)` selects the `no line is
assigned` error path and returns `-EINVAL` before `q6afe_port_start()` or the
machine driver's IBIT clock request. The new node maps the stereo QUAT port
to SD0/SD1, matching the historical OnePlus 3 DTS. For a two-channel stream,
the existing AFE code reduces the QUAD01 mode to SD0 and selects stereo mode;
no q6asm or codec behavior is changed.

## Owner build and package commands

The owner must rebuild both the kernel image and DTBs. `set -e` prevents a
stale DTB or Image.gz from being packaged after a failure. The initramfs is
reused unchanged.

```sh
set -e
cd /home/kai/src/oneplus3-mainline
test "$(git -C /home/kai/src/oneplus3-audio-dts-001 rev-parse HEAD)" = \
  1d6cdbb49ce8b2d4fd9d0888704097a797cc3f4e

make -C /home/kai/src/oneplus3-audio-dts-001 \
  O=/home/kai/src/oneplus3-mainline/out/linux-pmos-msm8996-6.12-defconfig-audio \
  ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- \
  CC=aarch64-linux-gnu-gcc-11 \
  CONFIG_EXTRA_FIRMWARE_DIR=/home/kai/src/oneplus3-mainline/source/linux-pmos-msm8996-6.12/extfw \
  -j"$(nproc)" Image.gz dtbs

sha256sum \
  out/linux-pmos-msm8996-6.12-defconfig-audio/arch/arm64/boot/Image.gz \
  out/linux-pmos-msm8996-6.12-defconfig-audio/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb

./scripts/pack-boot.sh \
  out/linux-pmos-msm8996-6.12-defconfig-audio/arch/arm64/boot/Image.gz \
  out/linux-pmos-msm8996-6.12-defconfig-audio/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb \
  artifacts/initrd-op3-audio-tx-hold-routefix-diagnostic.cpio.gz \
  artifacts/boot-oneplus3-pmos612-own-dtb-audio-speaker-sdlines-diagnostic.img

sha256sum \
  artifacts/boot-oneplus3-pmos612-own-dtb-audio-speaker-sdlines-diagnostic.img

fastboot boot \
  artifacts/boot-oneplus3-pmos612-own-dtb-audio-speaker-sdlines-diagnostic.img
```

## Owner device test

After SSH login:

```sh
mount | grep -q ' /sys/kernel/debug ' || \
  mount -t debugfs none /sys/kernel/debug

/opt/op3-audio/route.sh speaker
tinymix -D 0 get "QUAT_MI2S_RX Audio Mixer MultiMedia3"
tinypcminfo -D 0 -d 2

echo "before=$(date +%s.%N)"
pcm-tone -D 0 -d 2 -t 5 -f 440 \
  >/tmp/op3-speaker-pcm-tone-sdlines.log 2>&1 &
play_pid=$!
sleep 1

cat /sys/kernel/debug/asoc/OnePlus3/MultiMedia3/state
grep -E '^[[:space:]]*div1-clk[[:space:]]' \
  /sys/kernel/debug/clk/clk_summary || true
dmesg | grep -Ei \
  'no line|AFE port|ASM|DATA_WRITE_DONE|QUAT|MI2S|tfa989|NOCLK|XRUN|error|fail' \
  | tail -n 160

wait "$play_pid"
tone_rc=$?
echo "pcm-tone_exit=$tone_rc"
cat /tmp/op3-speaker-pcm-tone-sdlines.log
echo "after=$(date +%s.%N)"
```

This experiment passes its prepare gate only when `pcm-tone` no longer
reports `cannot prepare channel`, the active state shows `MultiMedia3` and
the speaker backend running, and `div1-clk` becomes enabled during playback.
That is not yet an acoustic acceptance. If the tone reaches completion,
repeat with the historical paced WAV helper and record direct speaker output.

## Archive

`patches/pmos612-op3-audio/0018-arm64-dts-msm8996-oneplus-configure-quat-mi2s-sd-lines.patch`

Historical reference:

`/home/kai/下载/home-sqwan-20260826/sqwan/linux-fa5-v56/arch/arm64/boot/dts/qcom/msm8996-oneplus-common.dtsi`
