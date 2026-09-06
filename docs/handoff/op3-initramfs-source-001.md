# OP3 self-built initramfs source migration handoff

~~~text
Task / GitHub Issue: OP3-INITRAMFS-SOURCE-001 (owner-requested next step)
Role: Implementation
Baseline commit: eac9462a6b11f9c3c309408bcc6f2e7ae66dcb0d
Working branch: agent/implementation/recovery-browser-001
Changed files: boot/initramfs/; buildroot/package-patches/op3-initramfs/;
  buildroot/op3-recovery.defconfig;
  buildroot/op3-recovery-post-build.sh;
  boot/wifi/initramfs/usr/bin/wifi_auto.sh;
  boot/wifi/opt/op3-wifi/wifi-start; boot/wifi/usr/bin/wifi;
  scripts/prepare-op3-buildroot.sh;
  scripts/stage-op3-initramfs-firmware.sh;
  scripts/restore-op3-recovery-kernel.sh;
  scripts/verify-op3-recovery-manifest.sh;
  manifests/op3-recovery-audio-full.env;
  boot/base-initramfs/README.md; docs/boot-image-format.md;
  docs/rebuild-recovery.md; docs/handoff/latest.md
Checkpoint commits: cde247f, af90564, 40f8f7f, 555afc2

Layer: initramfs source and Buildroot packaging
Hypothesis tested: A complete OP3 recovery initramfs can be generated from
GitHub-tracked startup sources and the pinned Buildroot tree, without consuming
the historical reference-initrd archive, while the same Buildroot target
remains usable as the optional persistent /newroot payload.
Only variable changed: initramfs source ownership and Buildroot output format;
kernel source, kernel configuration, DTS, recovery application logic, and
browser stack selection were not changed.

Build run by project owner: ATTEMPTED
Build result: Buildroot defconfig completed, then the full build stopped in
the host dependency preflight because `/usr/bin/install` was uutils coreutils
0.8.0. No project package was compiled and no generated initramfs or boot image
was produced.
Artifacts and SHA256: No new Buildroot initramfs or boot image was generated.
The expected source output is
out/buildroot-op3-recovery/images/rootfs.cpio.gz; the manifest marks its
artifact hash OWNER_BUILD_REQUIRED until the owner performs a clean build.

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: `/tmp/buildroot-op3-recovery.log` contains the
host preflight failure; `/home/kai/op3-rebuild-clean-20260906/host-tools/install`
now points to `/usr/bin/gnuinstall`. No device action was taken.

Conclusion: INCONCLUSIVE
Uncertainties: After the host `install` workaround, Buildroot package
ordering, static helper compilation,
devtmpfs/inittab startup, firmware requests from the self-built CPIO, and
recovery/Wi-Fi/audio runtime behavior require the owner build and OnePlus 3
boot test. Proprietary firmware remains outside GitHub and is still required.

Recommended next experiment: prepend the local `host-tools` directory to
PATH, rerun the Buildroot build from its existing recovery output directory,
and report the first package/build failure or generated image. The owner
should then inspect images/rootfs.cpio.gz, pack it with the pinned kernel/DTB,
and report the first failure or device evidence.
~~~
