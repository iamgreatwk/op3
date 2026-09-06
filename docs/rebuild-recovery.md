# OnePlus 3 recovery clean rebuild

This is the rebuild recipe for the current integration line:

```text
top-level GitHub branch: agent/implementation/recovery-browser-001
kernel baseline:        pmOS MSM8996 Linux 6.12.1
Buildroot commit:       679b9ead7620bbf193620d1ebf56f53c1764d37a
```

The top-level GitHub repository contains the source, kernel patch archive,
configuration, scripts and Buildroot project-owned patches. Generated
directories (`source/`, `out/`, `artifacts/`, and `cache/`) are deliberately
ignored. A small set of binary inputs must be kept outside GitHub and supplied
by path; their SHA256 values are checked by the recipe.

## 1. Fresh checkout

```bash
git clone --branch agent/implementation/recovery-browser-001 \
  --single-branch https://github.com/iamgreatwk/op3.git op3-rebuild
cd op3-rebuild
./scripts/agent-start.sh
```

The kernel source is a separate repository. Restore it from the pinned pmOS
6.12.1 baseline and the tracked 31-patch series. `OP3_ATH10K_EXTFW_SOURCE`
must point to an external directory containing:

```text
ath10k/QCA6174/hw3.0/firmware-6.bin
ath10k/QCA6174/hw3.0/board-2.bin
```

Both files are proprietary/external and their expected hashes are in
`manifests/op3-recovery-audio-full.env`.

```bash
mkdir -p source
git clone --branch msm8996-stable-6.12.y --single-branch \
  https://gitlab.com/msm8996-mainline/linux.git \
  source/linux-pmos-msm8996-6.12-base

OP3_ATH10K_EXTFW_SOURCE=/path/to/external-inputs \
  ./scripts/restore-op3-recovery-kernel.sh \
  source/linux-pmos-msm8996-6.12-base \
  source/linux-pmos-msm8996-6.12-recovery-audio-full \
  agent/implementation/recovery-browser-audio-full-001
```

The restore script applies all 31 patches and verifies the expected tree
object. It does not compile the kernel.

## 2. Buildroot source and patches

Run this against a fresh Buildroot checkout. It pins commit
`679b9ead7620bbf193620d1ebf56f53c1764d37a`, installs the tracked OP3 browser
defconfig, and installs the three Cog patches in the correct order. It does
not build anything.

```bash
./scripts/prepare-op3-buildroot.sh source/buildroot
```

The installed defconfig is also locked by SHA256 in
`manifests/op3-recovery-audio-full.env`; the preparation script rejects a
modified project defconfig before touching the Buildroot checkout.

The owner then performs the large Buildroot build:

```bash
mkdir -p out/buildroot-op3-egl
make -C source/buildroot O="$PWD/out/buildroot-op3-egl" \
  op3_browser_defconfig
make -C source/buildroot O="$PWD/out/buildroot-op3-egl" \
  BR2_JLEVEL=3 2>&1 | tee /tmp/buildroot-op3-egl.log
```

The tracked defconfig includes the WPE WebKit/Cog/Weston/Mesa stack, TLS,
WPEWebDriver, and `BR2_PACKAGE_TINYALSA=y` plus
`BR2_PACKAGE_TINYALSA_TOOLS=y`. Therefore the same Buildroot target supplies
the browser files and `tinycap`, `tinymix`, and `tinyplay` for recovery audio.

For a clean retry of a failed WebKit package build, keep the Buildroot output
directory and run the relevant owner-approved package dirclean before the
normal `make`; do not copy files from an older `out/` tree.

## 3. Reference initrd and firmware-provenance initrd

The historical pmOS ramdisk is not generated from the GitHub source tree. It
is extracted from the known-good external v100 boot image. The source boot
image must hash to:

```text
29ccd3eb8b093b29fc44435bd6e5f98367cf3794c117f9527a6bf3c1ebc5d781
```

Extract it with the tracked script:

```bash
./scripts/extract-reference-initrd.sh \
  /path/to/boot_fa5_v100_auto.img \
  artifacts/reference-initrd.img
```

The extracted `artifacts/reference-initrd.img` must hash to
`c3358a1cadb747996ddaa492e636827f2d72974040e8fd40d81f8a213e676366`.

The following two steps replace the declared Qualcomm firmware files while
leaving the rest of the historical archive controlled and auditable. They
require the owner-approved `mtools`, `pil-squasher`, `curl`, and `sha512sum`
environment described in `docs/build-environment.md`:

```bash
./scripts/prepare-a530-firmware.sh \
  artifacts/a530-firmware

./scripts/stage-msm8996-oneplus3-firmware.sh \
  artifacts/msm8996-oneplus3-firmware-verified

./scripts/make-firmware-provenance-initrd.sh \
  artifacts/reference-initrd.img \
  artifacts/msm8996-oneplus3-firmware-verified \
  artifacts/initrd-op3-firmware-provenance-v2.cpio.gz
```

`prepare-a530-firmware.sh` obtains the three Adreno files from the host
`linux-firmware` package and verifies their hashes. The Qualcomm modem/ADSP/
SLPI/Venus files are derived from the SHA512-pinned `NON-HLOS.bin` by the
second script. These firmware blobs are not committed to GitHub.

## 4. Browser bundle

Fetch the exact CJK font used by the tested browser image. The `.deb` is only
a download input; the staged font must hash to the value in the manifest.

```bash
mkdir -p artifacts/fonts
cd artifacts/fonts
apt-get download fonts-wqy-microhei=0.2.0-beta-4
dpkg -x fonts-wqy-microhei_0.2.0-beta-4_all.deb unpacked
install -m 0644 unpacked/usr/share/fonts/truetype/wqy/wqy-microhei.ttc \
  wqy-microhei.ttc
cd ../..
sha256sum artifacts/fonts/wqy-microhei.ttc
```

After the owner Buildroot build succeeds, stage the whole self-contained
Wayland/WPE browser tree:

```bash
./scripts/stage-browser-rootfs.sh \
  out/buildroot-op3-egl/target \
  artifacts/op3-browser-bundle.tar.gz
```

This creates an archive whose device path is `/newroot/opt/op3-browser`.
It includes the Buildroot runtime libraries, Cog/WPE/WebKit, Weston, Mesa,
font data, the tracked browser runner and the local test page.

## 5. Wi-Fi bundle

The Wi-Fi bundle is made from the modules installed by the same final 6.12.1
kernel build. The kernel owner must first build and install modules into a
new, empty staging directory:

```bash
kernel=source/linux-pmos-msm8996-6.12-recovery-audio-full
kout=out/pmos-msm8996-6.12-recovery-audio-full-s1302-retry
wifi_mods=artifacts/op3-wifi-modules-root

make -C "$kernel" O="$PWD/$kout" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 modules
make -C "$kernel" O="$PWD/$kout" ARCH=arm64 \
  CROSS_COMPILE=aarch64-linux-gnu- CC=aarch64-linux-gnu-gcc-11 \
  INSTALL_MOD_PATH="$PWD/$wifi_mods" modules_install
```

Then stage only the matching module dependency closure and the tracked Wi-Fi
CLI:

```bash
./scripts/stage-op3-wifi-rootfs.sh \
  artifacts/op3-wifi-modules-root \
  artifacts/op3-wifi-bundle.tar.gz
```

The Wi-Fi bundle does not contain credentials or ath10k firmware. Credentials
are entered on the device with `wifi connect`; the QCA6174 firmware remains
in the verified initrd. IPv6 is disabled by the initramfs hook and can be
enabled later with `wifi ipv6 on`.

## 6. Recovery and audio bundles

The small recovery bundle is compiled from tracked C/libtsm sources. This is
not a kernel or Buildroot build:

```bash
./scripts/build-recovery-mainline.sh \
  out/recovery/recovery_mainline
./scripts/stage-recovery-rootfs.sh \
  artifacts/op3-recovery-browser-audio-bundle.tar.gz \
  out/recovery/recovery_mainline
```

The audio payload is the Buildroot target plus the tracked diagnostic route
helper. It contains the TinyALSA tools required by the recovery microphone
key. The archive is a persistent payload, not an initramfs and not a complete
filesystem image:

```bash
./scripts/stage-op3-audio-rootfs.sh \
  out/buildroot-op3-egl/target \
  artifacts/op3-audio-rootfs.tar.gz
```

The final recovery initrd overlays the recovery bundle on the firmware-
provenance browser initrd:

```bash
./scripts/make-recovery-browser-initrd.sh \
  artifacts/initrd-op3-firmware-provenance-v2.cpio.gz \
  artifacts/initrd-op3-recovery-browser.cpio.gz
./scripts/make-recovery-audio-initrd.sh \
  artifacts/initrd-op3-recovery-browser.cpio.gz \
  artifacts/op3-recovery-browser-audio-bundle.tar.gz \
  artifacts/initrd-op3-recovery-browser-audio.cpio.gz
```

## 7. Pack and verify the boot image

After the owner builds the restored kernel, pack the Android boot image:

```bash
./scripts/pack-boot.sh \
  "$kout/arch/arm64/boot/Image.gz" \
  "$kout/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb" \
  artifacts/initrd-op3-recovery-browser-audio.cpio.gz \
  artifacts/boot-oneplus3-pmos612-recovery-audio-s1302-retry.img

./scripts/verify-op3-recovery-manifest.sh --source
./scripts/verify-op3-recovery-manifest.sh --artifacts
```

The last verifier compares the generated kernel/initrd/image against the
locked values when the exact tested outputs are being reproduced. A different
compiler or a changed external input can produce a valid boot image with a
different hash; record that as a new build result instead of copying old
artifacts.

## What must survive outside the local checkout

To delete the checkout and later rebuild it, retain an external input archive
or storage location containing:

1. the SHA256-pinned v100 boot image used by `extract-reference-initrd.sh`;
2. the two SHA256-pinned ath10k `extfw` files;
3. the owner-approved Qualcomm firmware tooling/input used by
   `stage-msm8996-oneplus3-firmware.sh`;
4. the pinned Buildroot download cache only if offline builds are required;
5. the CJK font package or a copy of the verified `.ttc` file.

`local/` contains private credentials and is intentionally excluded. It must
not be copied into GitHub or into a public recovery bundle.
