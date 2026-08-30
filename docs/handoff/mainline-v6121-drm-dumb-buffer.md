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

Layer: 04 DRM RGB (parallel diagnostic; not a Linux 7.2 acceptance result)
Previous PASS milestone: v6.12.1 DSI control reaches recovery.c
Sole hypothesis: The booting v6.12.1 DSI-control image can allocate, map, and
modeset a DRM dumb buffer through /dev/dri/card0, visibly producing a solid
RGB frame.
Only variable changed: The direct-KMS test program placed on the existing
sda15 root filesystem. Kernel, DTB, initramfs, cmdline, DRM/MSM, GPU, PM, and
userspace stack are unchanged.

Build run by project owner: NOT_RUN
Build result: NOT_RUN
Artifacts and SHA256: NOT_RUN; owner will produce artifacts/op3-drm-dumb

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: use the fixed boot control below

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
