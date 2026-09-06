# OP3 recovery audio-full kernel archive

This directory is the durable GitHub archive of the 31 source commits between
the formal pmOS MSM8996 Linux 6.12.1 baseline and the tested integrated kernel
tree. It is intentionally stored as a `git am` series instead of relying on a
local kernel repository or on an unpushed kernel branch.

The archive starts at:

```text
67b0bbc3cbf46bae712a2606a43361756fcbd829
```

The expected restored source tree is recorded in
`manifests/op3-recovery-audio-full.env` as `KERNEL_TREE`. Reapplying the
series can produce different commit IDs because Git records the new committer
metadata; the tree ID is the reproducibility check.

## Restore from a fresh kernel checkout

From a fresh checkout of this project and a fresh pmOS 6.12.1 kernel clone:

```bash
cd /home/kai/src/oneplus3-mainline

git clone --branch msm8996-stable-6.12.y \
  https://gitlab.com/msm8996-mainline/linux.git \
  /home/kai/src/linux-pmos-msm8996-6.12.1

export OP3_ATH10K_EXTFW_SOURCE=/path/to/external/op3-extfw
scripts/restore-op3-recovery-kernel.sh \
  /home/kai/src/linux-pmos-msm8996-6.12.1 \
  /home/kai/src/linux-pmos-msm8996-6.12.1-op3-recovery-audio-full
```

`OP3_ATH10K_EXTFW_SOURCE` must contain these externally obtained files; the
firmware is not committed to GitHub:

```text
extfw/ath10k/QCA6174/hw3.0/firmware-6.bin
extfw/ath10k/QCA6174/hw3.0/board-2.bin
extfw/ath10k/QCA6174/hw3.0/board.bin
```

The script checks their SHA256 values from the manifest, creates a new kernel
worktree from the pinned baseline, applies all 31 patches, checks the restored
tree ID, and installs the firmware into the restored worktree. It does not
run a kernel build.

## Owner build after restore

The full tested configuration is tracked at
`kernel/configs/oneplus3-recovery-audio-full.config`. The project owner should
run the large kernel build, using a new output directory:

```bash
project=/home/kai/src/oneplus3-mainline
kernel=/home/kai/src/linux-pmos-msm8996-6.12.1-op3-recovery-audio-full
output=$project/out/pmos-msm8996-6.12-recovery-audio-full-rebuild

mkdir -p "$output"
cp "$project/kernel/configs/oneplus3-recovery-audio-full.config" \
  "$output/.config"

make -C "$kernel" O="$output" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  olddefconfig

make -C "$kernel" O="$output" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  Image.gz dtbs
```

The resulting `.config` hash must equal `CONFIG_SHA256` in the manifest. The
initrd and boot image remain separate ignored build artifacts; use the tracked
recovery/initrd recipes and `boot/oneplus3-fa5.env` to reproduce those inputs,
then run `scripts/verify-op3-recovery-manifest.sh --artifacts` when all locked
artifacts are present.
