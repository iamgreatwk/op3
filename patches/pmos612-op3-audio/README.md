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

The corresponding committed source state is
`f1c19271bba49bf13b52b5bc0d1488a5f19cc825` on branch
`agent/implementation/op3-audio-mic-001` in the dedicated kernel worktree.
