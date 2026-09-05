# OP3 audio DTS topology patch

Apply this ordered series after the `patches/pmos612-op3-own-dtb/` series, to
pmOS MSM8996 v6.12.1 source at `91df7ccd284e5c62c5aed13c2738192b96c1f8dd`.

`0001` disables only OnePlus 3 `MultiMedia4` through `MultiMedia16` front-end
links. The compiled q6asm provider declares IDs 0–2 only, and the generic card
parser aborts at the first available link whose CPU DAI cannot be resolved.
MultiMedia1–3, including the requested MultiMedia3 route, and all back-end
links remain enabled. `0002` replaces only the board's audio-routing property,
dropping three generic external-microphone endpoints that have no widget
declarations; it preserves AMIC2/4/5 to MIC BIAS2/1/3 codec routes. `0003`
moves only the `div1_mclk` GPIO-gated codec-MCLK provider from root to `&soc`,
so the `gpio-clk` platform driver instantiates it. Its RPM parent clock,
GPIO15 gate, pinctrl state, other clocks and all audio routes are unchanged.
`0004` adds only the `MIC BIAS1` to `MCLK` route needed by the AMIC4 capture
path. This lets the already-instantiated `div1_mclk` provider be requested by
the active MIC BIAS1 DAPM path; AMIC2/5 and every other route remain unchanged.
`0005` changes only the WCD9335 decimator `POST_PMU` path: after the selected
TX path is configured, unmuted and its gain latched, it releases that path's
AMIC TX hold. It does not change MCLK routing, DAPM topology, Q6ASM, mixer
controls, firmware, or userspace. This is a legacy-6.3.1 evidence-backed
functional test, not an accepted microphone fix; OP3-AUDIO-010 rejects it.
`0006` restores the preceding WCD9335 behavior and is required only when
applying this sequence on top of `0005`. `0007` then adds a bounded Q6ASM
capture diagnostic: the first 16 `READ_DONE` events per audio client log DSP
status, token, returned address and expected mapped address. It does not
modify the completion, requeue, PCM, DTS, mixer, firmware or userspace path.
`0008` is the next isolated capture test: it restores only the legacy
`NO_REWINDS | SYNC_APPLPTR` capability pair for capture, requiring manual
application-pointer synchronization. It retains `0007`'s bounded trace and
does not modify playback or the Q6ASM/DTS/mixer/firmware/userspace code.
`0009` adds only a bounded, first-16 log at capture `ASM_DATA_CMD_READ_V2`
submission, immediately before the APR send. Together with `0007`, it exposes
the ordering of initial queueing, requeueing and DSP READ_DONE packets without
altering their buffer selection, cadence or handling.
`0010` changes only the prepare-time capture queue depth from all negotiated
periods to one. The existing READ_DONE callback still requeues one buffer, so
this tests whether the premature-completion fault requires a full DSP ring.
It is a diagnostic A/B only, not an accepted capture configuration.
`0011` retains that one-outstanding-read diagnostic and adds bounded pointer
accounting logs only: the first 16 capture READ_DONE callbacks log their
`pcm_irq_pos` increment, and the first 16 `.pointer` calls log their raw
position, wrap decision, reset position and returned frame position. It does
not change the queue, completion, PCM capability, DTS, mixer, firmware,
initramfs or userspace behavior.

`0012` adds only bounded ALSA-core accounting logs in `sound/core/pcm_lib.c`.
`0013` removes only `SNDRV_PCM_INFO_SYNC_APPLPTR` from the capture descriptor;
the device result showed that ARM64 still uses private sync storage and that
`q6asm_dai_open()` overwrites capture hardware parameters with playback ones.
`0014` therefore adds only `SNDRV_PCM_INFO_NO_REWINDS` to the effective
playback descriptor, reproducing the flag that the legacy 6.3.1 source placed
on both descriptors. The owner A/B now passes the ALSA cadence/accounting gate:
the stale backwards application-pointer update is rejected and the stream
remains period-paced. WAV payload validation is still pending.

The corresponding committed source state is
`57b9a78bdc2e` on branch
`agent/implementation/op3-audio-mic-001` in the dedicated kernel worktree.
