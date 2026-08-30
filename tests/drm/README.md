# OnePlus 3 direct DRM dumb-buffer test

`op3-drm-dumb.c` is a deliberately small, libdrm-free KMS test. It uses the
DRM UAPI to find a connected connector on `/dev/dri/card0`, creates a 32-bit
dumb buffer, fills it with one RGB colour, and calls the legacy KMS modeset
ioctl. It does not test Mesa, GBM, EGL, Wayland, Weston, Cog, WPE, GPU runtime
PM, or browser rendering.

The target test kernel is the existing booting upstream v6.12.1 DSI control:
`source/linux-mainline-6.12.1`, branch
`agent/implementation/mainline-v6121-dsi-pm-ab`, commit
`548b0dc49481bf0c4d6fe63cec76b0e516ec3f91`.

The project owner compiles and deploys the test binary. Agents must not build
or operate the device. See
`docs/handoff/mainline-v6121-drm-dumb-buffer.md` for the exact command and
PASS/FAIL conditions.
