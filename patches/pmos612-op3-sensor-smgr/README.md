# OP3 Sensor Manager / SLPI kernel archive

This series adds the Qualcomm Sensor Manager transport used by the OnePlus 3
vendor stack. It is an independent experiment for GitHub Issue #13; it is not
yet part of the accepted recovery image.

The series is based on the already-tested recovery kernel checkpoint:

```text
4f8595b13fbd0bc0caf18897bbb3361699cbb2e5
agent/implementation/recovery-browser-audio-full-001
```

The three commits are:

```text
b853c4b962fa  iio: Add Qualcomm Sensor Manager driver
e17f973f5fa8  soc: qcom: Add in-kernel sensors registry implementation.
d19b3b6bddaa  remoteproc: qcom: Enable in-kernel sns-reg
```

The first two commits are based on the Linux v6.16 MSM8996 Sensor Manager
implementation. The third patch is the equivalent 6.12 adaptation: the
current branch uses `struct qcom_adsp` and has different remoteproc cleanup
labels, so it was adapted without changing the sns-reg lifecycle.

## Fresh checkout

Restore the tested recovery kernel first, then apply this series to a new
worktree. Do not apply these patches directly to the camera worktree.

```bash
cd /home/kai/src/oneplus3-mainline

scripts/restore-op3-recovery-kernel.sh \
  /path/to/fresh-linux-pmos-msm8996-6.12 \
  /path/to/linux-pmos-msm8996-6.12-op3-recovery-audio-full \
  agent/restore/op3-recovery-audio-full-001

git -C /path/to/linux-pmos-msm8996-6.12-op3-recovery-audio-full \
  worktree add -b agent/implementation/op3-sensor-smgr-001 \
  /path/to/linux-pmos-msm8996-6.12-sensor-smgr HEAD

git -C /path/to/linux-pmos-msm8996-6.12-sensor-smgr am \
  --3way --committer-date-is-author-date \
  /home/kai/src/oneplus3-mainline/patches/pmos612-op3-sensor-smgr/*.patch
```

The `git worktree add` command above is illustrative: when the recovery worktree
already has the requested branch or path, create the sensor worktree from the
recovery worktree's `HEAD` using the normal nested-repository worktree rules.
The expected source commits are the three IDs listed above; the resulting
commit IDs are allowed to differ on a fresh host, but the tree must contain
the same files and changes.

## Required external registry

The OP3 vendor HAL uses the SLPI/SSC Sensor Manager. Its registry is not a
direct HLOS I²C device-tree description and must not be guessed or committed
to GitHub. Keep the recovered binary outside the checkout:

```text
$OP3_EXTERNAL_INPUTS/sensors/sns.reg
```

The locked SHA256 is recorded in
`manifests/op3-sensor-smgr.env`. Stage it after the normal firmware tree has
been created and before the Buildroot owner build:

```bash
export OP3_EXTERNAL_INPUTS=/home/kai/op3-recovery-external-inputs
scripts/stage-op3-sensor-registry.sh \
  "$OP3_EXTERNAL_INPUTS" \
  artifacts/op3-initramfs-firmware
```

The default recovery post-build hook already copies the complete staged
`lib/firmware` tree into the Buildroot target. Therefore no separate rootfs
copy is needed once this staging command succeeds.

## Kernel configuration and owner build

Merge the sensor-only fragment into a copy of the tested recovery config:

```bash
project=/home/kai/src/oneplus3-mainline
kernel=/path/to/linux-pmos-msm8996-6.12-sensor-smgr
output="$project/out/pmos-msm8996-6.12-sensor-smgr"

mkdir -p "$output"
cp "$project/kernel/configs/oneplus3-recovery-audio-full.config" \
  "$output/.config"
"$kernel/scripts/kconfig/merge_config.sh" -m -O "$output" \
  "$output/.config" \
  "$project/kernel/configs/oneplus3-recovery-sensor-smgr.fragment"
make -C "$kernel" O="$output" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  olddefconfig
```

The project owner must run the large kernel/Buildroot build and record the
exact source commit, output directories, artifact hashes, boot log, and IIO
enumeration evidence. This archive records no build or device result yet.
