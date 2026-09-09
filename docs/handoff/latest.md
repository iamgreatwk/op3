# Latest handoff

## OP3 PM8994 LVS1 SMD-RPM declaration candidate (device PASS, 2026-09-09)

The camera branch now contains nested-kernel commit `67630ec3b9e5`, archived as
`patches/pmos612-op3-camera-imx298/0013-arm64-dts-qcom-register-OP3-LVS1-via-SMD-RPM.patch`.
It registers PM8994 LVS1 below the OP3 board's actual
`&rpm_requests` / `qcom,rpm-pm8994-regulators` SMD-RPM provider, adds the
`vdd_lvs1_2-supply` input, and leaves IMX298 VIO on the known-good
`vreg_s4a_1p8`. This is an isolated regulator-registration test; it does not
change the camera driver, module, initramfs, or Buildroot.

Hypothesis: the earlier complete-DT LVS1 candidate rebooted because it used
the direct SPMI provider instead of the board's SMD-RPM topology. Build only
the DTB, reuse the locked Image.gz and Buildroot initrd, and use `fastboot
boot`; do not flash, rebuild Buildroot, or unload CAMSS. PASS requires boot to
userspace with an LVS1 regulator entry. The DTB SHA256 is
`cc1a046822cf867b6deaf082458a64f3eeb98c38358b30ad9bc8d0aefa3d39cd` and the
temporary boot image SHA256 is
`4933bbeec5631113c2a7311ee68524ed31fc78bae39f9a3da7d9cc01025b5e63`.
The phone reached userspace and exposed both `vreg_lvs1a_1p8` and `lvs1`.
This is a device PASS for isolated regulator registration, not Integration
acceptance and not an IMX298 chip-ID pass. Full commands and UART/pstore
evidence requirements are in `docs/handoff/op3-camera-imx298-001.md`.

## OP3 PM8994 LVS1 complete-DT declaration test rejected (2026-09-09)

The DTB-only candidate from nested-kernel commit `c298ff3e7197` added the
PM8994 LVS1 parent input `vdd_lvs_1_2-supply = <&vreg_s4a_1p8>` together with
the `lvs1 {}` node. Its DTB SHA256 was
`721a4a15c7366b5b3b44bfabea9f566b478d063a7fb9b036d4b93f4f5b45ab2b`; the
temporary boot image SHA256 was
`221f3077913f9ef393d57d1e2133638c694570851ca47354babd9dd08d48dc3e`.
`fastboot boot` sent the image successfully, but the phone rebooted to
fastboot before userspace, with no SSH. The missing-parent hypothesis is
therefore rejected. Keep LVS1 out of the normal recovery DT; future LVS1 work
requires UART/pstore evidence and a separate early-boot regulator diagnostic.
The camera branch was returned to the known-good no-LVS1 state by nested-kernel
revert commit `8cce8d4643b3`; the failed candidate remains in history.

## OP3 PM8994 LVS1 complete-DT declaration candidate (owner DTB build pending, 2026-09-09)

The camera branch has a new nested-kernel commit `c298ff3e7197` that adds
`vdd_lvs_1_2-supply = <&vreg_s4a_1p8>` and restores the PM8994 `lvs1 {}` child
node. This is based on the PM8994 SPMI binding and the board's known-good S4
1.8 V rail. The IMX298 `vio-supply` is intentionally still S4, so this is an
isolated regulator-registration test, not a camera VIO routing test.

Hypothesis: the prior LVS1 boot failure was caused by the incomplete node
missing its parent input supply. PASS requires boot to userspace with LVS1
registered; an early reboot or regulator probe failure is FAIL. Build only the
DTB, reuse the known-good Image.gz and Buildroot initrd, and use `fastboot
boot`; do not rebuild Buildroot or unload CAMSS. Full commands and evidence
requirements are in `docs/handoff/op3-camera-imx298-001.md`.

## OP3 IMX298 original power-resource sequence candidate (owner build pending, 2026-09-09)

The camera branch now contains commit `7fe1f2f950b2`, archived as patch
`patches/pmos612-op3-camera-imx298/0012-media-i2c-follow-op3-imx298-power-sequence.patch`.
This is one isolated kernel-camera hypothesis: the IMX298 still returns I2C
`-6` because the current mainline driver does not reproduce the original OP3
Android resource order. Preserved vendor evidence identifies VANA, VDIG, VIO,
GPIO39/CAM_VAF0, S5/CUSTOM1 at 2.15 V, 24 MHz MCLK, and RESET release with
2/5 ms delays. The candidate implements that order explicitly and reverses it
on cleanup. It keeps the known-good always-on `vreg_s4a_1p8` for VIO and does
not reintroduce the boot-crashing `lvs1` node.

The DTS and external module must be rebuilt by the project owner before this
can be tested; the prior CCI-400 boot image lacks the new `custom1-supply` and
`vaf-gpios` properties. Build/package commands and the required PASS/FAIL
evidence are in `docs/handoff/op3-camera-imx298-001.md`. No Buildroot rebuild
is needed. Until a device reports `IMX298 probe passed: chip ID=0x0298`, this
remains a candidate and the camera layer is not accepted.

## OP3 recovery USB-host independence A/B candidate (2026-09-09)

The current recovery initramfs candidate removes the USB host from the
recovery boot critical path. Previously, `init_mainline.sh` synchronously
configured the configfs RNDIS+ACM gadget, waited up to ten seconds for
`/dev/ttyGS0`, and started an interactive ACM shell before the inittab
`run_recovery.sh` entry could execute. The candidate moves that complete
optional USB debug setup to a background function and removes the direct
`log()` write to `ttyGS0`, while retaining RNDIS/ACM for debugging when USB is
available.

Only `boot/initramfs/sbin/init_mainline.sh` changes behavior; kernel, DTS,
DRM/GPU, recovery, Wi-Fi, audio, Buildroot configuration, and boot cmdline
are unchanged. Static shell and diff checks pass. A transient A/B initramfs and
boot image were repacked without a Buildroot rebuild:
`artifacts/initrd-op3-recovery-buildroot-usb-independent-ab-20260909.cpio.gz`
has SHA256
`123bfec8cb818d69aa689ef86c0edf2c7a4cf4d98de07fcf3cd951f677344837`, and
`artifacts/boot-oneplus3-pmos612-recovery-buildroot-usb-independent-ab-20260909.img`
has SHA256
`9ed9caf7efc083494c37865bf80a064b59ebcf5e8ea80a3c720f669fd9f6f267`.
The candidate is committed in
`docs/handoff/op3-recovery-usb-independent-001.md`; device validation and the
subsequent clean Buildroot rebuild are pending. The next PASS condition is a
visible recovery UI after a `fastboot flash` boot with no computer USB host
attached, followed by successful RNDIS/ACM availability when a host is
connected.

Owner then reported the no-USB-host condition working with the transient A/B
image. This is recorded as a device PASS for the isolated hypothesis, not yet
as final Integration acceptance: the host-attached RNDIS/ACM regression and a
clean Buildroot rebuild from the committed source remain pending.

## OP3 rear IMX298 probe candidate (original power-sequence test pending, 2026-09-09)

GitHub Issue #12 starts the camera layer with one isolated variable: rear Sony
IMX298 probe support. The dedicated nested kernel worktree is
`source/linux-pmos-msm8996-6.12-camera-imx298` on
`agent/implementation/op3-camera-imx298-001`, based on the integrated kernel
checkpoint `4f8595b13fbd`. The prior no-`LVS1` control checkpoint ends at
`306d4a364565` (the direct-`LVS1` change was `ec5025c75ff4`); the current
camera tip is `7fe1f2f950b2`.

The candidate adds a probe-only V4L2 driver, a binding, and OnePlus 3 15801
CCI0/CAMSS DT wiring. It uses CCI address `0x1a`, GPIO30 reset/XCLR, GPIO13
MCLK0, 24 MHz, four CSI-2 lanes, and the documented 1.1 V / 1.8 V / 2.6 V
rails. The current DTS sets CCI0 to 400 kHz, matching the old OP3 Android
camera stack's fast mode, keeps camera VIO on the legacy always-on
`vreg_s4a_1p8`, and removes the experimental PM8994 `lvs1 {}` child node. It
does not add Android camera blobs or mode tables, and it does not touch the
front IMX179, OIS, actuator, flash, EEPROM, or camera userspace.

The owner-only DTB build for `ec5025c75ff4` passed, but the corresponding
direct-`LVS1` boot image rebooted before userspace. The follow-up control image
from `a112a6f19fa2`, which retained only the `lvs1 {}` node while restoring the
old camera VIO mapping, also rebooted. The old probe image boots on the same
packaging path, so the failure is isolated to the new regulator node. The
camera branch checkpoint `306d4a364565` removes that node and booted
successfully. A direct device test
of the previous DTB loaded the full module closure in dependency order; CAMSS
created `/dev/video0`–`/dev/video5`, but the sensor still returned I²C error
`-6` and did not bind. The failed temporary boot image was
`artifacts/boot-oneplus3-pmos612-recovery-imx298-vio-lvs1.img` with SHA256
`bb71fb07cd3461fdf3bd79ee272779db5f306f4d4c7f8e2497b6eecb36f79c43`.
The successful no-`LVS1` control boot image has SHA256
`176885492e91e1ac308bac146fa62f8daa5ad01d27335aa6f85e6fe233e932f4`; its
DTB SHA256 is
`51f494ef4f0a20697b0aecd8d4edbdd376a1614eda4959ef934735f4b75f4df3`.
The prior 1 MHz probe returned I²C `-6`; commit `b0594dbd5bf4` changes only
CCI0 to 400 kHz for the next test. Use the candidate fragment
`kernel/configs/oneplus3-recovery-imx298-probe.fragment` and the complete
handoff at `docs/handoff/op3-camera-imx298-001.md`. The next PASS condition is
a clean `IMX298 probe passed: chip ID=0x0298` log and a registered V4L2 sensor
sub-device; capture is deliberately a separate follow-up. The 400 kHz DTB has
been built with SHA256
`067f4788407299dfe4dca1014a666e0e383036276b928848e7a06f87b0482588` and
packaged as
`artifacts/boot-oneplus3-pmos612-recovery-imx298-cci400.img` with SHA256
`ba95c78431c13b681ddb7780bb6bf8412f7566ff7de3a68cefe8f6530e6a5295`; it has
been tested on the device. All eleven camera modules loaded successfully and
CAMSS exposed `/dev/video0`–`/dev/video5`, but IMX298 still returned I²C
`-6` while reading its chip ID. The CCI-speed hypothesis therefore failed.
The module-only diagnostic run is complete. Nested-kernel commit
`c79909f9a448` changes only `drivers/media/i2c/imx298.c` logging: reset logical
state, each rail's enabled/voltage state, bulk-enable result, MCLK result and
rate, and separate `0x0016`/`0x0017` read results. It leaves the existing
`GPIOD_OUT_LOW`, power sequence, 400 kHz CCI DTS, and no-`LVS1` configuration
unchanged. The diagnostic patch is archived as
`patches/pmos612-op3-camera-imx298/0010-media-i2c-add-imx298-power-on-diagnostics.patch`.
The owner-built module SHA256 is
`d9b5ea7a08a4a11840f95777f272eceb73750e8a9b16228d5b38ea3ce417c70f`. On
`192.168.1.4`, the module loaded successfully and logged VDIG `1.1 V`, VIO
`1.8 V`, VANA `2.6 V`, and MCLK `24 MHz`; reset was initially logical `0`
before assertion and the chip-ID read still returned `-6`. This is a
diagnostic evidence PASS, not an IMX298 probe PASS. Do not rebuild or
repackage Buildroot, the DTB, or the boot image. The reset-initial-state test
was run as nested-kernel commit
`d247ce811242`, changing only `GPIOD_OUT_LOW` to `GPIOD_OUT_HIGH` when the
reset GPIO is requested. On `192.168.1.4`, the module loaded successfully and
the diagnostic log showed reset-before-assert logical `1`, but the chip-ID
read still returned `-6`. The reset hypothesis therefore failed. Commit
`7fe1f2f950b2` now implements the preserved old OP3/Android IMX298
power-resource order, including auxiliary S5/CUSTOM1 and GPIO39/VAF, while
keeping `vreg_s4a_1p8` for VIO and leaving `lvs1` disabled. The owner build and
device test are pending; see `docs/handoff/op3-camera-imx298-001.md`.

## OP3 recovery balanced CPU governor fix (userspace candidate device-tested, Buildroot pending, 2026-09-08)

The recovery userspace candidate changes one value only: the three-position
balanced mode now requests `schedutil` instead of `interactive`. The current
6.12 device exposes `schedutil` but not `interactive`; before this fix,
`/root/tri_mode=1` left both cpufreq policies in the screen-off `userspace`
policy at 307.2 MHz after wake because the failed governor write was ignored.
The source fix is committed as `4dff12f`. A small userspace-only candidate was
uploaded and atomically replaced on the running device; its SHA256 is
`de52b2e0e4b8b24a55348547e36c3a91160b64963f337099b32201c264519ae7`. The new
process was verified from `/newroot/sbin/recovery_mainline`. With
`/root/tri_mode=1`, both CPU policies reported `schedutil` while the screen
was on; a physical power-key cycle verified wake restoration to `schedutil` and
screen-off entry into `userspace` at `307200` kHz for both clusters. The old
binary remains recoverable at
`/newroot/sbin/recovery_mainline.before-schedutil-20260908`.

No kernel, DTS, DRM, S1302, or Buildroot configuration was changed. Buildroot
has deliberately not been rebuilt yet; this transient test binary is not the
final initramfs artifact. Final Buildroot integration remains pending after the
remaining test series.

## OP3 S1302 polling interval 100 ms candidate (kernel/device scoped PASS, Buildroot pending, 2026-09-08)

The formal kernel worktree has one isolated DTS change after the current
integrated checkpoint: `polling-interval-ms = <30>` -> `<100>` for the S1302
capacitive-key controller. Kernel commit `4f8595b13fbd` and tree
`5633301bb0fa5f05254d0d48d0c55cfed81a40e2` are locked in the manifest, with
patch 0033 added to the durable restore series. The owner kernel build passed
in `out/pmos-msm8996-6.12-recovery-audio-full-s1302-poll100-drm100`.
`Image.gz` SHA256 is
`aac420e188dd2ede0e0ade0e42fb110af6f5d03643d2d22be2581c9cdc03233a`, and the
OP3 DTB SHA256 is
`acf85fd6ae148861374ec4d65feee0e3d909cce9b75e96d09c2f44a102914d1b`.
The owner then used temporary `fastboot boot` with the existing GPU-idle
Buildroot initramfs (no Buildroot rebuild). Dmesg reported `polling=100 ms`.
IRQ87 (`i2c_qup`, BLSP2 I2C2) increased from `11398` to `11793` in 10 seconds,
about `39.5 IRQ/s`, below the old 30 ms reference of about `118–121 IRQ/s`.
The recovery log recorded complete press/release pairs for both S1302 key
codes `158` and `580`. The temporary boot image
`artifacts/boot-oneplus3-pmos612-recovery-s1302-poll100-gpu-auto-transient.img`
has SHA256
`6825245ee29bfa4009cf65fb4bbc8eabf33ed2a1f2980640bebd2988642583b9`.
This is a scoped device PASS; final Buildroot integration and Integration-role
acceptance remain pending.

The current image reference sample (30 ms polling, screen off) showed CPU
about 92% idle, recovery at 0% CPU, 5.5 GB available memory, and GPU
`auto/suspended`. The S1302 BLSP2 I2C2 controller IRQ increased by about 118/s
over 10 seconds. The owner then woke the panel with the physical power key and
a 30-second screen-on sample recorded about 90% → 92% CPU idle, recovery at
0% CPU, GPU `auto/suspended`, and about 28.4 seconds in `cpu-sleep-0`. IRQ87
increased by about 121/s, essentially unchanged from screen-off; DSI IRQ
increased only six times. No separate high-CPU or active-GPU consumer was
observed.

## OP3 PSCI CPU idle and power-key wake verified (wall-charger reference baseline, 2026-09-08)

The next experiment changes only the formal recovery kernel configuration:
`CONFIG_ARM_PSCI_CPUIDLE=y`. The OP3 device tree already contains the PSCI
`CPU_SLEEP_0` state (`standalone-power-collapse`), while the previous running
image reported `current_driver=none`. No DRM, refresh, input, Wi-Fi, audio, or
system-suspend behavior was changed.

The owner build completed from the separate output directory
`out/pmos-msm8996-6.12-recovery-audio-full-psci-cpuidle`; the generated
configuration contains both `CONFIG_ARM_PSCI_CPUIDLE=y` and the automatically
selected `CONFIG_ARM_PSCI_CPUIDLE_DOMAIN=y`. The locked hashes are config
`6f8efe25de1c7f64af15002f46e180b8cc0a8c214508e880d5008180bc1c23a9`, Image.gz
`edb7c939018a4ddfaa816adb34971eb57a23c09801e4b04a6e20438bad7eef42`, DTB
`87aff2df7ef853966f88f9bb07488cffb3315dfe407bef801032deb319ff88bd`, and
boot image `10e34f455707bac3060f5d58edc6d589c8ba6e7b5578a896350326782a93bbaa`.
The existing Buildroot initrd remains fixed. The owner booted this image and
read-only SSH verification confirmed `current_driver=psci_idle`; the
`cpu-sleep-0` state accumulated about 14 seconds during a 15-second idle
sample, and the PM8941 power-key wakeup attribute is `enabled`. The owner then
confirmed physical power-key息屏/唤醒; the recovery log contains complete
`KEY_POWER` press/release pairs and the final backlight is `255`. A valid
30-second screen-off reference sample was then run with a wall charger and
SSH over Wi-Fi: charging remained `Charging`/`Fast` at a 3 A input limit, battery
temperature changed from 35.3 C to 35.2 C, and the GPU remained
`auto/suspended`. `cpu-sleep-0` accumulated about 29.0 seconds during the
30-second interval. These values are a reference for tuning the current
version; no old-kernel comparison or thermal-improvement claim is made. The
earlier USB-host sample was kept separate because it was limited to 500 mA and
the battery was discharging.
Full system suspend is deliberately not part of this experiment.

## OP3 recovery DRM idle wakeup and S1302 IRQ mitigation packaged (device test pending, 2026-09-08)

Two independent changes are now packaged for the next integrated recovery test.
The recovery userspace commit `e11a9dc` changes only the screen-on idle
`poll()` timeout from 20 ms to 100 ms. Input and PTY events still wake the
poll immediately; the goal is to reduce idle CPU wakeups while retaining the
direct CPU-rendered DRM UI. The previous sample showed about 51 voluntary
context switches/s in the 20 ms loop, while the GPU was already runtime
suspended.

The formal kernel commit `21a64a8c2a20` changes only S1302 event delivery on
the OP3: because GPIO132 was observed low and IRQ88 increased by about 784/s,
the DT now selects a 30 ms I2C polling worker instead of requesting the
level-low IRQ. The existing EV_KEY mappings and chin-key behavior are kept.
The change is archived as patch 0032 in
`patches/pmos612-op3-recovery-audio-full/`; the kernel tree lock is
`afa264a577f7f531159816f2d62c5173c87af735`.

The first Buildroot attempt was stopped by the host's uutils `install`; the
resumed attempt then exposed a stale firmware staging directory without the
required QCA6174 `board.bin`. The documented recovery is to put GNU `install`
first in `PATH`, stage a fresh versioned firmware directory, and resume the
existing Buildroot output. The corrected incremental build completed and
produced a valid `rootfs.cpio.gz`.

Locked build outputs are:

- `Image.gz`: `5c89259d9340071c9c8684d361042482ca25c8f76b2de4d33cdacf2105f78861`
- OP3 DTB: `264f981678c1dd8d1d9a52f2db6e2130a0ebccbb9f4485ab8740784f73806db7`
- Buildroot initramfs: `27736d1d662158bedd5170c033f9b73d807d559a564d3fd6c9090438b6d0f968`
- boot image: `fcd5e6bd476467e09abfbcbea3dcafd206b0ae98e60a17165a697fd87f4239c5`

The initramfs passed `gzip -t`; the boot image was inspected as an Android
boot image and uses the device-specific `sda15` UUID from
`boot/oneplus3-fa5.env`. Device testing remains pending until charging is
complete. Handoffs: `docs/handoff/op3-recovery-drm-idle-poll-001.md` and
`docs/handoff/op3-recovery-s1302-irq-storm-001.md`.

## Default recovery leaves GPU runtime-PM unchanged (2026-09-07)

The default `boot/initramfs/sbin/run_recovery.sh` no longer writes `on` to
`/sys/bus/platform/devices/b00000.gpu/power/control`. The standard recovery
profile does not launch a browser, so forcing the A530 runtime active only
adds heat. Recovery now logs the current policy and leaves it at the kernel
default, allowing the display hardware to suspend when idle. The optional
`boot/recovery-browser-test` launcher keeps its separate GPU-on workaround for
future browser experiments.

This is a default initramfs userspace change only. It requires a new Buildroot
initramfs and boot image before device testing. The new image was built,
flashed to the non-A/B `boot` partition, and booted successfully. Device
evidence shows `control=auto`, `runtime_status=suspended`, and
`cur_freq=27000000`; the A530 is now allowed to idle when recovery is not
actively using it. The same boot retained `/dev/sda15` mounting, SSH
`root/1234`, direct DRM display, and the OnePlus3 ALSA card.

The GPU-idle initrd SHA256 is
`4f788efb94a267130df19db23d436eb9f2546417e1c659bc777bf9ced686e93a` and the
flashed boot image SHA256 is
`11c376755b61300c45112fe788968c3e6dd0fc58bb6616d7877debf03734cfdf`.

## Boot partition flashed and post-flash verification (2026-09-07)

The owner-authorized image
`artifacts/boot-oneplus3-pmos612-recovery-buildroot-board-fallback-passwd.img`
was written only to the non-A/B `boot` partition. Fastboot reported both
`Sending 'boot' OKAY` and `Writing 'boot' OKAY`; no other partition was
modified. The image is 54 MiB and fits the verified 64 MiB boot partition.

After `fastboot reboot`, the flashed image accepted SSH login as `root` with
password `1234`, mounted `/dev/sda15` using UUID
`feba81cd-3eee-4971-a703-a7d80dd04b5a`, and exposed the persistent recovery
rootfs. `wifi_auto` loaded the ath10k module closure and `wlan0` appeared. In
the limited post-flash wait, the driver performed authentication attempts but
the state was still scanning; automatic Wi-Fi association is therefore left
INCONCLUSIVE for this run. Manual `wifi reconnect 1106` is the next test.

## Password-enabled Buildroot recovery boot and persistent rootfs replacement (2026-09-07)

The recovery Buildroot defconfig was corrected and rebuilt after the first
incremental build was found to still use the stale generated
`source/buildroot/configs/op3_recovery_defconfig`. The project-owned template
and the generated defconfig now both set `BR2_TARGET_ENABLE_ROOT_LOGIN=y` and
the SHA-512 crypt hash for the lab password `1234`. The generated target
contains the expected non-empty `/etc/shadow` root entry.

The owner-authorized test image was booted successfully with `fastboot boot`.
SSH login as `root` with password `1234` passed. The booted initramfs exposed
`wlan0`, the OnePlus3 ALSA card, and `/newroot` mounted from `/dev/sda15`.
The exact partition guard passed for UUID
`feba81cd-3eee-4971-a703-a7d80dd04b5a` before modifying the filesystem.

The persistent rootfs on `/dev/sda15` was cleared and replaced from
`artifacts/op3-audio-rootfs-board-fallback-passwd.tar.gz`. Post-extraction
checks passed for the root password hash, `/newroot/sbin/recovery_mainline`,
the QCA6174 `board.bin`, `/newroot/opt/op3-wifi/wifi`, and TinyALSA tools.

After provisioning the device-local Wi-Fi profile with `wifi connect`, the
phone was returned to bootloader and booted with the same image a second time.
The second boot again mounted `/dev/sda15`, accepted SSH `root/1234`, and
`wifi_auto` loaded the complete ath10k module closure. `wlan0` automatically
associated with the saved `1106` profile, obtained `192.168.1.5` and a default
IPv4 route, with IPv6 still disabled. The ath10k log confirms the expected
`board-2.bin` miss for the `0000:0000` subsystem followed by successful
`board_file` fallback and interface association.

Build outputs and hashes:

- kernel commit `4a486e2ea7e46622d68ae039a2ccf2db909ab99d`, `Image.gz`
  `835480696c9318e7c8d4895dcba15363ecd2b8b6463c870f750c0134cc8b8d3d`
- DTB `264f981678c1dd8d1d9a52f2db6e2130a0ebccbb9f4485ab8740784f73806db7`
- initrd `087f6a56b6a3c010cc219894f1f41ed23ffabba1d0c58203d710092aca92ba76`
- persistent rootfs TAR `27aebfc29509e1b1618cdd7c53a0c490e5b4126256168f092146f3bb2ee41807`
- boot image `c39acb3cd79421523f25c45d939d4b173fe3ca2d71b0ca3957ceb798236d354c`

Wi-Fi credentials remain device-local by design and are not included in the
rootfs artifact. The test device now has its profile under
`/newroot/etc/op3-wifi/profiles`; a newly formatted device must run
`wifi connect` once after boot if automatic Wi-Fi association is required.

## OP3 corrected Wi-Fi board-fallback image built (device retest pending, 2026-09-06)

The owner-authorized corrected build completed successfully on the project
branch. Kernel `Image.gz` and DTB were built from the formal integrated kernel
worktree, and Buildroot generated a new CPIO with the Wi-Fi module closure,
TinyALSA tools, recovery program, and QCA6174 `firmware-6.bin`, `board-2.bin`,
and legacy `board.bin`. The new initrd passed `gzip -t`; its SHA256 is
`315cc923a43d5caded31612cfc043285d552e339adac60b6c40c6684c09eede1`.

The test boot image is
`artifacts/boot-oneplus3-pmos612-recovery-buildroot-board-fallback.img` with
SHA256 `9ecef144150562b6eae47b34b54df5929a34af304303af11d07e391af851a2a0`.
Its boot command line uses the device-specific `sda15` UUID from
`boot/oneplus3-fa5.env`. The persistent target tarball is
`artifacts/op3-audio-rootfs-board-fallback.tar.gz` with SHA256
`631ca41fa6ceb07c5dfa4f1e3cb182130cae985532efb12b78c90f9503e6f957`.
The canonical manifest and previously tested artifacts remain unchanged until
this corrected image is boot-tested. Handoff:
`docs/handoff/op3-wifi-board-fallback-build-001.md`.

## OP3 QCA6174 board-data fallback prepared (device retest pending, 2026-09-06)

ACM diagnostics isolated the Wi-Fi failure to firmware board-data selection,
not to the interface name, module closure, PCI binding, or `wifi` executable
path. The device reports PCI subsystem `0000:0000`; its `board-2.bin` contains
no matching entry, and the kernel log ends with `failed to fetch board-2.bin or
board.bin`, `failed to fetch board file: -2`, and `could not probe fw (-2)`.

The 8124-byte `board.bin` fallback was extracted from the retained historical
reference initrd and matches the host firmware package. It is now recorded in
the external-input SHA256 manifest with hash
`1a8d225818b46986fc4f615594fbe448fa820618590d6902c8f844bb37cda667`. Kernel
`CONFIG_EXTRA_FIRMWARE`, kernel restoration, initramfs firmware staging, and
Buildroot post-build validation all require and carry both `board-2.bin` and
`board.bin`. A temporary ACM copy followed by PCI unbind/bind made `wlan0`
appear; `wifi reconnect 1106` then obtained DHCP address `192.168.1.5` with
IPv6 disabled. This proves the isolated firmware fix, but the copy is not
persistent and no rebuilt kernel/initramfs has been boot-tested yet. Handoff:
`docs/handoff/op3-wifi-board-fallback-001.md`.

The follow-up staging check also found that `stage-op3-initramfs-firmware.sh`
double-prefixed `ath10k` when only `OP3_EXTERNAL_INPUTS` was set. Commit
`b17e838` accepts either the external-input root or its direct `ath10k`
subdirectory; the staging test now passes with the documented environment.

## Clean rebuild artifact check (2026-09-06)

The owner-authorized clean rebuild in
`/home/kai/op3-rebuild-clean-20260906` now passes source and artifact
verification. The Buildroot recovery initramfs is 44,801,175 bytes with SHA256
`3a704c8f64dde483204f4391997cb60230bf736478009330bf27df2085e0bf6c`. The
reproducible kernel CPIO has SHA256
`e15eb1c349081c2650e535ec9774c6ed0afa178226ec89af09eed62be21e14c9`, and
`Image.gz` matches the locked SHA256
`5c89259d9340071c9c8684d361042482ca25c8f76b2de4d33cdacf2105f78861`. The
boot image was repacked successfully with SHA256
`f0aed8d6e62c6702f68b28003eebc657ef0871d88d4aa3769f21a0dbd13fed46`.
`verify-op3-recovery-manifest.sh --source` and `--artifacts` both pass. No
device boot test has been run with `fastboot boot`; basic recovery startup
passed, while Wi-Fi and persistent-root behavior remain INCONCLUSIVE.

The device reported Linux `6.12.1-msm8996+ #1 SMP PREEMPT Sun Sep 6
14:36:13 CST 2026`, DRM first-frame activation, the OnePlus3 ALSA card with
MultiMedia1--3 playback/capture, S1302/volume/tri-state/power/touch input
devices, and haptics input. Recovery audio produced a 137,324-byte
`/tmp/voice.wav` and invoked `tinyplay`. Early `/dev/sda15` mounting failed,
although a later manual mount succeeded; Wi-Fi module loading from `/lib`
worked but the PCIe log reported `Phy link never came up`, so no `wlan0`
appeared. SSH password access was unavailable because the Buildroot root
shadow entry is empty; ACM serial remained available.

## UUID-selected persistent rootfs deployment and boot test (2026-09-06)

The device-specific persistent partition was verified before the destructive
deployment: `/dev/sda15` had UUID
`feba81cd-3eee-4971-a703-a7d80dd04b5a` and was mounted at `/newroot`. Its old
contents were cleared and the owner-built Buildroot `rootfs.tar` was
extracted there. The deployed UUID-trial TAR had SHA256
`24f8b2367e91ec8294f74c4c3d1b236ee6cdbac90150ca5e0e88edffaee495da`.

The project branch now contains the UUID mount fix in commits `c941062` and
`ec52894`. `boot/oneplus3-fa5.env` supplies
`pmos_root_uuid=feba81cd-3eee-4971-a703-a7d80dd04b5a`; initramfs resolves
that UUID with `blkid`, creates `/newroot`, and retries the mount for up to 15
seconds. The first UUID trial exposed the missing mountpoint; the second
trial included the one-line correction.

The final test image was booted with `fastboot boot` and has SHA256
`f91a2a69897fae661ba2efe19b664d8ae68779d58dc12767b54aef197186e982`; its
Buildroot CPIO has SHA256
`31f3465000b26382a4640799f5ca58b5b8986a92088fcca4dbcff514d6262612`.
The device cmdline contained the expected UUID, and the boot log recorded:
`newroot mounted (/dev/sda15 UUID=feba81cd-3eee-4971-a703-a7d80dd04b5a)`.
`mountpoint /newroot` passed and `recovery_mainline` resolved to
`/newroot/sbin/recovery_mainline`.

The same boot discovered touch, power, gpio-backed tri-state and volume,
S1302 capacitive keys, and `spmi_haptics`; ALSA exposed the `OnePlus3` card
with MultiMedia1--3 playback/capture. This is a mount/integration PASS for
this run; Wi-Fi association and a fresh audio capture/playback run remain
separate tests.

## Buildroot GNU mirror timeout (2026-09-06)

After the local GNU `install` workaround, the clean Buildroot run proceeded
through `host-libtool`. The default `ftpmirror.gnu.org` then returned HTTP 504
for `autoconf-2.72`; Buildroot's fallback source is reachable, but using it
only after the mirror retry makes the build unnecessarily slow. The rebuild
guide now passes `BR2_PRIMARY_SITE=https://sources.buildroot.net`; rerunning the
same command resumes from the existing `dl/` and output directories. No final
Buildroot artifact exists yet.

## Buildroot host preflight workaround (2026-09-06)

The clean rebuild reached Buildroot `op3_recovery_defconfig` successfully.
The first full-build invocation stopped before compiling project packages
because this host resolves `install` to uutils coreutils 0.8.0. A local
`host-tools/install` symlink to `/usr/bin/gnuinstall` was prepared in the clean
rebuild directory; the owner can resume the existing Buildroot output by
prepending that directory to `PATH`. The workaround is now documented in
`docs/rebuild-recovery.md`. No Buildroot artifact or device test exists yet.

## Fresh kernel reconstruction uses tree identity (2026-09-06)

The clean rebuild rehearsal exposed and corrected a false failure in source
verification. The archived 31-patch series reconstructs the exact locked
kernel tree `dd6476a68184e7293b05a7e962f0536f0d54048a`, but patch mail does not
carry the historical committer timestamps, so a fresh `git am` checkout can
have a different final commit SHA from the previously tested worktree. The
restore script now uses author dates for deterministic new-machine commits,
and the manifest verifier accepts the reconstructed commit when its tree,
baseline ancestry, and branch all match. No kernel build or device test was
run.

## OP3 initramfs source migration prepared (owner build and device test pending, 2026-09-06)

The canonical recovery initramfs is now intended to come from the project-owned
Buildroot package op3-initramfs and its CPIO/GZIP output
out/buildroot-op3-recovery/images/rootfs.cpio.gz. Tracked sources under
boot/initramfs provide /init, inittab, init_mainline.sh, the recovery launcher,
audio initialization, feed_entropy, and the diagnostic nc helper. The default
Buildroot profile enables CPIO_FULL and CPIO_GZIP while retaining TAR for the
same target's optional persistent /newroot payload.

The post-build hook now requires artifacts/op3-initramfs-firmware and copies
the hash-verified A530, Qualcomm, and ath10k firmware into the Buildroot
target. Wi-Fi scripts support both /newroot and the CPIO root, with IPv6 still
disabled by default. The historical v100 boot image and standalone reference
initrd are no longer operational inputs for this path.

No Buildroot build, kernel build, boot-image packaging, or device test was run
by this checkpoint. Handoff: docs/handoff/op3-initramfs-source-001.md.

## OP3 v100 boot image retained as historical provenance (2026-09-06)

Earlier work extracted the gzip-compressed ramdisk from the hash-verified
62,226,432-byte `boot_fa5_v100_auto.img` and stored it outside GitHub as
`/home/kai/op3-recovery-external-inputs/initrd/reference-initrd.img`.
That 50,705,116-byte file and the original image under `archive/` are retained
only for historical comparison. They are not required by the current rebuild,
are excluded from the external required-input checksum manifest, and must not
be passed to the current initramfs preparation or packaging commands.

The current flow does not consume the old kernel payload, appended DTB, boot
header, cmdline, or ramdisk. The old extraction/reserialization scripts and
their handoffs remain historical records, not the canonical build path.

## OP3 default Buildroot profile excludes browser (2026-09-06)

The default Buildroot profile is now `buildroot/op3-recovery.defconfig` and
is installed as `op3_recovery_defconfig`. It keeps TinyALSA and the recovery
diagnostic tools, and now integrates the Wi-Fi userspace and matching kernel
module closure through the recovery post-build hook. It does not select
Mesa/Freedreno EGL/GLES, Weston DRM, WPE WebKit, Cog, or WPEWebDriver. The
existing `buildroot/op3-browser.defconfig` remains an explicit opt-in profile; use
`scripts/prepare-op3-buildroot.sh source/buildroot-browser browser` with a
separate Buildroot source/output tree when browser testing is needed. No
kernel, recovery runtime, or device behavior changed by the browser profile.
Handoff:
`docs/handoff/op3-recovery-default-profile-001.md`.

## OP3 Wi-Fi integrated into default Buildroot target (2026-09-06)

The canonical recovery build no longer requires a separate Wi-Fi bundle. The
default `op3_recovery_defconfig` enables Buildroot `wpa_supplicant`/`wpa_cli`,
`iw`, and the wireless regulatory database. A tracked post-build hook copies
the Wi-Fi CLI/scripts and the dependency closure for the owner-built kernel's
`cfg80211`, `rfkill`, `mac80211`, `ath`, and `ath10k` modules into the same
Buildroot target that is later staged with the audio/recovery payload. The
intermediate `artifacts/op3-wifi-modules-root` is still required after the
kernel `modules_install` step, but `scripts/stage-op3-wifi-rootfs.sh` is kept
only for compatibility with older images. No browser packages were enabled in
the default profile. Handoff:
`docs/handoff/op3-wifi-buildroot-001.md`.

## OP3 recovery bundle integrated into default Buildroot target (2026-09-06)

The default recovery Buildroot profile now includes the project-owned
`op3-recovery` package. During the Buildroot build it compiles the tracked
direct-DRM `recovery_mainline` and installs `/sbin/recovery_mainline`, the
`browser` command, the browser-session supervisor, and both browser runner
scripts into the same persistent target that already receives audio and
Wi-Fi. The old standalone recovery stager and recovery-audio initrd overlay
remain compatibility tools only. The browser runtime itself remains an
explicit separate bundle. No kernel or device test was run for this
packaging change. Handoff:
`docs/handoff/op3-recovery-buildroot-001.md`.

## OP3 external input bundle (2026-09-06)

All binary inputs that must survive deletion of the checkout are organized in
`/home/kai/op3-recovery-external-inputs`. It contains the reference v100 boot
image, ath10k files, Qualcomm `NON-HLOS.bin` and `a530_zap.elf`, A530 GPU
firmware, CJK font/package, `mcopy`, and the pinned `pil-squasher` source and
tools. SHA256/SHA512 manifests and a README are stored in that directory.

Verify and export it before rebuilding:
`./scripts/verify-op3-external-inputs.sh /home/kai/op3-recovery-external-inputs`.
The rebuild scripts now consume `OP3_EXTERNAL_INPUTS` and the explicit tool
overrides. The directory is outside GitHub and was not added to the public
repository.

## OP3 recovery clean-rebuild provenance (2026-09-06)

`docs/rebuild-recovery.md` now gives the complete current build order for
Buildroot-generated initramfs, external firmware, browser, Wi-Fi, audio,
recovery bundles, and final boot-image packaging. Buildroot is pinned to commit
`679b9ead7620bbf193620d1ebf56f53c1764d37a`; its project-owned Cog patches are
archived on the current GitHub branch. The default `op3_recovery_defconfig`
explicitly enables TinyALSA plus `tinycap`/`tinymix`/`tinyplay` while leaving
Mesa/Freedreno, Weston, WPE WebKit, Cog, and WPEWebDriver disabled; the
browser stack remains an explicit opt-in profile. New preparation scripts
restore the Buildroot source, extract the hash-pinned historical initrd from
the external v100 boot image, and stage the audio target bundle.

This is a source/provenance checkpoint only: no large build, device test, or
local-directory deletion was performed. Qualcomm/A530/ath10k firmware inputs
and the optional browser font remain external and must be retained outside
GitHub; the historical v100 image and extracted initrd are not current
rebuild inputs. Handoff: `docs/handoff/op3-rebuild-provenance-001.md`.

## OP3 recovery integrated rebuild organization (2026-09-06)

The current rebuild is locked by
`manifests/op3-recovery-audio-full.env`. It selects the top-level recovery
branch `agent/implementation/recovery-browser-001`, the separate kernel
worktree `source/linux-pmos-msm8996-6.12-recovery-audio-full` at commit
`4a486e2ea7e4`, the owner output directory, boot profile, recovery initrd, and
the latest tested artifact hashes. Run
`scripts/verify-op3-recovery-manifest.sh --source` before building and
`scripts/verify-op3-recovery-manifest.sh --artifacts` after packaging. Old
worktrees, `out/`, `cache/`, and historical artifacts remain outside the
rebuild input and are not copied into it.

The independent kernel source is now reproducible from GitHub: the 31-patch
archive is in `patches/pmos612-op3-recovery-audio-full/`, the tested full
configuration is `kernel/configs/oneplus3-recovery-audio-full.config`, and
`scripts/restore-op3-recovery-kernel.sh` reconstructs a fresh kernel worktree
from `KERNEL_BASE_COMMIT`. The restored tree must equal manifest
`KERNEL_TREE`. ath10k firmware remains an external, SHA256-pinned input and is
not committed. Archive reconstruction was tested locally by applying all 31
patches and matching the expected tree ID; this organization change did not
run a kernel build or a device test.

## OP3 recovery S1302 startup retry follow-up (registration PASS, physical test pending, 2026-09-06)

The final audio-integrated image registered the audio card and `/dev/snd`
devices, but its S1302 probe failed before input registration:
`op3-capkey-s1302 3-0020: error -ENXIO: failed to read initial key state`.
The DTB still contains the expected `75b6000.i2c/capkey@20` node, and the
driver object is identical to the earlier standalone capkey image that passed
physical testing.  This isolates the current regression to the integrated
image's S1302 power/reset startup timing; recovery is not receiving the chin
keys because no S1302 input device exists.

Kernel commits `c15090406a2a` and `4a486e2ea7e4` add bounded retries only to
the initial S1302 I2C read: up to eight retries at 25 ms for transient
`-ENXIO`, `-EREMOTEIO`, or `-EIO` results.  Runtime IRQ reads remain
single-shot; audio, DTS wiring, recovery userspace, and key mappings are
unchanged.  The owner must build
`source/linux-pmos-msm8996-6.12-recovery-audio-full` on
`agent/implementation/recovery-browser-audio-full-001`, repack the image,
and repeat the S1302/recovery event checks.  The owner has now booted the
retry image: the first S1302 read returned `-6`, retry 1 succeeded, dmesg
reports `S1302 capacitive keys ready (irq=88)`, `/dev/input/event2` is
`op3-capkey-s1302`, and recovery reports `cap=10`.  Registration is therefore
PASS; physical left/right press and release evidence is still pending.  Handoff:
`docs/handoff/op3-recovery-capkeys-audio-regression-001.md`.  This is
INCONCLUSIVE pending physical key evidence.

## OP3 recovery ALSA sound-card restore checkpoint (owner build passed, device test pending, 2026-09-06)

The current device image has the recovery audio tools, but no ALSA card:
`/dev/snd` contains only `timer`, `/proc/asound/cards` reports
`--- no soundcards ---`, and `msm-snd-apq8096` aborts while parsing
`MultiMedia4`. The first attempted fix (`228b319`) was rejected: adding
q6asm child IDs 3--15 produced repeated `valid dai id not found:0`, DAI
registration `-12`, and still no ALSA card. That patch has been removed from
the active preparation path.

The final kernel integration worktree
`source/linux-pmos-msm8996-6.12-recovery-audio-full` is based on the already
validated `agent/implementation/op3-audio-mic-001` branch, which keeps valid
q6asm MM1--MM3 sessions and disables unavailable MM4--MM16 links. It then
adds the validated S1302, volume/tri-state, and PM8994 haptics commits. This
is the final kernel integration branch; no additional feature branch should be
selected. Kernel branch: `agent/implementation/recovery-browser-audio-full-001`, HEAD
`9491be0d6460`. The first owner build exposed and the agent fixed a missing
DTS closing brace (`9f81c0cd4289`, `9491be0d6460`); the worktree is now clean.
The next build exposed missing ignored ath10k `extfw` inputs; both required
files are now staged locally in the final worktree. The owner then completed
the kernel build successfully: `.config` `c3da142e…`, `Image.gz`
`504ca5bc…`, and OP3 DTB `264f9816…`. Device boot and ALSA enumeration are
still pending. Final test image:
`artifacts/boot-oneplus3-pmos612-recovery-audio-full-v2.img`, SHA256
`2a979d0f…`; final initrd:
`artifacts/initrd-op3-recovery-browser-audio.cpio.gz`, SHA256 `c0f0a2b4…`.
Handoff: `docs/handoff/op3-recovery-audio-card-001.md`. This checkpoint is
INCONCLUSIVE pending device evidence.

## OP3 recovery voice capture/playback userspace preparation (owner test pending, 2026-09-06)

Commit `2eab778` updates the recovery microphone-key flow to use the verified
AMIC4 -> MultiMedia1 `tinycap` route, remove a stale `/tmp/voice.wav` before
capture, validate the WAV after stopping, and write independent capture and
playback diagnostics to `/tmp/op3-recovery-audio.log`. Playback keeps the
optional `pcm-wav` path when present and falls back to the `tinyplay` already
present in the current audio payload; no kernel, DTS, or Buildroot audio
sources were changed. The documented interaction is a two-second microphone
long press to start, then a tap to stop and replay.

The small static AArch64 recovery compile passed. Binary SHA256:
`a82e2ff356e0447ecce422b372dbc134c23658c9bd08d4f5ebd48db41368f300`.
Staged package:
`artifacts/op3-recovery-browser-audio-bundle.tar.gz`, SHA256
`a744c2ea4112d3551681e6aaa42ed2ec86ca14c712ecab284760134a76832fe8`.
The current audio rootfs has `tinycap`, `tinymix`, and `tinyplay`, but not
`pcm-wav`. Owner deployment and device recording/playback validation are
pending; external speaker routing remains a separate uncertainty. Handoff:
`docs/handoff/op3-recovery-audio-001.md`. This checkpoint is INCONCLUSIVE.

## OP3 recovery kernel haptics device PASS (Integration pending, 2026-09-06)

Commit `d31f471` prepares a separate formal pmOS MSM8996 Linux 6.12.1 patch
series for the missing OnePlus 3 vibration device. Patch 1 allows the
existing `qcom-spmi-haptics` driver to accept the ERM actuator type; patch 2
enables the existing `pmi8994_haptics` peripheral in the OP3 DTS and sets a
5 ms wave-play rate. The validated OP3 configuration already has
`CONFIG_INPUT_QCOM_SPMI_HAPTICS=y`, so no Kconfig change is included. Static
patch checks pass against the formal baseline and the active kernel checkout;
the owner completed the kernel build and device test. Historical downstream data suggests
an ERM motor around 2700 mV, but the current driver does not implement the
old `qcom,vmax-mv` property. The owner test exposed `spmi_haptics` as
`/dev/input/event0` with `EV_FF`, recovery logged
`vibration: input FF -> /dev/input/event0 name=spmi_haptics effect=0`, and
the owner reports that the physical motor works. Handoff:
`docs/handoff/op3-recovery-haptics-kernel-001.md`. This is independent of the
volume/tri-state physical-key patch. The device result is PASS; final
Integration acceptance remains pending.

## OP3 recovery volume and tri-state physical-key device PASS (Integration pending, 2026-09-06)

Commit `6f58544` prepares a formal pmOS MSM8996 Linux 6.12.1 DTS patch for
the missing OnePlus 3 side keys. It registers the standard `gpio-keys`
device: PMIC GPIO3/2 as active-low `KEY_VOLUMEUP(115)`/`KEY_VOLUMEDOWN(114)`
and PMIC GPIO6/4/5 as active-low tri-state top/middle/bottom codes
600/601/602. The three switch inputs receive PMIC pull-ups. The validated
kernel configuration already has `CONFIG_KEYBOARD_GPIO=y`, so no config
change is needed. Baseline and active-checkout `git apply --check` plus
`git diff --check` pass. The owner then built and booted the combined kernel.
Device evidence shows `gpio-keys` on `/dev/input/event4`; recovery opened
`tri=8` and `volume=9`, and the owner reports volume up/down plus all three
tri-state positions work physically. Handoff:
`docs/handoff/op3-recovery-physical-keys-001.md`. The device result is PASS;
final Integration acceptance remains pending.

## OP3 recovery S1302 chin capacitive-key preparation (pending owner test, 2026-09-06)

Commit `d1c525f` prepares a formal pmOS MSM8996 Linux 6.12.1 patch series for
the separate OnePlus 3 S1302 controller: a standard EV_KEY driver, binding,
OP3 DTS registration, GPIO 132 level-low IRQ, GPIO 76 active-low reset, and
the L13/S4 supplies. The driver reports the two physical chin keys as
`KEY_APPSELECT` (580, left/recent) and `KEY_BACK` (158, right/back); the
center fingerprint/home button remains a separate device. Recovery already
discovers codes 580/158 by capability, so no recovery userspace change is
needed for this exposure. `git apply --check` against baseline commit
`67b0bbc3cbf46bae712a2606a43361756fcbd829` and `git diff --check` pass. The
configuration was merged in an external output directory using the previously
validated OP3 base config, not a source-tree config generated from the host
`/boot/config`. The owner completed the kernel build: config
`c3da142e…`, Image.gz `389ed53b…`, and DTB `bfb81b23…`; the packed test image
is `artifacts/boot-oneplus3-pmos612-capkey.img` with SHA256
`9b7f25f549f69e2398516e46504c143eba7c3e2a3d62a2c9e9f0c16aa886e044`.
Owner booted the image and confirmed kernel registration on `75b6000.i2c`:
`op3-capkey-s1302` is `/dev/input/event1`, dmesg reports IRQ 86 ready, and
recovery discovers codes 580/158. Physical testing produced complete press and
release events for both codes 580 and 158. The device scope is supported;
Integration acceptance remains pending. Handoff:
`docs/handoff/op3-recovery-capkeys-001.md`.

## OP3 recovery Wi-Fi cold association wait extension (Issue #10 follow-up, 2026-09-06)

The owner ran `/newroot/opt/op3-wifi/wifi auto` with
`OP3_WIFI_ASSOC_TIMEOUT=120`; it returned `auto-rc=0`, associated to SSID
1106, and obtained DHCP address `192.168.1.5`. Kernel evidence shows the
successful association only after repeated authentication attempts, at about
171 seconds after boot. Commit `1a166c0` therefore raises the default wait to
180 seconds. The `/newroot` paths, stale-lease cleanup, DHCP retry behavior,
IPv6 policy, kernel, DTS, DRM, audio, input, and browser contents are
unchanged. Replacement bundle:
`artifacts/op3-wifi-bundle-ipv6-assoc180-clean-retry.tar.gz`, SHA256
`3b68515b71f2226d5e82cc55a6262b64b0dfa4fd4f323456aea29ccde6247938`.
Owner cold-boot evidence now shows association to SSID 1106, IPv4
`192.168.1.5/24`, default route via `192.168.1.1`, and `ipv6=off`; the
connected BSSID was `12:78:86:70:53:79` at approximately `-92 dBm`. This is a
device PASS candidate for the automatic Wi-Fi/DHCP scope, but remains pending
Integration acceptance. Handoff:
`docs/handoff/op3-recovery-wifi-timeout-001.md`.

## OP3 recovery Wi-Fi stale lease cleanup (Issue #11 follow-up, 2026-09-06)

Owner evidence showed `wpa_state=SCANNING` and `wlan0=NO-CARRIER` while an old
IPv4 address and default route remained. Commit `835a926` stops the previous
background `udhcpc` and flushes `wlan0` IPv4 addresses/routes before a new
profile starts. This prevents a stale lease from being mistaken for a live
connection or from making the DHCP observation return early. Replacement
bundle:
`artifacts/op3-wifi-bundle-ipv6-assoc90-clean-retry.tar.gz`, SHA256
`2d1bbe71a56363e2b7599936971d0d57a6e2d3d0fc9e1b203c5b512cb238b5a7`.
Owner retest still showed `wpa_state=SCANNING`, `wlan0=NO-CARRIER`, no
`wlan0` IPv4 address, and only the USB route. The stale-address cleanup is
therefore behaving as intended. That earlier run is superseded by the later
180-second cold-boot connection evidence; this is not an Integration
acceptance. Handoff:
`docs/handoff/op3-recovery-wifi-dhcp-001.md`.

## OP3 recovery Wi-Fi association timeout guard correction (Issue #10 follow-up, 2026-09-06)

The prior 90-second association change updated the wait loop but left the
post-loop timeout guard hard-coded at 30 seconds. Commit `b617476` changes
that guard to use the same `ASSOC_TIMEOUT` value. This is a one-line
correction in the existing recovery Wi-Fi userspace layer; `/newroot` paths,
module loading, DHCP, IPv6 policy, kernel, DTS, DRM, audio, input, and browser
contents are unchanged. Static shell checks pass. Replacement bundle:
`artifacts/op3-wifi-bundle-ipv6-assoc90-fixed.tar.gz`, SHA256
`143714f00fb17fe5c63f3cb00821ea77e3e0c8616504d8e497b89e9d65f200f7`.
Owner reports that recovery now connects to Wi-Fi automatically after this
bundle was deployed, without a manual `wifi connect` step. This confirms the
automatic path reached the persistent `/newroot` CLI and the 90-second guard
is effective. Full evidence for IPv4 address, default route, and IPv6-off
state is still pending; this is not an Integration acceptance. Handoff:
`docs/handoff/op3-recovery-wifi-timeout-001.md`.

## OP3 recovery Wi-Fi DHCP retry checkpoint (Issue #11, 2026-09-06)

Historical 6.3.1 recovery used background `udhcpc -b -q`, while the current
6.12.1 CLI used fail-fast `udhcpc -n -q`. Owner logs show WPA association can
complete while IPv4 is still absent. The current CLI now keeps DHCP running
in background retry mode and observes the interface for a lease before
returning. Only the DHCP client lifecycle changed; modules, firmware,
regulatory data, WPA profile, association timeout, IPv6 policy, kernel, DTS,
DRM, audio, input, and browser are unchanged. Owner bundle deployment and
device retest are pending. Handoff:
`docs/handoff/op3-recovery-wifi-dhcp-001.md`.

## OP3 recovery Wi-Fi cold-start association checkpoint (Issue #10, 2026-09-06)

Owner logs showed the QCA6174 driver initialized around 9 seconds after boot,
while the first successful authentication/association arrived around 46
seconds. The Wi-Fi CLI's 30-second wait therefore exited before association,
leaving the background wpa_supplicant to connect later without running DHCP.
The wait is now 90 seconds; no driver, firmware, regulatory, IPv4/DHCP, IPv6,
kernel, DTS, DRM, audio, input, or browser logic changed. Owner retest is
pending. Handoff: `docs/handoff/op3-recovery-wifi-timeout-001.md`.

## OP3 recovery IPv6 opt-in checkpoint (Issue #9, 2026-09-06)

Recovery Wi-Fi now applies an IPv6-off policy before the existing automatic
IPv4 connection. The initramfs hook runs `wifi ipv6 off` before
`/newroot/opt/op3-wifi/wifi auto`; after boot, `wifi ipv6 on`, `wifi ipv6 off`,
and `wifi ipv6 status` provide explicit control. Current and future interface
sysctls are covered. The kernel, DTS, ath10k modules/firmware, IPv4 DHCP,
DRM, audio, input, browser, and credentials are unchanged.

Agent shell/static checks pass. The first owner boot reached the recovery
launcher but had no `wlan0` or ath10k dmesg records because sda15 still held
the pre-IPv6 CLI; the new hook exited before `wifi-start`. A replacement
no-credential bundle is available at
`artifacts/op3-wifi-bundle-ipv6.tar.gz` with SHA256
`eeabbb20f0f8331fb220252c77acf52f1b0fabe2919dc5197c45690421994654`.
Handoff: `docs/handoff/op3-recovery-ipv6-001.md`. This is a separate
network-policy checkpoint on the same recovery implementation branch. After
the replacement bundle was deployed, manual `wifi connect` passed with
`wpa_state=COMPLETED`, IPv4 `192.168.1.5/24`, default route via
`192.168.1.1`, and `ipv6=off all=1 default=1 wlan0=1`; automatic post-reboot
validation remains pending.

## OP3 recovery Wi-Fi integration checkpoint (Issue #8, 2026-09-06)

The recovery initramfs recipe now appends the already validated
`/usr/bin/wifi_auto.sh` hook from OP3-WIFI-001. Once the established initramfs
has mounted `/newroot`, the hook invokes the persistent
`/newroot/opt/op3-wifi/wifi auto` command before the recovery launcher. The
Wi-Fi CLI, matching ath10k modules/firmware, credentials, kernel, DTS, DRM,
audio, input, and browser contents are unchanged.

Agent packaging verification passed. The generated recovery initramfs is
`artifacts/initrd-op3-recovery-browser.cpio.gz` with SHA256
`8516cdf2e53e8926191cdd25f43f911abb433356aac005454256efa7c5e54bf1`.
Owner boot-image repack and device validation are pending. Handoff:
`docs/handoff/op3-recovery-wifi-001.md`. Browser testing remains paused until
the recovery foundation issues are complete.

## OP3 recovery direct DRM checkpoint (Issue #7, 2026-09-06)

The unified recovery branch `agent/implementation/recovery-browser-001` now
contains commits `a58f166`, `398ec3a`, `071cc75`, and `eeba948`; they replace recovery's `/dev/fb0` + `FBIOPAN`
display path with a small raw `/dev/dri/card0` KMS backend. It selects the
connected DSI connector/CRTC/preferred mode, creates an XRGB8888 dumb buffer,
uses `DRM_IOCTL_MODE_DIRTYFB` for full-frame submissions, and destroys the
KMS objects before a later compositor handoff. Recovery can reopen card0 and
redraw its existing PTY/libtsm UI after the other DRM client exits.

Agent static aarch64 compilation and bundle staging pass. Current recovery
binary SHA256 is
`6ab82320aa87fd6255a203d94077a3e9afd1e7e147d957822b97127545414ac0`; the
current persistent bundle SHA256 is
`484d7db7d4a977bb0b451cb148b50532c83a7eddedd8938817e340c7cb83cb3a`.
Owner DRM-only boot evidence is now a successful smoke test: recovery opened
`/dev/dri/card0` (connector 33, CRTC 106, 1080x1920, pitch 4352), and its
process held no `/dev/fb0` descriptor; A530 PM4/PFP/GPMU firmware also
loaded. The controlled session logged DRM release, handoff-ready, card0
reopen, and display restore; both session markers cleared and recovery PID
388 remained alive. The captured `pp done time out, lm=2` occurred at dmesg
2.097s, before the recovery launcher marker at 9.809s, so it is classified as
a pre-recovery boot warning. The DRM-only result is not yet an Integration
acceptance because the first smoke test showed a black restored panel even
though the backlight restored to 255. Commit `398ec3a` preserves that
brightness, and `071cc75` defers `SETCRTC` until the first recovery frame is
rendered. The latest commit `eeba948` leaves the active panel scanout in place
while closing recovery's DRM fd; the explicit disable was powering down the
DSI panel unnecessarily. The owner retest now shows the recovery GUI after
the fake session exits, so the DRM-only behavior is device-validated. Issue
#7 handoff:
`docs/handoff/op3-recovery-drm-001.md`. Browser testing remains paused until
this DRM-only gate has evidence.

## OP3 recovery/browser integration checkpoint (Issue #6, 2026-09-06)

Branch `agent/implementation/recovery-browser-001` contains the recovery
startup and browser-session lifecycle implementation in commits `e902c33`,
`d18ffef`, `194ae3f`, `0e7ced3`, `9f3c465`, `a458290`, `d4f9923`,
`3234d5d`, and `5a73922`. The ported
`recovery_mainline`
keeps its PTY/libtsm terminal state alive while `/run/op3-browser.active` is
present, but now closes its fb0 mapping, vsync fd, and inherited-on-exec path
before Weston owns DRM. The browser supervisor waits for
`/run/op3-browser.recovery-ready`; after browser cleanup, recovery reopens fb0
and redraws the same prompt on the same boot.

The agent's static compile, shell checks, rootfs packaging, and initramfs
overlay checks pass. The first device run lacked early A530 firmware; the
corrected initrd loaded it, but later Cog launches still hard-reset the phone.
The latest device log confirms the GPU workaround is active (`control=on`,
`runtime_status=active`) and all A530 firmware is loaded, so the remaining
recovery-specific hypothesis is the fb0/DRM handoff. Commit `5a73922` adds an
explicit close-on-exec plus release/ready/reopen handshake. It is not device
validated and remains **INCONCLUSIVE**, not an accepted milestone. Full handoff:
`docs/handoff/op3-recovery-browser-001.md`.

```text
PROJECT MODE: PRODUCT BASELINE
TARGET KERNEL: pmOS MSM8996 Linux 6.12.1 LTS
TARGET COMMIT: 67b0bbc3cbf46bae712a2606a43361756fcbd829
SHELVED RESEARCH: Linux 7.x (physical UART required to resume)
LEGACY KERNEL: Linux 6.3.1 pmOS-derived
DO NOT BUILD LEGACY OR SHELVED 7.x UNLESS EXPLICITLY REQUESTED
HOST TARGET: Ubuntu 26.04 LTS x86_64
DEVICE: OnePlus 3 / MSM8996
```

## Historical 7.x investigation (2026-08-29) — shelved

**All 6.x pmOS kernels boot with the v74 DTB (compiled from 6.3.1-v74full);
7.2 does not boot even with the v74 DTB.**

Test matrix (2026-08-29):
- OP3-BOOT-035: pmOS **v6.16.12 stable** + strict v74 + own DTB → FAIL
- OP3-BOOT-036: pmOS v6.16.12 stable + strict v74 + **v74 DTB** → **PASS**
- OP3-BOOT-037: pmOS **v6.19.5** + strict v74 (UFS=y) + **v74 DTB** → **PASS**
- OP3-BOOT-038: Linux **7.2** + strict v74 + **v74 DTB** → FAIL
- OP3-BOOT-039: Linux 7.2 + strict v74 + v74 DTB + no **LSUI** → FAIL

**Root cause narrowed:**
- The device boots **any 6.x kernel when given the 6.3.1-compiled v74 DTB**.
  6.12/6.16/6.19 all fail with their *own* DTB but boot with v74 DTB.
  This explains all earlier "6.12 boots, 6.16/7.2 fail" confusion: 6.12's
  "boot" image had been packed with the v74 DTB.
- 6.19.5's earlier fastboot failure was additionally caused by
  `CONFIG_SCSI_UFS_QCOM=m`; v74 full config forces `=y`.
- **7.2 fails even with v74 DTB** → 7.2 has an independent kernel-code/config
  issue. Disabling the only 7.2-unique ARMv9 feature `CONFIG_ARM64_LSUI`
  (unsupported on A53) did NOT fix it.

## Next action (7.2, unresolved) — SUPERSEDED 2026-08-30: 7.x shelved, baseline is 6.12.1 (see below)

1. Get early-boot log from 7.2 (USB ACM gadget or earlycon) to locate where it
   hangs — 7.2 is the only kernel failing with the v74 DTB.
2. Or diff msm8996-mainline 6.19.5 (boots) vs torvalds mainline 7.2
   (fails) early-boot code (head.S / clk-msm8996 / smem / pinctrl).
3. Parallel line: diff v74 DTB vs 6.16/6.19 own DTB to find the node that
   breaks their own DTB boot (candidate: new `qcom,rpm-proc` structure).

## DRM RGB gate PASS (2026-08-30, layer 04 parallel diagnostic)

The DRM dumb-buffer RGB gate passes on the device with the pmOS 6.12 control
kernel plus the v74 DTB:

```text
image:  artifacts/boot-oneplus3-pmos612-v74dtb-drm-test-60s.img
        f9693a7baf9e1fae5ff8ce27517ac6c246782576b8eb739093a84c690b7a3670
result: connector=33 crtc=106 mode=1080x1920@60, solid colour active, exit 0
owner:  red → green → blue → red, displayed in that order and correct
scope:  layer 04 DRM RGB only. Not EGL, GBM, Wayland, Weston, Cog, WPE, or GPU
        runtime PM, and not a Linux 7.2 acceptance result.
```

Two test-program defects were found and fixed on the way: `d43821a` (second-pass
array pointers in the enumeration ioctls) and `e261c4d` (the
`connector_status_connected` constant). Details are in
`docs/handoff/pmos612-drm-dumb-buffer.md`, rows `OP3-DRM-001` …
`OP3-DRM-005` in `docs/test-matrix.md`. Promoting the result to an accepted
milestone is the Integration role’s decision.

This does not change the Linux 7.2 line below.

## EGL gate PASS + ACM debug console (2026-08-30, layer 05)

**OP3-EGL-001 passes all three criteria** on the pmOS 6.12 control kernel plus
the v74 DTB:

```text
image:   artifacts/boot-oneplus3-pmos612-v74dtb-egl.img (284936e6…)
         bundle sda15:/opt/op3-egl (e7b1df23…), Mesa 26.0.1
result:  EGL 1.5 on /dev/dri/card0, GL renderer FD530 (freedreno hardware,
         not llvmpipe), OpenGL ES 3.1; kmscube 30 s windows, 1680 frames at
         59.8 fps, exit 0; owner confirmed the rotating cube repeatedly,
         including two image-only boots and three fastboot-boot runs
scope:   layer 05 EGL only. Not Wayland, Weston, Cog, WPE, and not a Linux 7.2
         acceptance result. Promotion is the Integration role's decision.
```

Fixed en route (details in `docs/handoff/pmos612-egl-gate.md`, row
`OP3-EGL-001` in `docs/test-matrix.md`):

- `run.sh` must not export `LD_LIBRARY_PATH` (broke busybox, SIGBUS in
  kmscube), and must feed kmscube stdin from a `sleep` pipe (kmscube treats
  any readable stdin as `user interrupted!` and exited after one frame).
- GPU runtime PM resume hard-resets the device (DTB has dummy GPU regulators);
  the launcher disables A530 runtime PM at boot as a workaround
  (`docs/known-issues.md`, DTB fix is a separate task).

**ACM debug console is now operational end to end**
(`boot-oneplus3-pmos612-v74dtb-egl-acm.img`, `65cac825…`):
the launcher fixes the stale `/dev/ttyGS0` placeholder node, relays
`/dev/kmsg` to it and spawns a debug shell; the host needs the udev rule
`scripts/99-op3-acm.rules` (group `dialout`) and `kai` in `dialout`. Verified:
live kernel log reaches `cat /dev/ttyACM0`, and commands typed on the ACM port
execute on the device (fallback channel when RNDIS/SSH is dead). Usage notes
and pitfalls in `docs/known-issues.md`. Root fix for a real `console=ttyGS0`
(panics, init output) needs a pmOS 6.12 kernel rebuild with
`CONFIG_U_SERIAL_CONSOLE=y` — recorded as a separate task.

This does not change the Linux 7.2 line below.

## Wayland gate PASS (2026-08-30, layer 06)

weston 14 composites on the panel with the GL renderer and an animated Wayland
client (`docs/handoff/pmos612-wayland-weston.md`, row `OP3-WAYLAND-001`):

```text
image:  artifacts/boot-oneplus3-pmos612-v74dtb-weston.img (ea6043cc…)
bundle: artifacts/op3-weston-bundle.tar.gz (0a8163cb…, whole Buildroot target)
result: weston DRM backend + GL renderer (FD530) on DSI-1 1080x1920@60;
        weston-simple-egl spinning triangle visible on the panel; owner
        confirmed; two runs + live session, zero assertions
scope:  layer 06 only. Not the browser, and not a Linux 7.2 acceptance result.
```

Six issues were fixed on the way (loader/module paths, eudev input_id rule,
/usr/libexec helper wrappers, stale weston twin holding DRM master, Mesa
built without the wayland platform) — details in the gate doc.

Next: layer 07 browser gate (Cog + WPE WebKit) — large Buildroot build.

## Browser gate PASS (2026-08-30, layer 07)

cog (WPE WebKit) renders a local HTML page fullscreen on the panel through
WPEBackend-fdo → weston → freedreno (`docs/handoff/pmos612-browser-cog.md`,
row `OP3-BROWSER-001`):

```text
image:  artifacts/boot-oneplus3-pmos612-v74dtb-browser.img (802d803c…)
bundle: artifacts/op3-browser-bundle.tar.gz (bec5ce8b…)
result: cog `wl` platform, fullscreen 1080x1920; page "Loaded successfully";
        CSS colour animation + JavaScript seconds counter advancing
        (owner-confirmed, two runs + deploy-session run)
scope:  layer 07 local-page rendering. Network browsing and multimedia are
        out of scope (gstreamer OFF in the build).
```

The graphics/user-space chain on the pmOS 6.12 control is validated end to
end: **DRM dumb buffer → GPU firmware → EGL → Wayland compositor → browser**.
Ten build/runtime fixes were required en route (all documented in the gate
doc): build parallelism, harfbuzz-icu, Mesa `-Dplatforms=wayland`, Mesa
`legacy-wayland=bind-wayland-display` (diagnosed with a cross-compiled
eglGetProcAddress probe), cog platform name `wl`, `--fullscreen` removal,
ln -sfn bridge gotcha, DejaVu fonts, wpe-webkit-2.0 helper wrappers,
shared-mime-info, and a cog fullscreen-size patch.

Next: the control-baseline bring-up is COMPLETE through layer 07. Remaining
project lines: Linux 7.2 early-boot (last), GPU regulator nodes in DTB,
`CONFIG_U_SERIAL_CONSOLE=y` kernel rebuild.

## Cross test: browser on the 6.3.1 (v100) kernel — PASS (2026-08-30)

The same browser bundle ran unchanged on `6.3.1-msm8996+ #31`
(`artifacts/boot-oneplus3-v100-browser.img`, v100 Image+DTB + browser
initramfs): weston GL renderer FD530, cog `wl` platform, page loaded,
desktop + rendered page owner-confirmed. Caveat: ~452 GPU SMMU context
faults (b40000.iommu, iova=0x0) on 6.3.1's a5xx path — non-blocking,
worth investigating in the parallel project.

**Conclusion: the 6.3.1 parallel project's browser failures were
user-space, not kernel.** The browser bundle + run.sh (fonts, MIME database,
loader discipline, helper wrapping, `wl` platform) transfers directly.
Details in `docs/handoff/pmos612-browser-cog.md` cross-test section.

## PROJECT BASELINE DECISION (2026-08-30): pmOS 6.12.1 LTS — 7.x line SHELVED

**The long-term project kernel is the pmOS 6.12.1 LTS line** (LTS EOL
2028-12-31; already validated end-to-end through the layer-07 browser gate on
this device). Non-LTS 6.19/7.0/7.1/7.2 carry no long-term maintenance and are
out of scope for the product path.

- **7.x line shelved** (owner decision): OP3's LK produces no early-boot
  output without a physical UART, so every 7.x boot is blind (fastboot
  return / no ACM / no pstore). Last data points: OP3-BOOT-040/041 — 7.0-rc1
  fails with BOTH the upstream OP3 DTB and the v74 DTB under the gemini-proven
  recipe; regression window is the 6.19.5 → 7.0-rc1 merge window. Full
  analysis and resume criteria in `docs/handoff/linux70rc1-minimal-ab.md`.
- **source/ reduced** to `linux-mainline-6.12.1`, `linux-pmos-msm8996-6.12`,
  `linux-pmos-msm8996-6.3.1` (+ `buildroot`). Unpushed local work from the
  removed trees (S6E3FA5 driver ports for 6.16/6.19.5, the 7.2 A/B patches)
  is archived in `patches/shelved-7x/` — the reusable asset if a newer LTS
  (e.g. 6.18) is ever picked up again.
- **Baseline retest PASS (OP3-BROWSER-003, 2026-08-30)**: the layer-07 browser
  gate re-runs clean on the pmOS 6.12.1 baseline after the source/out cleanup —
  weston desktop + cog fullscreen animated page owner-confirmed, and for the
  first time with a full ACM kernel-log capture from boot (554 lines,
  `artifacts/console-browser-retest-20260830.log`).
- Still-open project lines (unchanged): GPU regulator nodes in DTB,
  `CONFIG_U_SERIAL_CONSOLE=y` kernel rebuild for a real `console=ttyGS0`.

## Browser network gate PASS (OP3-BROWSER-005, 2026-08-31) — CJK font caveat

Branch `agent/implementation/op3-browser-net-001` (based on the OP3-WIFI-001
tip) combines the two PASS chains: the sda15 `run.sh` brings up Wi-Fi via
`/newroot/opt/op3-wifi/wifi auto`, steps the clock from the baidu HTTP Date
header (initramfs busybox has no ntpd), powers the GPU on after association,
and cog loads `https://www.baidu.com` with the TLS-enabled bundle
(glib-networking → OpenSSL GIO backend + ca-certificates; run.sh bridges
`/usr/lib/gio` and `/etc/ssl`). Device result: **PASS** — owner-confirmed
page rendering, `Loaded successfully` with no TLS error
(`d27dac50…` boot image). Two recorded caveats: (1) Chinese glyphs render as
tofu — bundle has DejaVu only; follow-up OP3-BROWSER-006 (stage a CJK font,
no rebuild needed). (2) Two early runs hard-reset at association time before
the GPU power-on sequencing fix; not reproduced since, attribution unproven
(placeholder DTB GPU regulators remain the suspected root cause). Details:
`docs/handoff/op3-browser-net-001.md`, row `OP3-BROWSER-005` in
`docs/test-matrix.md`.

Status update (2026-08-31 evening): OP3-BROWSER-006 PASS (owner confirmed
Chinese renders, WQY Microhei bundled + `TZ=CST-8`). A third hard reset
during browser display was traced to a **low battery** — all device tests
must now record battery/charging state. Architecture settled for the phone:
power-first on-demand sessions over the fbcon + agent-CLI home (see the
"Display model, power-first" section of the handoff); `kiosk-shell` is the
run.sh default and the dbus session bus is started for `cogctl`/WebDriver;
`BR2_PACKAGE_WPEWEBKIT_WEBDRIVER=y` is in the defconfig for the owner's
Playwright-style automation (incremental rebuild in progress).

Handover (2026-09-02): WebDriver transport verified end-to-end from the PC
(session/navigate/element/JS-inject/data/screenshot — see
`tests/browser/`); renderer intermittently crashes or hangs on heavy pages
→ the "mixed objects" theory was REJECTED (dirclean rebuild is
byte-identical to the reconfigure build) and POWER is the primary suspect;
the landscape output default (rotate-90, 1920x1080) is live. Full handover
steps + charging-check protocol: "Handover state (2026-09-02)" section of
`docs/handoff/op3-browser-net-001.md`. Private collection assets
(credentials) live in gitignored `local/jnu/` — never commit/push.

## Key artifacts

- **OP3-INITRD-FW-001 PASS (OP3-BOOT-044, 2026-08-31)**: Issue #3's six
  MSM8996 firmware files were reconstructed from SHA512-pinned inputs and
  verified against the historical SHA256 output manifest. With Image.gz, own
  DTB, boot profile, cmdline, non-firmware initramfs entries and sda15 fixed,
  the device reached recovery/RNDIS/SSH; early SLPI and ADSP loads succeeded.
  Artifact `boot-oneplus3-pmos612-own-dtb-firmware-provenance.img`
  (`38e59038…`); handoff `docs/handoff/op3-initrd-fw-001.md`.
- **Own-DTB browser closure (OP3-BROWSER-004, 2026-08-31)**: with the same
  OP3-BOOT-044 Image.gz and own DTB, the existing GPU-firmware/browser overlay
  and rootfs bundle ran Weston → Cog/WPE successfully. Weston used DSI-1 at
  1080x1920@60 with Mesa/freedreno FD530; the local page loaded and the owner
  visually confirmed it. This completes the own-DTB boot/initramfs/browser
  test line; no kernel source or configuration change is indicated.
- **Firmware placement follow-ups are deferred**: modem/MBA (MSS), Venus and
  ath10k are module-driver projects, not part of the boot-image gate. Open one
  branch/task per driver when that functionality is needed; first establish the
  exact modules and dependencies for `6.12.1-msm8996+`, then test firmware
  placement and loading after `/newroot`. Do not reopen OP3-BOOT-044 merely
  because these optional module paths remain untested.
- pmOS 6.12 own-DTB + reproducible-initramfs boot PASS (OP3-BOOT-042/043):
  `artifacts/boot-oneplus3-pmos612-own-dtb-repro-initrd.img`
  (`0b2e85ee…`); source `91df7ccd…`, tree-built DTB `cb29ab65…`, generated
  initramfs `61cf6338…`. The source uses the direct `rpm-glink` topology; it
  replaces the v74 DTB only for the pmOS 6.12 product path. The generated
  initramfs is a reproducible/auditable reserialization of the still-pinned
  historical archive; MSM8996 firmware provenance is now separately validated
  by OP3-BOOT-044. Details: `docs/handoff/op3-dts-rpm-001.md` and
  `docs/handoff/op3-initrd-001.md`.
- Project-owned pmOS source patch archive (GitLab upstream remains read-only):
  `patches/pmos612-op3-own-dtb/` contains the ordered S6E3FA5 and direct
  RPM-GLINK patches needed to reproduce OP3-BOOT-042 on the pinned 6.12.1
  source commit.
- 6.16.12 bootable (v74 DTB): `artifacts/boot-oneplus3-pmos616-v74strict-v74dtb.img`
- 6.19.5 bootable (v74 DTB): `artifacts/boot-oneplus3-pmos6195-v74strict-v74dtb.img`
- 6.16.12 worktree: `source/linux-pmos-msm8996-6.16` (tag `v6.16.12-msm8996`)
- v74 DTB: `out/pmos-msm8996-6.3.1-v74full/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb` (73383 bytes, `463b2c72...`)
- Panel driver source: `source/linux-mainline-6.4/drivers/gpu/drm/panel/panel-samsung-s6e3fa5.c`
## DEVICE full flow PASS: feishu delivered from the phone (2026-09-03)

The complete JNU flow ran on the OnePlus 3 (browser-net image, rotate-270,
same collector): slider (human) -> fill -> login -> mouse-mode override ->
tab -> semester + both categories via filter+first-item-click -> building
name -> query (report iframe took ~40 s on-device; execute/sync BLOCKS
while the report renders - tolerate long timeouts) -> table extraction
(1.05 MB) -> parse (29 classrooms, 本科703/研究生189/借用19) -> JSON
-> feishu DELIVERED.

Device-specific findings vs PC rig:
- After slider verification the login FORM fields (#un/#pd) render LATE
  (~10 s on device vs instant on PC): the collector must poll for
  #un existence before filling (patched; also login deadline 300->480 s).
  The first fill+click hit a not-yet-rendered form and the yidun token
  expired before the manual retry - the owner re-clicked and login
  succeeded; collector deadline expired seconds before success, so the
  remaining steps were driven ad-hoc against the live session. The
  patched collector automates exactly that sequence for future runs.
- Same engine-version parity as the PC rig held: no flow changes needed
  for the device other than timing tolerance.

## Chrome line: HEADLESS FULL FLOW PASS on PC (2026-09-03 night)

chrome_test.py collect mode: headless Chromium + session.json storage_state
-> 免登录排课管理 -> tab -> 学期/教室分类 via the ORIGINAL paste_text
interaction (Control+a -> press_sequentially 20ms -> **Backspace** re-filter
-> click the item's SPAN) -> 教室名称 -> visible-查询 click -> report
iframe table (1.05MB) -> parse (29 classrooms, 703/189/19) -> JSON ->
feishu DELIVERED. exit=0, zero human input (login session reused).

Key interaction details learned from the owner's original auto_schedule.py
(paste_text): per-key typing at 120ms delay breaks the jqx filter ("选项
消失") - the original flow pastes/inputs the FULL filter text in one
event, then presses **Backspace** to re-trigger the filter, then clicks
the first item's SPAN (not the item container). The 查询 button locator
must pick the VISIBLE a:has-text("查询") (an invisible 校级-教室查询 menu
item matches first).

## Chrome/Playwright migration line - day 1 results + WAF wall (2026-09-03 evening)

Decision: the automation product line moves to Chromium/Playwright (the
owner's original, proven stack); the WPE line stays as the lightweight
browser bring-up achievement. Rationale: every WPE automation blocker
(jqx touch-mode mismatch, cookie-jar losing login tickets, WebDriver
injection dropping cross-domain cookies, no native headless) does not
exist in Chromium, and the owner's original Playwright code ports as-is.

PC validation day 1 (chrome_test.py, in cogwebauto private repo):
- Env: playwright already present (owner's original automation),
  chromium-1234 browsers installed; user-level install on python3.14.
- VISIBLE login PASS: persistent context + auto-filled credentials +
  human slider + login click. CRITICAL detection lesson: after login the
  SPA renders 排课管理 AT the CAS URL (URL stays icas.jnu.edu.cn/cas/...)
  - success detection MUST use the page TITLE, never the URL.
- Login cookies are SESSION cookies (no expiry) -> Chromium by design
  never writes them to the profile disk -> restarting a persistent
  context cannot restore the login. This is browser security design,
  not a bug.
- Correct restore mechanism = Playwright storage_state export/import
  (session.json, includes session cookies): implemented in chrome_test.py
  (login exports after success; verify imports + headless). UNTESTED:
  blocked by the auth4 WAF (see below).
- Headless wall: auth4.jnu.edu.cn 云防护 WAF returns 网关错误 for
  HeadlessChrome UA; with a normal-Chrome UA override the icas step
  passed but the auth4 login navigation got blocked again (fingerprint
  inconsistency and/or rate limiting after ~dozens of automation hits
  today). The WAF likely flagged this IP/fingerprint for the day.
- gid_ URL finding: the jw app URL embeds an auth ticket (gid_=MkpJ...)
  but the owner confirmed it only works combined with the session
  cookies; an old recorded URL (t_s=2022) was rejected.

Tomorrow: resume storage_state import validation (fresh login ->
export -> immediate headless restore), then headless full flow, then the
device rootfs decision (pmOS vs Ubuntu debootstrap vs deb-extraction
into the buildroot rootfs). Avoid UA inconsistency between visible and
headless runs; consider warming up slowly to avoid the WAF.

## PC rig: FULL JNU flow PASS + feishu delivered (2026-09-03)

End-to-end on the PC comparison rig (same WPE/cog pins as device):
login (slider+fill+click) -> jqx mouse-mode override -> tab -> semester via
filter+first-item-click -> both category selections (same interaction,
values 6+7 verified in hidden input) -> building name fill -> query ->
iframe table extraction -> parse (29 classrooms, 本科703/研究生189/借用19)
-> JSON saved -> feishu text+file DELIVERED.

Fixed en route (all pushed to cogwebauto): collector helper ordering
(UnboundLocalError), iframe selector must iterate ALL iframes (first one is
an empty placeholder; the report lives in the frReport2/show.do iframe).
jqx touch-mode root cause and the mouse-mode override are documented above;
the override is applied by the collector right after login.

## PC rig: JNU login PASS; collection dropdown findings (2026-09-03)

PC comparison rig (buildroot x86_64, same WPE 2.50.5/cog 0.18.5 pins) is
OPERATIONAL after fixing a chain of rig-only issues, and the JNU login
flow PASSED end-to-end on it:

- slider (human) -> yidun-gone detection -> human pause -> key-event fill ->
  pause -> programmatic login click -> CAS redirect -> 排课管理 marker:
  **PASS on PC**. The redirect-load-error seen on the device did NOT
  reproduce on the PC -> that failure is device/network-specific, not a
  flow bug. Device attempt can proceed once the collection step is fixed.
- Rig fixes landed in `local/jnu/op3-pc-rig.sh` (pushed to cogwebauto):
  `COG_MODULEDIR` (cog looks for platform modules at compiled-in
  /usr/lib/cog/modules), `GIO_MODULE_DIR` (glib TLS module; without it
  every https page shows the device-era "TLS not supported" error),
  XDG_RUNTIME_DIR must be the DESKTOP SESSION's /run/user/<uid> (cog's
  check_supported() does wl_display_connect(NULL); a rig-private
  XDG_RUNTIME_DIR makes that fail -> wl platform reports "not supported"),
  libWPEBackend-default.so symlink to the fdo backend.
- Rebuild chain this session (all committed): MESA3D_LLVM explicit
  (kconfig silently dropped llvmpipe->EGL/GLES->wpewebkit otherwise),
  CAIRO for cog's wayland platform, libdrm re-dirclean (same incremental
  trap), LLVM AMDGPU backend needed for radeonsi, host-llvm dirclean
  (llvm-dirclean does NOT rebuild the host variant; mesa consults the
  STALE host llvm-config via sysroot). Verified: probe shows
  `bind-wayland-display: YES` (WPEBackend-fdo requirement).
- Collection step findings (page DOM, live session): the 学期 select is
  jqxDropDownList (already correct value, can be skipped); 教室分类
  (JSFLDM) is emap multi-select2 wrapping jqxDropDownList. Position-based
  xpaths from the old Playwright flow do NOT match this DOM. JS-synthetic
  el.click() and jqxDropDownList('selectItem') do NOT register item
  selection; emap harvests the selection on the jqx 'close' event
  (handlers on wrapper: close x2, open, keydown/focus/blur). Next step:
  open via jqxDropDownList('open') (works), select items by their native
  mousedown path (WebDriver native click on .jqx-item needs position
  verification), then trigger 'close' and verify the hidden input value,
  then fill 教室名称 (番禺教学大楼2) and click 查询 -> iframe table ->
  parse -> feishu.

## Privacy-scraping content lives in a PRIVATE repo (2026-09-02)

All JNU/credential-bearing web-scraping assets are versioned in the private
repo `https://github.com/iamgreatwk/cogwebauto` (branch `main`): collector
(`jnu_collect.py` incl. yidun-aware humanized login flow), parser
(`parse_table_to_json_v2.py`), credentials (`config.json`), deploy tooling
(`op3-deploy.sh`) and the xlsx templates. In THIS repo they stay under
`local/` which is gitignored - NEVER commit them to any public branch; the
nested git repo at `local/jnu/.git` is their version control. Excluded from
the private repo: `site-packages/`, `wheels/`, `__pycache__/`, `out/`.

## Device test result + PC comparison rig (2026-09-02, second session)

Full-flow device test (charged, fastboot boot browser-net img, rotate-270
confirmed in weston log): yidun slider drag PASSED (owner saw the green
verify-success mark), script filled un/pd and clicked login; during the
post-login redirect the browser showed an error page (owner description:
a "no path / route"-style load error). After that the CAS form was back
empty. All device processes were then stopped (collector + browser stack,
0 residual, GPU control=auto).

Owner directive: STOP debugging on the phone. Build a PC comparison rig
with the SAME engine versions and validate the whole flow there first:

- `buildroot/op3-browser-x86-pc.defconfig` (committed): x86_64 buildroot
  with the SAME pins as the device (WPE WebKit 2.50.5, cog 0.18.5,
  WPEWebDriver), llvmpipe software GL for a self-contained stack. Output:
  `out/buildroot-x86-pc`. Build running (nohup, /tmp/buildroot-x86-pc.log).
- `local/jnu/op3-pc-rig.sh` (private): up/down/collect/log. Prefers the
  desktop's native GNOME Wayland session for cog (no weston install
  needed); falls back to nested distro weston. System python3 already has
  requests+bs4 for the collector.
- PC-rig rule: the JNU flow (slider login incl. the redirect-failure step)
  must PASS on the rig before any device attempt; one device session per
  validated change only, then `op3-deploy.sh stop`.

## Device heat protocol (owner directive, 2026-09-02)

The browser stack (weston + cog + forced-on GPU) was left running during
host-side code/debug cycles and cooked the phone. RULES:

- After EVERY debug/test session on the device, kill the browser stack:
  `tests/browser/op3-automation-stop.sh` (on device; kills weston/cog/
  WPEWebDriver/webkit helpers/dbus/udevd, restores GPU runtime PM to auto,
  fbcon takes the display back). Host shortcut: `local/jnu/op3-deploy.sh stop`.
- Never leave the device-side session up "just in case" between host-side
  code edits; restart it on demand (`op3-deploy.sh restart-session`).
- Overheated / low-battery device: stop all device operations until it has
  cooled down / recharged (reset-class failures must record battery state).

## Screen orientation fix + JNU flow prepared (2026-09-02)

- Owner confirmed the landscape rotation is 180 deg off: default
  `WESTON_TRANSFORM` changed `rotate-90` -> `rotate-270` in BOTH
  `boot/browser-test/opt/op3-browser/run.sh` and
  `tests/browser/op3-automation-session.sh` (not yet deployed - device
  cooling/recharging; deploy with `local/jnu/op3-deploy.sh deploy`).
- JNU CAS login PASS recorded (slider gate was the only blocker; see
  op3-browser-net-001.md). Anti-detection pacing added to
  `local/jnu/jnu_collect.py`: yidun-disappear detection -> human pause ->
  key-event fill -> pause -> programmatic login click -> human-paced
  collection steps (random-jitter pauses via human_pause()); fixed
  undefined `res`/`BUILDING_NAME` in main()/collect_rest().
- New tooling: `tests/browser/op3-automation-stop.sh` (device stop script)
  and `local/jnu/op3-deploy.sh` (host: deploy / restart-session / collect /
  log / stop). Full-flow retest pending device cool-down + charge: deploy ->
  restart-session -> collect -> owner slides slider on the (now correctly
  oriented) touchscreen -> script fills/clicks -> collection -> feishu.
