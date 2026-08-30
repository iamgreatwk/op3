# OnePlus 3 direct DRM dumb-buffer test

`op3-drm-dumb.c` is a deliberately small, libdrm-free KMS test. It uses the
DRM UAPI to find a connected connector on `/dev/dri/card0`, creates a 32-bit
dumb buffer, fills it with one RGB colour, and calls the legacy KMS modeset
ioctl. It does not test Mesa, GBM, EGL, Wayland, Weston, Cog, WPE, GPU runtime
PM, or browser rendering.

The target test kernel is the pmOS 6.12 full-initrd control
(`out/pmos-msm8996-v6.12-v74strict` plus the v74 DTB). Use that one, not the
upstream v6.12.1 DSI control: the v6.12.1 image reaches `recovery.c` but the
owner has never seen it expose USB RNDIS or the ACM shell, so it cannot produce
log evidence.

The project owner compiles and deploys the test binary. Agents must not build
or operate the device. See `docs/handoff/pmos612-drm-dumb-buffer.md` for the
exact command and PASS/FAIL conditions.

`boot/drm-test-initramfs/sbin/run_recovery.sh` is the automatic launcher, and
`scripts/make-drm-test-initrd.sh` appends it, together with the binary, to the
validated reference initramfs as one extra cpio member.
