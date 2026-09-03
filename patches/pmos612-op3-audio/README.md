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
functional test, not an accepted microphone fix.

The corresponding committed source state is
`2a97f48e6eb948fd05482c42a305c4da5e58d4c7` on branch
`agent/implementation/op3-audio-mic-001` in the dedicated kernel worktree.
