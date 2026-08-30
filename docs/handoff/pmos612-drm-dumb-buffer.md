# OnePlus 3 pmOS 6.12 direct DRM dumb-buffer gate

```text
Task / GitHub Issue: Owner-authorized no-Issue direct KMS smoke test
Role: Implementation
Formal baseline: Linux v7.2 pristine upstream (unchanged)
Diagnostic kernel: pmOS 6.12 (v6.12-v74strict) full-initrd control, the only
  control with owner-verified USB RNDIS and ACM monitoring
Baseline commit: 85792c20e50d6b27550b9c02b371e6ff37d4f697
Working branch: agent/implementation/s6e3fa5-linux72-port
Changed files: tests/drm/op3-drm-dumb.c; tests/drm/README.md;
  boot/drm-test-initramfs/sbin/run_recovery.sh; boot/drm-test-initramfs/README.md;
  scripts/make-drm-test-initrd.sh
Source commit SHA: 0e1d84b
Fix commit SHA: d43821a (EFAULT root-cause fix, on top of 0e1d84b)

Layer: 04 DRM RGB (parallel diagnostic; not a Linux 7.2 acceptance result)
Previous PASS milestone: pmOS 6.12 full-initrd control boots with USB RNDIS,
  ACM shell and Dropbear (owner-verified). The upstream v6.12.1 control reaches
  recovery.c but exposes neither USB network nor ACM.
Sole hypothesis: The booting pmOS 6.12 control image can allocate, map, and
modeset a DRM dumb buffer through /dev/dri/card0, visibly producing a solid
RGB frame.
Only variable changed: One appended cpio member inside the initramfs of the
fixed pmOS 6.12 control image: a replacement `sbin/run_recovery.sh` plus the
static `op3-drm-dumb` binary. Kernel, DTB, header, cmdline, `/init`,
`init_mainline.sh`, inittab, DRM/MSM, GPU, PM, and sda15 content are unchanged.

Build run by project owner: the single-file static test binary only
(`artifacts/op3-drm-dumb`, built from commit `d43821a`, 2026-08-30). No kernel,
Buildroot, Mesa, WebKit, WPE, or other large-project build.
Build result: ELF 64-bit LSB ARM aarch64, statically linked, SHA256
`a41b02d1976071f16f57afd6c365e9ddce93b7500864cd89141b4a1368fd4be3`.
The 2026-08-30 `0e1d84b` binary
(`d74da26cc2914ea083a2c4d1cbc6095812673226b87c3e89de0ffbdd540e2675`) is
superseded: it contains the EFAULT defect.

Device test run by project owner: 2026-08-30, derived image booted with USB
  RNDIS up; launcher ran the sequence; all colours exited 1 until commit
  `e261c4d`
Device result: PASS criteria 1 and 2 met. Exit 0 with `connector=33 crtc=106
  mode=1080x1920@60`, and the owner reports a uniform red panel that stays on.
  Criterion 3, reproducibility across two distinct colours, is not recorded yet.
Evidence links / log paths: `/newroot/var/log/op3-drm-dumb.log` on sda15;
  see “2026-08-30 second run” below

Conclusion: KMS PASS pending the second-colour check
Uncertainties: This legacy KMS path does not exercise atomic KMS, GBM, EGL,
Wayland, Weston, Cog, or WPE. A successful static binary also depends on the
running image exposing /dev/dri/card0 to the sda15 root filesystem.
Recommended next experiment: Run red, green, and blue cases and record both
the program output and the visible panel result. Only after a PASS, test EGL.
```

## Fixed boot control

Do not rebuild the kernel for this gate. Boot the already validated pmOS 6.12
full-initrd control image. It is the control for this gate because it is the one
image the owner has seen expose USB RNDIS and the ACM shell:

```text
kernel build: out/pmos-msm8996-v6.12-v74strict/arch/arm64/boot/Image.gz
DTB: out/pmos-msm8996-6.3.1-v74full/.../msm8996-oneplus3.dtb
boot image: artifacts/boot-oneplus3-pmos612-v74dtb-full-initrd.img
kernel payload SHA256 (Image.gz + raw DTB):
  50ffea424e6b7625b30acd5b14673ea287d2b04c7c283ee145f895247d8e881a
ramdisk: artifacts/reference-initrd.img
  c3358a1cadb747996ddaa492e636827f2d72974040e8fd40d81f8a213e676366
cmdline: fbcon=nodefault console=tty0 pmos.debug-shell
```

Its configuration has `CONFIG_DRM=y`, `CONFIG_DRM_MSM=y`,
`CONFIG_DRM_MSM_DSI=y`, `CONFIG_DRM_MSM_DSI_14NM_PHY=y`,
`CONFIG_DRM_PANEL_SAMSUNG_S6E3FA5=y` and `CONFIG_BACKLIGHT_CLASS_DEVICE=y`, so
the display path is present.

The earlier plan used the upstream v6.12.1 DSI control
(`artifacts/boot-oneplus3-mainline-6121-dsi-pm-v74dtb-full-initrd.img`). The
owner reported that image reaches `recovery.c` but exposes **no USB RNDIS
network and no ACM shell**, so it cannot produce log evidence. It is superseded
as the control for this gate, and
`artifacts/boot-oneplus3-mainline-6121-dsi-pm-v74dtb-drm-test.img`, which was
built from it, must not be tested.

## Owner command: compile the test binary

This is a tiny static userspace compile, not a kernel, Buildroot, Mesa,
WebKit, WPE, or other large-project build. Run it only from the committed
state containing source commit `0e1d84b`:

```bash
set -e

project=/home/kai/src/oneplus3-mainline
source="$project/tests/drm/op3-drm-dumb.c"
binary="$project/artifacts/op3-drm-dumb"

aarch64-linux-gnu-gcc-11 -static -std=gnu11 -O2 -Wall -Wextra -Werror \
  -o "$binary" "$source"

file "$binary"
sha256sum "$binary"
```

The program has no libdrm dependency. It directly uses the installed kernel
DRM UAPI headers and libc, opens `/dev/dri/card0`, finds a connected connector,
creates an XRGB8888 dumb buffer, fills it, and calls legacy `SETCRTC`.

## Owner device procedure

This is the fallback path if the automatic launcher below is not used. Copy
`op3-drm-dumb` into the sda15 root filesystem (for example
`/newroot/usr/bin/op3-drm-dumb`) using the existing owner-established procedure.
Do not change the kernel image or its companions, and do not infer a new USB
address, credential, mount point, or boot command. Once that procedure has
started the pmOS 6.12 control image, stop the recovery client so it cannot hold
the DRM master, then run:

```bash
chmod 0755 /newroot/usr/bin/op3-drm-dumb

# Recovery holds the DRM master and would make legacy SETCRTC fail with EACCES.
# Replace the respawn target in the running system, then stop the client.
cp -a /sbin/run_recovery.sh /sbin/run_recovery.sh.bak
printf '#!/bin/sh\nexec sleep 3600\n' > /sbin/run_recovery.sh
chmod 0755 /sbin/run_recovery.sh
killall recovery_mainline

/newroot/usr/bin/op3-drm-dumb red --hold
```

Use Ctrl-C in that session to restore the previous CRTC state and exit.
Repeat with `green --hold` and `blue --hold`; or use, for example,
`red --seconds 30` for an automatic return after 30 seconds.

Restore the original launcher on the device afterwards, or simply re-flash the
control image, so later boots return to the validated state.

## 2026-08-30 deployment and first run

With explicit owner authorization, the static executable was copied through
the already-running USB-network SSH endpoint to the persistent root filesystem:

```text
endpoint: 172.16.42.1 (SSH root account; credentials are not recorded here)
sda15 mount observed on device: /dev/sda15 mounted at /newroot
installed path: /newroot/usr/bin/op3-drm-dumb
mode: 0755
device SHA256: d74da26cc2914ea083a2c4d1cbc6095812673226b87c3e89de0ffbdd540e2675
```

The device exposes both `/dev/dri/card0` and `/dev/dri/renderD128`. The first
diagnostic invocation was:

```text
/newroot/usr/bin/op3-drm-dumb red --seconds 60
```

It exited before dumb-buffer allocation or modesetting with:

```text
DRM_IOCTL_MODE_GETRESOURCES: Bad address
```

Therefore no RGB/scanout claim is valid. The failure is in the test program's
KMS-resource enumeration, not evidence of a panel, GPU, or renderer failure.
The source has now been corrected in commit `d43821a`; a new binary must be
built and retested. See “Root cause of the EFAULT” below.

The contemporaneous `dmesg` also contains `msm_mdp ... pp done time out` and a
later missing `qcom/a530_pm4.fw` request. Neither is assigned as the cause of
this test failure: the program failed earlier, before it could create a dumb
buffer or issue `SETCRTC`.

## Root cause of the EFAULT (2026-08-30)

The failing ioctl is the **second** `DRM_IOCTL_MODE_GETRESOURCES`, not the
first; both calls print the same message. The old code allocated only the
connector and CRTC arrays, so the second pass kept the counts returned by the
first pass while leaving `fb_id_ptr` and `encoder_id_ptr` at 0.

`drm_mode_getresources()` (v6.12.1 `drivers/gpu/drm/drm_mode_config.c`) copies an
entry whenever its running count is below the count the caller supplied, and it
does not validate the matching pointer:

```c
	encoder_id = u64_to_user_ptr(card_res->encoder_id_ptr);
	drm_for_each_encoder(encoder, dev) {
		if (count < card_res->count_encoders &&
		    put_user(encoder->base.id, encoder_id + count))
			return -EFAULT;
```

MSM always exposes at least one encoder, so `count_encoders` from the first pass
is non-zero while `encoder_id_ptr` is NULL: `put_user()` writes to NULL and the
ioctl returns `-EFAULT` (“Bad address”). It is deterministic on any device with
an encoder and happens before any dumb buffer or `SETCRTC` is attempted.

The same defect existed one layer down in the second
`DRM_IOCTL_MODE_GETCONNECTOR`: `props_ptr` and `prop_values_ptr` stayed NULL
while `count_props` was non-zero, and `drm_mode_object_get_properties()`
(`drivers/gpu/drm/drm_mode_object.c`) also copies while
`*arg_count_props > count` without checking the pointers. That failure would
have appeared immediately after the first one was fixed.

Commit `d43821a` allocates every array the UAPI defines and always passes each
pointer together with the capacity it belongs to, inside a bounded retry loop so
a list that grows between the two passes is refetched instead of truncated. Only
the test program changed; kernel, DTB, initramfs, cmdline, and sda15 content are
unchanged.

## Expected outcomes for the next run

| Program output | Meaning | Next single variable |
| --- | --- | --- |
| `connector=… crtc=… mode=…` then `solid colour active on /dev/dri/card0`, panel uniformly coloured | PASS candidate | confirm with a second colour, then record PASS |
| `DRM_IOCTL_MODE_SETCRTC: Permission denied` | This process is not the DRM master. `SETCRTC` is a `DRM_MASTER` ioctl (v6.12.1 `drm_ioctl.c`); another client, most likely `recovery_mainline`, holds it | stop the recovery launcher, see next section |
| `DRM has no connectors or CRTCs` | A DRM lease hides them, or the driver registered none | collect `dmesg`; do not change the kernel |
| `no connected connector with a usable CRTC` | No connector reports `connected`; no sink detected on the DSI path | collect `dmesg`; this is display-layer evidence, not a program bug |
| any other ioctl error or hang | unclassified | record the exact output; do not change kernel, DTB, or GPU first |

## 2026-08-30 second run: root cause found, KMS PASS

The derived image booted, USB RNDIS came up and the launcher ran the whole
sequence. Every colour exited 1 with `no connected connector with a usable
CRTC`, while sysfs reported `card0-DSI-1 status=connected enabled=enabled
mode=1080x1920` and backlight `brightness=255 actual=255 max=255 power=0`. The
panel was never the problem.

A read-only KMS probe (only `GETRESOURCES`, `GETCONNECTOR`, `GETENCODER` and
`GETCRTC`; run from `/tmp`, never packaged into any image) showed what the
kernel actually returns:

```text
resources: fbs=0 crtcs=1 connectors=1 encoders=1
crtc ids: 106
encoder ids: 32
connector ids: 33
crtc 106: fb=107 mode_valid=1 x=0 y=0
encoder 32: type=6 crtc=106 possible_crtcs=0x1 clones=0x1
connector 33: type=16 type_id=1 connection=1 encoder_id=32 modes=1 encoders=1 props=5
  mode[0] 1080x1920@60 clock=150348 flags=0x0 type=0x48
  possible encoder 32: crtc=106 possible_crtcs=0x1
```

`connection=1` is `connector_status_connected`. The test program had defined
`DRM_CONNECTOR_STATUS_CONNECTED` as 2, which is
`connector_status_disconnected`, so it rejected the only connected connector on
every run. That enum is not exported through the UAPI headers, so its value was
guessed, and guessed wrong.

Commit `e261c4d` restates the enum from `include/drm/drm_connector.h`
(`connected = 1`, `disconnected = 2`, `unknown = 3`) with a comment. Nothing
else changed.

Result after the fix, run interactively on the device from `/tmp` (diagnostic
only: the binary inside the image is still the old one until the owner
rebuilds it):

```text
connector=33 crtc=106 mode=1080x1920@60
solid colour active on /dev/dri/card0; press Ctrl-C or wait for timeout
exit=0
```

That is a KMS PASS at the ioctl level: connector, CRTC, 1080x1920@60 mode, dumb
buffer, `ADDFB`, `MAP_DUMB` and legacy `SETCRTC` all succeed. It does not yet
claim a visible panel result; the owner confirms the colour while a 900 s red
hold is active.

Non-blocking `dmesg` noise, not assigned as causes: missing
`qcom/a530_pm4.fw` (Adreno GPU firmware; not needed for KMS scanout) and
`msm_mdp ... pp done time out, lm=2`.

## Owner rebuild (done 2026-08-30)

The owner rebuilt the binary from commit `e261c4d` and repackaged:

```bash
aarch64-linux-gnu-gcc-11 -static -std=gnu11 -O2 -Wall -Wextra -Werror \
  -o artifacts/op3-drm-dumb tests/drm/op3-drm-dumb.c
scripts/make-drm-test-initrd.sh
scripts/pack-boot.sh \
  out/pmos-msm8996-v6.12-v74strict/arch/arm64/boot/Image.gz \
  out/pmos-msm8996-6.3.1-v74full/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb \
  artifacts/initrd-op3-drm-test.cpio.gz \
  artifacts/boot-oneplus3-pmos612-v74dtb-drm-test.img
```

The resulting SHAs are recorded in “Artifacts” above. Rebuilding the binary
from `e261c4d` on the host reproduces
`057a4604870afe19b570efa0de2ca60a635e73b8054dabbb287ce1c050e5c5cf` bit for bit,
so the binary behind the observed red is exactly the committed source.

## Longer-hold variant (2026-08-30)

20 s per colour was too short to observe. `HOLD_SECONDS` is now 60 and
`FINAL_HOLD_SECONDS` is 1800; nothing else in the launcher changed. This is a
separate image so the PASS-evidence image above is preserved untouched.

```text
boot image: artifacts/boot-oneplus3-pmos612-v74dtb-drm-test-60s.img
  SHA256 f9693a7baf9e1fae5ff8ce27517ac6c246782576b8eb739093a84c690b7a3670
  kernel payload 50ffea424e6b7625b30acd5b14673ea287d2b04c7c283ee145f895247d8e881a
  ramdisk       fae0d703786840dce815ed572e2eee8f102f856c7c80a8476575b11e20992467
  cmdline       fbcon=nodefault console=tty0 pmos.debug-shell
```

Timeline after boot: wait for `/dev/dri/card0` (it appeared after 0 s in the
earlier run) → red 60 s → green 60 s → blue 60 s → red 1800 s → idle.

```text
fastboot boot artifacts/boot-oneplus3-pmos612-v74dtb-drm-test-60s.img
```

## Required next test image design

The current initramfs keeps `/dev/sda15` mounted at `/newroot` and runs
`/sbin/init_mainline.sh`. That established script sets up the tested RNDIS
endpoint (`172.16.42.1`), configfs RNDIS+ACM gadget, `/dev/ttyGS0` shell,
Dropbear SSH, `dmesg` archives, heartbeat, and `/root/boot_mainline.log`.
Afterwards its inittab respawns `/sbin/run_recovery.sh`, which starts
`recovery_mainline` and can compete with a direct display test.

For the next test, retain the pmOS 6.12 image's kernel, DTB, header, cmdline,
`/init`, and `init_mainline.sh`. In a derived initramfs only:

1. Include the corrected static `op3-drm-dumb` binary.
2. Replace the inittab target launcher `sbin/run_recovery.sh` with a dedicated
   DRM-test launcher; do not start `recovery_mainline`.
3. Have that launcher wait for `/dev/dri/card0`, capture its own standard
   output/error and relevant `dmesg` both in initramfs and persistently at
   `/newroot/var/log/op3-drm-dumb.log`, then run the RGB sequence.
4. Keep `init_mainline.sh` unchanged so SSH, USB RNDIS, ACM shell, and its
   existing logs remain available for observation.

This is a one-variable boot-userspace diagnostic. It modifies no kernel, DTB,
DRM/MSM, GPU, or PM code, and no sda15 content apart from the intended
persistent log file.

## Derived test image (built 2026-08-30, owner-authorized)

The derived initramfs is an **overlay**, not a repack.
`scripts/make-drm-test-initrd.sh` appends one gzip cpio member to the validated
reference initramfs. The kernel unpacks concatenated compressed initramfs
members in order (`init/initramfs.c`, `unpack_to_rootfs`) and opens a regular
file entry with `O_WRONLY|O_CREAT|O_TRUNC`, so the appended launcher replaces
the reference one while every other file, owner, and device node stays
byte-identical. Repacking the 50 MB archive as an unprivileged host user would
silently have changed uid/gid and dropped device nodes.

Appended entries:

| Path | Mode | Source |
| --- | --- | --- |
| `sbin/run_recovery.sh` | 0755 | `boot/drm-test-initramfs/sbin/run_recovery.sh` |
| `usr/bin/op3-drm-dumb` | 0755 | `artifacts/op3-drm-dumb` |

Launcher behaviour: wait up to 90 s for `/dev/dri/card0`, record the kernel
identity, the `/dev/dri` state, the DRM connector `status`/`enabled`/`mode` and
the backlight `brightness`/`max_brightness`/`bl_power` from sysfs, and filtered
`dmesg`; run `op3-drm-dumb red/green/blue` with a `HOLD_SECONDS` hold each; then
hold red for `FINAL_HOLD_SECONDS`; then idle forever. Both durations are
constants near the top of the launcher. The inittab entry is `::respawn:`, so the launcher
must not exit; a fast exit would make busybox init disable the entry. It never
starts `recovery_mainline`, so no other client can hold the DRM master. The sysfs
dumps are read-only and do not change the KMS sequence.

Artifacts:

```text
overlay initrd: artifacts/initrd-op3-drm-test.cpio.gz (intermediate; rewritten
  in place, so only its newest content survives on disk)
  PASS variant  c9a01a62eed8da4e788193b27c62deda92c72543db72a9bba9dc7854f7a6d132
                (50996287 bytes)
  60 s variant  fae0d703786840dce815ed572e2eee8f102f856c7c80a8476575b11e20992467
                (50996295 bytes; current content)
  both begin with the 50705116 bytes of artifacts/reference-initrd.img,
  SHA256 c3358a1cadb747996ddaa492e636827f2d72974040e8fd40d81f8a213e676366

boot image (PASS evidence, owner-built, 20 s holds):
  artifacts/boot-oneplus3-pmos612-v74dtb-drm-test.img
  62951424 bytes, SHA256
  b37ef527a0307fba9fc561bea1360ab17f89314c33c729f00580984e094e1f5a

test binary: artifacts/op3-drm-dumb
  SHA256 057a4604870afe19b570efa0de2ca60a635e73b8054dabbb287ce1c050e5c5cf
  reproduced bit-for-bit from commit e261c4d with the same compile command.
  The earlier a41b02d1976071f16f57afd6c365e9ddce93b7500864cd89141b4a1368fd4be3
  predates e261c4d and is superseded.
```

Verified on the rebuilt image: kernel payload still
`50ffea424e6b7625b30acd5b14673ea287d2b04c7c283ee145f895247d8e881a`, identical to
the control, and the ramdisk is exactly `artifacts/initrd-op3-drm-test.cpio.gz`.

Packing and verification were run by the agent; no device action was taken.

```bash
scripts/make-drm-test-initrd.sh
scripts/pack-boot.sh \
  out/pmos-msm8996-v6.12-v74strict/arch/arm64/boot/Image.gz \
  out/pmos-msm8996-6.3.1-v74full/arch/arm64/boot/dts/qcom/msm8996-oneplus3.dtb \
  artifacts/initrd-op3-drm-test.cpio.gz \
  artifacts/boot-oneplus3-pmos612-v74dtb-drm-test.img
```

The ramdisk is the only difference from the validated control image
`boot-oneplus3-pmos612-v74dtb-full-initrd.img`. Both kernel payloads
(`Image.gz` followed by the raw v74 DTB) hash to
`50ffea424e6b7625b30acd5b14673ea287d2b04c7c283ee145f895247d8e881a`, and both
images use header v0, 4096-byte pages, kernel `0x80008000`, ramdisk
`0x81000000`, tags `0x80000100`, and the cmdline
`fbcon=nodefault console=tty0 pmos.debug-shell`.

## Owner device procedure for this image

Use the established non-persistent boot procedure; do not flash:

```text
fastboot boot artifacts/boot-oneplus3-pmos612-v74dtb-drm-test.img
```

`init_mainline.sh` runs first, so the RNDIS endpoint (`172.16.42.1`), the ACM
shell and Dropbear come up as before. The RGB sequence then starts by itself;
no interactive command is needed. For the longer-hold variant
`boot-oneplus3-pmos612-v74dtb-drm-test-60s.img`, watch 60 s red, 60 s green,
60 s blue, then a 30 minute red hold.

Because recovery no longer runs, a black panel before the first colour is
expected: nothing else lights the display. The log is the only way to tell
“no modeset” apart from “modeset succeeded but the backlight is off”.

Collect the evidence from the device:

```text
/newroot/var/log/op3-drm-dumb.log   persistent copy on sda15 (survives reboot)
/var/log/op3-drm-dumb.log           initramfs copy for the running session
dmesg | grep -i -E "drm|msm|dsi|panel|mdp"
```

The launcher also writes its progress lines to `/dev/kmsg`, so they appear in
`dmesg` and in `init_mainline.sh`'s existing dmesg archive.

## PASS / FAIL record

PASS requires all of the following:

1. The program prints a selected connector, CRTC, and mode without an ioctl
   error.
2. The panel becomes the requested uniform red, green, or blue for the hold
   interval.
3. The result is reproducible for at least two distinct colours.

Status on 2026-08-30, with the rebuilt image
`b37ef527a0307fba9fc561bea1360ab17f89314c33c729f00580984e094e1f5a`:

| Criterion | Status | Evidence |
| --- | --- | --- |
| 1 connector/CRTC/mode without ioctl error | met | `connector=33 crtc=106 mode=1080x1920@60`, `solid colour active`, `exit=0` |
| 2 uniform colour on the panel | met for red | owner: “已经看到红色，持续显示着” |
| 3 reproducible in two colours | **not recorded** | only red observed; green and blue not yet watched |

FAIL is an open/ioctl/modeset error, no `/dev/dri/card0`, no connected
connector, a black/non-uniform panel, or a hang. Record the exact standard
error output and the observed panel state. A PASS proves only the DRM RGB
gate; it does not establish EGL, Wayland, Cog, WPE, GPU runtime PM, or Linux
7.2 readiness.

## Next step to complete criterion 3

Boot the rebuilt image once and watch the first ~70 s: it runs red, green and
blue for 20 s each, then holds red for 600 s. Recording that two of the three
colours appear completes criterion 3. Then read
`/newroot/var/log/op3-drm-dumb.log` for the per-colour exit codes.

```text
fastboot boot artifacts/boot-oneplus3-pmos612-v74dtb-drm-test.img
```

Only the Integration role may promote this to an accepted result; this entry
records evidence, not acceptance. The next layer after a full PASS is EGL, and
it needs its own task with its own single variable.
