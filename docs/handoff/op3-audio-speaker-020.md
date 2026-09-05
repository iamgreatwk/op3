# OP3-AUDIO-020 — q6asm playback ACK queueing

## Scope

This is a kernel-only speaker-output follow-up on the pmOS MSM8996 Linux
6.12.1 baseline. The sole changed source file is
`sound/soc/qcom/qdsp6/q6asm-dai.c`; the DTS topology, QUAT MI2S clock
provider patch, `route.sh`, TFA989x driver, firmware, initramfs and playback
fixture remain fixed.

The preceding OP3-AUDIO-019 image set the speaker mixer route to `On`, but a
raw `tinyplay` run failed on its first PCM write and closed `MultiMedia3`
before any active DSP link was visible. The old 6.3.1 implementation used a
custom `pcm-tone`/`pcm-wav` path and did not blindly requeue playback buffers
from every ASM completion. This experiment isolates that playback scheduling
difference.

## Agent handoff

```text
Task / GitHub Issue: OP3-AUDIO-020
Role: Implementation agent
Baseline commit: 8c181e065c6fc4f911cdc746dbf5ce8bd3e391ca
Working branch: agent/implementation/op3-audio-mic-001
Changed files: sound/soc/qcom/qdsp6/q6asm-dai.c
Commit SHA: f95f3b02ec1e770ff50604a252a310c443620006

Layer: audio kernel / ASoC q6asm front-end playback scheduling
Hypothesis tested: the first speaker write fails because q6asm queues a
  playback buffer unconditionally from RUN_DONE and every DATA_WRITE_DONE,
  independent of ALSA's application pointer. Queueing only the periods made
  available by ALSA's `ack` callback should keep the DSP/ALSA ring in sync.
Only variable changed: playback q6asm queue ownership. The patch adds a
  `queue_ptr`, advertises `SNDRV_PCM_INFO_SYNC_APPLPTR` on the playback
  descriptor, removes the two blind playback requeues, and implements the
  component `.ack` callback. Capture code is unchanged.

Build run by project owner: PASS (2026-09-05)
Build result: commit `f95f3b02ec1e770ff50604a252a310c443620006` compiled and
was packaged successfully.
Artifacts and SHA256: `be41815eb03c5127ec0c7726a79a2dc025542b3e30cb799761776587b0d26fe6`
(`boot-oneplus3-pmos612-own-dtb-audio-speaker-ack-diagnostic.img`)

Device test run by project owner: YES (2026-09-05)
Device result: INCONCLUSIVE at the PCM-prepare gate. `pcm-tone` failed on
its first 3840-frame write with `cannot prepare channel: Invalid argument`
(`errno 22`), `MultiMedia3` stayed `close`, and `div1-clk` stayed disabled.
No q6asm playback completion or QUAT runtime event was reached.
Evidence links / log paths: owner pasted terminal output in the task;
`/tmp/op3-speaker-pcm-tone.log` on the device.

Conclusion: INCONCLUSIVE
Uncertainties: the failure occurs before q6asm playback scheduling can be
  observed. Source inspection identifies the existing DTS omission of
  `qcom,sd-lines` for `QUATERNARY_MI2S_RX`; `q6afe_i2s_port_prepare()` then
  rejects the port with `sd_line_mask=0`. This is tracked as OP3-AUDIO-021 and
  does not reject the ACK hypothesis.
Recommended next experiment: add only the historical QUAT SD0/SD1 DTS
  declaration, rebuild the DTB together with this q6asm commit, and repeat
  the same `pcm-tone` test.
```

## Why this variable

The supplied 6.3.1 source has a `queue_ptr` and a component `.ack` callback.
Its playback hardware advertises `SNDRV_PCM_INFO_SYNC_APPLPTR`; the ASM
`RUN_DONE` and `DATA_WRITE_DONE` callbacks only report period completion,
while the ALSA ACK path submits exactly the periods newly exposed by
`runtime->control->appl_ptr`. The current 6.12 file instead requeues a
buffer from both completion cases, even when ALSA has not advanced the
application pointer. That is the smallest source-only difference supported
by the historical successful speaker path.

The patch deliberately adds the capability bit only to playback. The
microphone capture runtime and its accepted `NO_REWINDS` behavior are not
part of this experiment.

## Prepare-gate finding

The owner run proved that the ACK callback was not reached: the first
`pcm_writei()` returned `-EINVAL`, the frontend state was `close`, and
`div1-clk` never became enabled. In the 6.12 source,
`of_q6afe_parse_dai_data()` initializes the QUAT DAI's SD-line mask to zero
when its child node has no `qcom,sd-lines`. `q6afe_i2s_port_prepare()` then
returns `-EINVAL` for `num_sd_lines == 0` before it can start the AFE port.
The legacy OnePlus DTS contains the missing declaration:
`dai@22 { reg = <QUATERNARY_MI2S_RX>; qcom,sd-lines = <0 1>; }`.
That DTS-only correction is isolated in OP3-AUDIO-021.

## Owner build and package commands

The owner alone runs the kernel build and device boot. `set -e` is required so
an old `Image.gz` cannot be packaged after a compile failure. The DTB and
initramfs are reused from the last audio checkpoint.

```sh
set -e
cd /home/kai/src/oneplus3-mainline
test "$(git -C /home/kai/src/oneplus3-audio-dts-001 rev-parse HEAD)" = \
  f95f3b02ec1e770ff50604a252a310c443620006

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
  artifacts/boot-oneplus3-pmos612-own-dtb-audio-speaker-ack-diagnostic.img

sha256sum \
  artifacts/boot-oneplus3-pmos612-own-dtb-audio-speaker-ack-diagnostic.img
```

## Owner device test

After `fastboot boot` and SSH login, first verify the route and the custom
diagnostic tools. Do not use the `tinyplay` exit status as the pass criterion.

```sh
mount | grep -q ' /sys/kernel/debug ' || mount -t debugfs none /sys/kernel/debug

/opt/op3-audio/route.sh speaker
tinymix -D 0 get "QUAT_MI2S_RX Audio Mixer MultiMedia3"
command -v pcm-tone
command -v pcm-wav
command -v tinypcminfo
tinypcminfo -D 0 -d 2
tinypcminfo -D 0 -d 5

dmesg > /tmp/op3-speaker-before
date +%s.%N
pcm-tone -D 0 -d 2 -t 5 -f 440 > /tmp/op3-speaker-pcm-tone.log 2>&1 &
play_pid=$!
sleep 1

echo '=== active ASoC states ==='
cat /sys/kernel/debug/asoc/OnePlus3/MultiMedia3/state
grep -E '^[[:space:]]*div1-clk[[:space:]]' \
  /sys/kernel/debug/clk/clk_summary || true

echo '=== TFA9890 regmap (read-only) ==='
for registers in /sys/kernel/debug/regmap/*-0036/registers; do
  [ -r "$registers" ] || continue
  echo "=== $registers"
  cat "$registers"
done

echo '=== speaker-related kernel log ==='
dmesg | grep -Ei 'ASM|DATA_WRITE_DONE|QUAT|MI2S|tfa989|NOCLK|XRUN|error|fail' \
  | tail -n 160

wait "$play_pid"
echo "pcm-tone_exit=$?"
cat /tmp/op3-speaker-pcm-tone.log
```

For the first A/B, a useful result is a five-second tone process that stays
alive for approximately five seconds, reports the custom completion/drain
message, leaves the playback backend in `start` while active, and produces
audible output. Repeated `DATA_WRITE_DONE` events and a TFA9890 status without
`NOCLK` are supporting evidence. If `pcm-tone` succeeds, run the historical
paced WAV helper next:

```sh
pcm-wav -D 0 -d 2 \
  /newroot/var/log/op3-amic4-effective-norewinds.wav \
  > /tmp/op3-speaker-pcm-wav.log 2>&1
cat /tmp/op3-speaker-pcm-wav.log
```

No device result is claimed until the owner reports the process duration,
logs and direct acoustic result. A tone-only pass does not complete the
TFA989x speaker migration.

## Archive

`patches/pmos612-op3-audio/0017-ASoC-q6asm-queue-playback-from-applptr-ack.patch`

Historical references:

* `/home/kai/下载/home-sqwan-20260826/sqwan/linux-fa5-v56/sound/soc/qcom/qdsp6/q6asm-dai.c`
* `/home/kai/下载/Documents-Codex-20260826/Codex/2026-08-20/3-pcm-tone-c-0-7/work/handoff/README_FOR_CHATGPT.md`
