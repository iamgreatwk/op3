# OnePlus 3 DRM dumb-buffer test initramfs overlay

This directory holds the boot-userspace part of the direct DRM RGB gate
described in `docs/handoff/mainline-v6121-drm-dumb-buffer.md`.

It is an **overlay**, not a replacement initramfs. `scripts/make-drm-test-initrd.sh`
appends one gzip cpio member to the validated reference initramfs
(`artifacts/reference-initrd.img`); the kernel unpacks concatenated compressed
members in order, so the appended entries win while every other file stays
byte-identical.

Appended entries:

| Path | Purpose |
| --- | --- |
| `sbin/run_recovery.sh` | Replaces the recovery selector. Waits for `/dev/dri/card0`, runs `op3-drm-dumb red/green/blue`, then a long red hold. Never starts `recovery_mainline`, which would hold the DRM master. |
| `usr/bin/op3-drm-dumb` | The static ARM64 test binary built from `tests/drm/op3-drm-dumb.c`. |

Logs are written to `/var/log/op3-drm-dumb.log` in the initramfs and mirrored to
`/newroot/var/log/op3-drm-dumb.log` while sda15 is mounted at `/newroot`.

Unchanged on purpose: kernel `Image.gz`, DTB, `/init`, `sbin/init_mainline.sh`,
`etc/inittab`, cmdline, and the sda15 content. `init_mainline.sh` still runs
first, so USB RNDIS, the ACM shell, Dropbear and its logs stay available.

The launcher never exits: inittab respawns this entry, and a fast exit would
make busybox init disable it.
