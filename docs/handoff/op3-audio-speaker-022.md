# OP3-AUDIO-022 — route the QUAT speaker link through q6routing

## Scope

This is a DTS-only speaker-topology follow-up on the pmOS MSM8996 Linux
6.12.1 baseline. The sole changed file is
`arch/arm64/boot/dts/qcom/msm8996-oneplus-common.dtsi`; the QUAT SD-line
declaration, q6asm ACK implementation, QUAT clock-provider/IBIT machine-driver
code, mixer helper, TFA989x driver, firmware and initramfs remain fixed.

The OP3-AUDIO-021 image contained `q6afedai/dai@22/qcom,sd-lines = <0 1>`, but
the first speaker write still failed in `SNDRV_PCM_IOCTL_PREPARE` with
`EINVAL`. `MultiMedia3` showed no active DSP links and `div1-clk` remained
disabled. The current `speaker-dai-link` has a CPU and codec but no `platform`
child, unlike the historical 6.3.1 OnePlus DTS.

## Agent handoff

```text
Task / GitHub Issue: OP3-AUDIO-022
Role: Implementation agent
Baseline commit: 1d6cdbb49ce8b2d4fd9d0888704097a797cc3f4e
Working branch: agent/implementation/op3-audio-mic-001
Changed files: arch/arm64/boot/dts/qcom/msm8996-oneplus-common.dtsi
Commit SHA: 44429af1bfab351c5046fdd9d00a1140b7586b1e

Layer: device tree / ASoC DPCM speaker topology
Hypothesis tested: the speaker link is parsed as a normal direct PCM because
  it lacks a `platform` child. It therefore is not marked `no_pcm`, does not
  receive `apq8096` backend operations, and cannot connect the MultiMedia3
  frontend to the QUAT speaker backend through q6routing. Adding the
  q6routing platform component should make it a DPCM backend.
Only variable changed: one `platform` child in `speaker-dai-link`:
  `sound-dai = <&q6routing>;`. No code, clock, codec, mixer or userspace
  behavior changed.

Build run by project owner: NOT_RUN
Build result: the owner must rebuild `Image.gz` and `dtbs` from commit
  `44429af1bfab351c5046fdd9d00a1140b7586b1e`.
Artifacts and SHA256: pending owner build

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: pending owner report

Conclusion: INCONCLUSIVE
Uncertainties: a prepare pass will only prove the DPCM/AFE path reaches the
  active state. q6asm playback scheduling, QUAT clock status, TFA989x
  configuration and audible output still require separate evidence.
Recommended next experiment: boot the rebuilt image, enable the observed
  `QUAT_MI2S_RX Audio Mixer MultiMedia3` route, and repeat the bounded
  `pcm-tone` prepare/state/clock test below. If it passes, run the historical
  paced WAV fixture and record direct speaker output.
```

## Why this change

`qcom_snd_parse_of()` allocates a platform component for every link, but only
sets `link->no_pcm = 1` when the DT link has an explicit `platform` child. A
link with CPU + codec and no platform is left as a regular PCM link. The
machine driver's `apq8096_add_be_ops()` installs the fixed 48 kHz/stereo
backend constraints, QUAT clock-provider callbacks and codec channel-map
operation only on `no_pcm` links. The historical OnePlus 3 DTS connects the
speaker link to `&q6routing`, which makes it a DPCM backend and lets the
MultiMedia3 frontend route to QUAT MI2S. This patch restores that one missing
topology edge without changing any runtime implementation.

## Owner build and package commands

The owner must rebuild both the kernel image and DTBs. `set -e` prevents a
stale DTB or `Image.gz` from being packaged after a failure. The initramfs is
reused unchanged.

```sh
set -e
cd /home/kai/src/oneplus3-mainline
test "$(git -C /home/kai/src/oneplus3-audio-dts-001 rev-parse HEAD)" = \
  44429af1bfab351c5046fdd9d00a1140b7586b1e

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
  artifacts/boot-oneplus3-pmos612-own-dtb-audio-speaker-q6routing-diagnostic.img

sha256sum \
  artifacts/boot-oneplus3-pmos612-own-dtb-audio-speaker-q6routing-diagnostic.img

fastboot boot \
  artifacts/boot-oneplus3-pmos612-own-dtb-audio-speaker-q6routing-diagnostic.img
```

## Owner device test

After SSH login, use one bounded attempt and capture the state even if the
helper exits early:

```sh
mount | grep -q ' /sys/kernel/debug ' || \
  mount -t debugfs none /sys/kernel/debug

/opt/op3-audio/route.sh speaker
tinymix -D 0 get "QUAT_MI2S_RX Audio Mixer MultiMedia3"
tinypcminfo -D 0 -d 2

pcm-tone -D 0 -d 2 -t 5 -f 440 \
  >/tmp/op3-speaker-pcm-tone-q6routing.log 2>&1 &
play_pid=$!
sleep 1

echo '=== MultiMedia3 state ==='
cat /sys/kernel/debug/asoc/OnePlus3/MultiMedia3/state
echo '=== QUAT clock ==='
grep -E '^[[:space:]]*div1-clk[[:space:]]' \
  /sys/kernel/debug/clk/clk_summary || true
echo '=== speaker runtime log ==='
dmesg | grep -Ei \
  'sound|asoc|q6afe|q6asm|q6routing|AFE port|ASM|DATA_WRITE_DONE|QUAT|MI2S|tfa989|NOCLK|XRUN|prepare|hw_params|error|fail' \
  | tail -n 200

if wait "$play_pid"; then
  tone_rc=0
else
  tone_rc=$?
fi
echo "pcm-tone_exit=$tone_rc"
cat /tmp/op3-speaker-pcm-tone-q6routing.log
```

The prepare gate passes only when the tone log has no `cannot prepare
channel`, the MultiMedia3 playback path shows an active backend (rather than
`No active DSP links`), and the QUAT clock is enabled while the stream is
active. That is not yet acoustic acceptance; retain the bounded log and then
run the paced WAV fixture if the gate passes.

## Archive

`patches/pmos612-op3-audio/0019-arm64-dts-msm8996-oneplus-route-speaker-q6routing.patch`

Historical reference:

`/home/kai/下载/home-sqwan-20260826/sqwan/linux-fa5-v56/arch/arm64/boot/dts/qcom/msm8996-oneplus-common.dtsi`
