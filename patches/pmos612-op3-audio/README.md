# OP3 audio DTS topology patch

Apply this ordered series after the `patches/pmos612-op3-own-dtb/` series, to
pmOS MSM8996 v6.12.1 source at `91df7ccd284e5c62c5aed13c2738192b96c1f8dd`.

`0001` disables only OnePlus 3 `MultiMedia4` through `MultiMedia16` front-end
links. The compiled q6asm provider declares IDs 0–2 only, and the generic card
parser aborts at the first available link whose CPU DAI cannot be resolved.
MultiMedia1–3, including the requested MultiMedia3 route, and all back-end
links remain enabled. `0002` replaces only the board's audio-routing property,
dropping three generic external-microphone endpoints that have no widget
declarations; it preserves AMIC2/4/5 to MIC BIAS2/1/3 codec routes.

The corresponding committed source state is
`401a5673192be4239aecfddb5594f254d3d3e2a2` on branch
`agent/implementation/op3-audio-mic-001` in the dedicated kernel worktree.
