# OnePlus 3 upstream v6.12.1 direct DRM dumb-buffer gate

```text
Task / GitHub Issue: Owner-authorized no-Issue direct KMS smoke test
Role: Implementation
Formal baseline: Linux v7.2 pristine upstream (unchanged)
Diagnostic kernel: upstream v6.12.1 DSI control only
Baseline commit: 85792c20e50d6b27550b9c02b371e6ff37d4f697
Working branch: agent/implementation/s6e3fa5-linux72-port
Changed files: tests/drm/op3-drm-dumb.c; tests/drm/README.md
Source commit SHA: 0e1d84b
Fix commit SHA: d43821a (EFAULT root-cause fix, on top of 0e1d84b)

Layer: 04 DRM RGB (parallel diagnostic; not a Linux 7.2 acceptance result)
Previous PASS milestone: v6.12.1 DSI control reaches recovery.c
Sole hypothesis: The booting v6.12.1 DSI-control image can allocate, map, and
modeset a DRM dumb buffer through /dev/dri/card0, visibly producing a solid
RGB frame.
Only variable changed: The direct-KMS test program placed on the existing
sda15 root filesystem. Kernel, DTB, initramfs, cmdline, DRM/MSM, GPU, PM, and
userspace stack are unchanged.

Build run by project owner: NOT_RUN (no kernel, Buildroot, Mesa, WebKit, WPE,
or other large-project build)
Build result: NOT_RUN
Artifacts and SHA256: a static ARM64 test executable was generated locally as
`artifacts/op3-drm-dumb`, SHA256
`d74da26cc2914ea083a2c4d1cbc6095812673226b87c3e89de0ffbdd540e2675`. That
binary is superseded: it was built from `0e1d84b`, which contains the EFAULT
defect. Build a new one from `d43821a` and record its SHA256 here.

Device test run by project owner: partial diagnostic execution, 2026-08-30
Device result: INCONCLUSIVE / test-program failure before modeset
Evidence links / log paths: see “2026-08-30 deployment and first run” below

Conclusion: INCONCLUSIVE
Uncertainties: This legacy KMS path does not exercise atomic KMS, GBM, EGL,
Wayland, Weston, Cog, or WPE. A successful static binary also depends on the
running image exposing /dev/dri/card0 to the sda15 root filesystem.
Recommended next experiment: Run red, green, and blue cases and record both
the program output and the visible panel result. Only after a PASS, test EGL.
```

## Fixed boot control

Do not rebuild or repack anything for this gate. Boot the already validated
upstream v6.12.1 DSI-control image:

```text
kernel source: source/linux-mainline-6.12.1
source branch: agent/implementation/mainline-v6121-dsi-pm-ab
source commit: 548b0dc49481bf0c4d6fe63cec76b0e516ec3f91
boot image: artifacts/boot-oneplus3-mainline-6121-dsi-pm-v74dtb-full-initrd.img
boot image SHA256: 87bcc4ba5fe76768d8a0b310d68ecde07c2aa2ee0f63fde4c79ebc6abbf65f19
cmdline: fbcon=nodefault console=tty0 pmos.debug-shell
```

The owner previously reported this exact control reaches `recovery.c`. Its
v74 DTB and complete reference initramfs are fixed test companions here; they
are not a formal Linux 7.2 DTS or rootfs decision.

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

Use the existing owner-established procedure to copy `op3-drm-dumb` into the
root filesystem on sda15 (for example, `/usr/bin/op3-drm-dumb`). Do not change
the kernel image or its companions, and do not infer a new USB address,
credential, mount point, or boot command. Once that established procedure has
started the v6.12.1 control image and made the sda15 root filesystem active,
run:

```bash
chmod 0755 /usr/bin/op3-drm-dumb
exec /usr/bin/op3-drm-dumb red --hold
```

Use Ctrl-C in that session to restore the previous CRTC state and exit.
Repeat with `green --hold` and `blue --hold`; or use, for example,
`red --seconds 30` for an automatic return after 30 seconds.

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

This is a one-variable boot-userspace diagnostic. It must be committed and
explicitly authorized for packaging before any derived boot image is created;
it must not modify kernel, DTB, DRM/MSM, GPU, PM, or sda15 content apart from
the intended persistent log/test binary.

## PASS / FAIL record

PASS requires all of the following:

1. The program prints a selected connector, CRTC, and mode without an ioctl
   error.
2. The panel becomes the requested uniform red, green, or blue for the hold
   interval.
3. The result is reproducible for at least two distinct colours.

FAIL is an open/ioctl/modeset error, no `/dev/dri/card0`, no connected
connector, a black/non-uniform panel, or a hang. Record the exact standard
error output and the observed panel state. A PASS proves only the DRM RGB
gate; it does not establish EGL, Wayland, Cog, WPE, GPU runtime PM, or Linux
7.2 readiness.
