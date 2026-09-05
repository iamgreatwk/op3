# OP3-AUDIO-023 — mux the QUAT MI2S speaker pins

## Scope

This is a DTS-only physical-pin follow-up on the pmOS MSM8996 Linux 6.12.1
baseline. The sole changed file is
`arch/arm64/boot/dts/qcom/msm8996-oneplus-common.dtsi`; q6asm playback,
q6routing, QUAT clock-provider code, mixer helper, TFA989x code, firmware,
initramfs and the WAV fixture remain fixed.

OP3-AUDIO-022 proved that `MultiMedia3 -> q6routing -> Speaker` can prepare,
run and drain, but the owner heard no output from either the tone or a paced
WAV. The current DTS had no pinctrl state for the QUAT MI2S pins. The
historical 6.3.1 device snapshot shows GPIO58, GPIO59, GPIO60 and GPIO61
owned by `sound` with function `qua_mi2s` before, during and after playback.

## Agent handoff

```text
Task / GitHub Issue: OP3-AUDIO-023
Role: Implementation agent
Baseline commit: 44429af1bfab351c5046fdd9d00a1140b7586b1e
Working branch: agent/implementation/op3-audio-mic-001
Changed files: arch/arm64/boot/dts/qcom/msm8996-oneplus-common.dtsi
Commit SHA: c76160cde821b2f4908b0751a7c19aa2a828a6cf

Layer: device tree / QUAT MI2S physical pinmux
Hypothesis tested: playback reaches the digital Speaker backend but remains
  silent because GPIO58–61 are not muxed from their reset/default function to
  the QUAT MI2S clock/frame/data signals. Applying the historical `sound`
  pinctrl state should put the TFA9890's SCLK/WS/SD0/SD1 pins on the QUAT bus.
Only variable changed: the `&sound` default pinctrl reference and the three
  QUAT pinctrl states for GPIO58–61. No TFA supply/reset, clock code, q6asm,
  routing, mixer or userspace behavior changed.

Build run by project owner: YES (2026-09-06)
Build result: PASS. The owner rebuilt `Image.gz` and `dtbs` from
  `c76160cde821b2f4908b0751a7c19aa2a828a6cf` and packaged the diagnostic boot
  image.
Artifacts and SHA256:
  `Image.gz` =
  `89ff623ca7880504ae96bbc176af5ab6f9ed906d96f0ef1706a931f083122385`
  `msm8996-oneplus3.dtb` =
  `5998241ee287b23ae83afdeb81207298c8a34040af12c8e099f2a47dac313dc8`
  `boot-oneplus3-pmos612-own-dtb-audio-speaker-pinctrl-diagnostic.img` =
  `6572d1e2d207313c3a96161cf04f55a317bd4e697df30d8d46cea5d57b36ece8`

Device test run by project owner: YES (2026-09-06)
Device result: PASS (pinmux and acoustic gate). Runtime pinmux showed GPIO58,
  GPIO59, GPIO60 and GPIO61 owned by `sound` with `function qua_mi2s`. The
  owner enabled `QUAT_MI2S_RX Audio Mixer MultiMedia3`; `pcm-tone` completed
  493 writes and allowed the final buffer to drain, and the owner confirmed
  that the external speaker was audible.
Evidence links / log paths: owner SSH output; runtime
  `/sys/kernel/debug/pinctrl/*/pinmux-pins`; `/tmp/op3-speaker-pcm-tone-pinctrl.log`.

Conclusion: SUPPORTED (pinmux hypothesis supported by owner test; Integration
  promotion remains pending)
Uncertainties: OP3-AUDIO-022's `tfa989x ... supply vddd not found, using
  dummy regulator` warning remains unchanged. The TFA9890 reset/power state
  is a separate follow-up variable and must not be folded into this pinmux A/B.
  The unrelated PHY `-517`, RMI4 `-22`, GPU firmware `-2`, and PDR `-6` lines
  are not evidence of a speaker-stream failure.
Recommended next experiment: no further speaker source change is required for
  basic output. Preserve this PASS while the owner records the result for
  Integration; if codec/power cleanup is desired later, test the historical
  `vddd-supply` as a separate variable.
```

## Hardware volume status

The current QUAT path has no supported TFA9890 hardware-volume control. The
speaker link is `MultiMedia3 -> q6routing -> QUAT_MI2S_RX -> TFA9890`, while
the TFA989x driver deliberately bypasses CoolFlux DSP; its source notes that
this bypass disables the amplifier's volume control. The `RX* Digital Volume`
controls exported by WCD9335 belong to a different codec path and must not be
treated as QUAT speaker controls. Until a TFA profile/container or a
documented gain register is integrated as a separate experiment, use the
verified `pcm-wav -v 0..100` software scaling and leave the hardware route
unchanged.

## Why this variable

The mainline pinctrl driver exposes `qua_mi2s` on GPIO57–63, but the current
OnePlus common DTS did not request any of those groups. ASoC can therefore show
an active backend while the SoC's QUAT signals never reach the board traces.
The 6.3.1 OnePlus DTS requests:

* GPIO58/59 as the QUAT clock/frame pair, drive strength 8, bias disabled,
  output high;
* GPIO60 as QUAT SD0, drive strength 8, bias disabled; and
* GPIO61 as QUAT SD1, drive strength 8, bias disabled.

This patch copies only that physical pin state and attaches it to the common
sound card. It deliberately does not add the historical `vddd-supply` or
`tfa-reset-hog`, because those would make a second hardware variable.

## Owner build, package and boot commands

The owner must run the build and device boot. Stop on any build error so a
stale DTB cannot be packaged:

```sh
set -e
cd /home/kai/src/oneplus3-mainline
test "$(git -C /home/kai/src/oneplus3-audio-dts-001 rev-parse HEAD)" = \
  c76160cde821b2f4908b0751a7c19aa2a828a6cf

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
  artifacts/boot-oneplus3-pmos612-own-dtb-audio-speaker-pinctrl-diagnostic.img

sha256sum \
  artifacts/boot-oneplus3-pmos612-own-dtb-audio-speaker-pinctrl-diagnostic.img

fastboot boot \
  artifacts/boot-oneplus3-pmos612-own-dtb-audio-speaker-pinctrl-diagnostic.img
```

## Owner device test

```sh
mount | grep -q ' /sys/kernel/debug ' || \
  mount -t debugfs none /sys/kernel/debug

echo '=== QUAT pinmux before playback ==='
cat /sys/kernel/debug/pinctrl/*/pinmux-pins \
  | grep -E 'pin (58|59|60|61) ' || true

/opt/op3-audio/route.sh speaker
tinymix -D 0 get "QUAT_MI2S_RX Audio Mixer MultiMedia3"

pcm-tone -D 0 -d 2 -t 5 -f 440 \
  >/tmp/op3-speaker-pcm-tone-pinctrl.log 2>&1 &
play_pid=$!
sleep 1

echo '=== QUAT pinmux while playback is active ==='
cat /sys/kernel/debug/pinctrl/*/pinmux-pins \
  | grep -E 'pin (58|59|60|61) ' || true
echo '=== MultiMedia3 state ==='
cat /sys/kernel/debug/asoc/OnePlus3/MultiMedia3/state
echo '=== speaker-related kernel log ==='
dmesg | grep -Ei \
  'sound|asoc|q6afe|q6asm|q6routing|QUAT|MI2S|tfa989|NOCLK|DATA_WRITE_DONE|prepare|error|fail' \
  | tail -n 200

if wait "$play_pid"; then
  tone_rc=0
else
  tone_rc=$?
fi
echo "pcm-tone_exit=$tone_rc"
cat /tmp/op3-speaker-pcm-tone-pinctrl.log

echo '=== TFA9890 status after playback ==='
for registers in /sys/kernel/debug/regmap/*-0036/registers; do
  [ -r "$registers" ] || continue
  grep -E '^00:|^03:' "$registers" || true
done
```

The pinmux gate passes only if GPIO58–61 all show `sound` and
`function qua_mi2s`, the backend remains `start` with S16_LE/48 kHz/stereo,
and the owner can hear the tone. A clean `pcm-tone` exit without the pinmux
or acoustic evidence is not a pass. If the pins are correct but the speaker
is still silent, retain this commit and test the separate TFA `vddd-supply`
variable next.

## Archive

`patches/pmos612-op3-audio/0020-arm64-dts-msm8996-oneplus-mux-quat-mi2s-speaker-pins.patch`

Historical reference:

`/home/kai/下载/home-sqwan-20260826/sqwan/linux-fa5-v56/arch/arm64/boot/dts/qcom/msm8996-oneplus-common.dtsi`
