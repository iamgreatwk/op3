# pmOS 6.12 OnePlus 3 q6asm sound-card registration patch

This patch targets the formal pmOS MSM8996 Linux 6.12.1 baseline at
`67b0bbc3cbf46bae712a2606a43361756fcbd829`. It is source preparation on the
recovery implementation branch; apply it in the assigned/private formal
kernel worktree, not in this recovery checkout and not in another agent's
kernel worktree.

Apply it from the formal kernel worktree:

```sh
git am \
  /home/kai/src/oneplus3-mainline/patches/pmos612-op3-audio-card/0001-*.patch
```

The existing OP3 DTS declares `MultiMedia1` through `MultiMedia16` in the
`qcom,apq8096-sndcard`, while `&q6asmdai` only declares frontend IDs 0, 1,
and 2. Linux 6.12's q6asm driver registers only child DAIs declared in DT.
As a result, the card parser fails at `MultiMedia4` with `-EINVAL` before
registering any ALSA card. The patch enables IDs 3 through 15, matching all
existing links and leaving their routes unchanged.

This patch is intentionally limited to sound-card DAI enumeration. It does
not change the recovery userspace, codec controls, microphone routing,
speaker routing, Wi-Fi, DRM, or input/haptics patches. The expected first
device evidence is a registered ALSA card under `/proc/asound/cards` and
`/dev/snd/pcm*`; only after that should recovery `tinycap` and `tinyplay`
be tested.
