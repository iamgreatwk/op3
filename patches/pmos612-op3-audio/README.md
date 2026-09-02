# OP3 audio DTS topology patch

Apply this patch after the ordered `patches/pmos612-op3-own-dtb/` series, to
pmOS MSM8996 v6.12.1 source at `91df7ccd284e5c62c5aed13c2738192b96c1f8dd`.

`0001` disables only OnePlus 3 `MultiMedia4` through `MultiMedia16` front-end
links. The compiled q6asm provider declares IDs 0–2 only, and the generic card
parser aborts at the first available link whose CPU DAI cannot be resolved.
MultiMedia1–3, including the requested MultiMedia3 route, and all back-end
links remain enabled.

The corresponding committed source state is
`955ea0e962134ad27b0fd6fc9b6945c6ffce4817` on branch
`agent/implementation/op3-audio-mic-001` in the dedicated kernel worktree.
