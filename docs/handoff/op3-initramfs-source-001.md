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
Checkpoint commits: cde247f, af90564, 40f8f7f, 555afc2, 026c359

Layer: initramfs source and Buildroot packaging
Hypothesis tested: A complete OP3 recovery initramfs can be generated from
GitHub-tracked startup sources and the pinned Buildroot tree, without consuming
the historical reference-initrd archive, while the same Buildroot target
remains usable as the optional persistent /newroot payload.
Only variable changed: initramfs source ownership and Buildroot output format;
kernel source, kernel configuration, DTS, recovery application logic, and
browser stack selection were not changed.

Build run by project owner: ATTEMPTED
Build result: The owner completed the kernel and Buildroot builds after the
host `install` and download-mirror workarounds. Buildroot generated
`rootfs.cpio.gz` with SHA256
`3a704c8f64dde483204f4391997cb60230bf736478009330bf27df2085e0bf6c`, and the
boot packer produced an image with SHA256
`904fe32e7b619b9fb0008a9b87843e81326dceef07a6475c812b2cee081a00b5`.
The artifact verifier stops because the newly built `Image.gz` has SHA256
`0d51f0978efd047c4974be1d5d14ed62f1023c93218e40863f7e70a4372662e1`, while
the locked value is `5c89259d9340071c9c8684d361042482ca25c8f76b2de4d33cdacf2105f78861`.
The only observed input difference is the kernel `UTS_VERSION` build
timestamp; the config and DTB match.
Artifacts and SHA256: No new Buildroot initramfs or boot image was generated.
The expected source output is
out/buildroot-op3-recovery/images/rootfs.cpio.gz; the manifest marks its
artifact hash OWNER_BUILD_REQUIRED until the owner performs a clean build.

Device test run by project owner: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: `/tmp/buildroot-op3-recovery.log` contains the
successful finalization and image generation; `/home/kai/op3-rebuild-clean-20260906/host-tools/install`
points to `/usr/bin/gnuinstall`; `source/buildroot/dl/libtool/libtool-2.4.6.tar.xz`
is cached. No device action was taken.

Conclusion: INCONCLUSIVE
Uncertainties: After the timestamp-controlled kernel rebuild, artifact
verification and device boot behavior remain untested. Buildroot package
ordering, static helper compilation,
devtmpfs/inittab startup, firmware requests from the self-built CPIO, and
recovery/Wi-Fi/audio runtime behavior require the owner build and OnePlus 3
boot test. Proprietary firmware remains outside GitHub and is still required.

Follow-up check: the latest owner run produced Image.gz SHA256
`3175a92fa47c54ad33c131b4a353f8e425c898270cdcda53bafda9032f84bc8d`, still
different from the locked `5c89259d9340071c9c8684d361042482ca25c8f76b2de4d33cdacf2105f78861`.
The output configuration was present and matched the tracked config, and
`olddefconfig` reported no change. The first phase nevertheless stopped
because `make -B` forces the top-level `.config` target; Linux defines that
target as an error guard rather than a rebuild recipe. The shell then
continued into later phases, producing a different invalid hash. The guide
now uses `set -e` and removes `-B`; the timestamped CPIO command itself causes
the required CPIO rebuild.

The remaining reproducibility issue is the empty default initramfs: its
archived directory mtime is `2026-09-06 14:32:51 +0800`, while GNU date
interprets literal `CST` as US Central time. The corrected sequence generates
the CPIO with numeric `+0800`, removes only the timestamp option from its
saved `.cmd` record, compiles `init/version.o` with the normal temporary
value, and uses the tracked `scripts/op3-repro-date.sh` wrapper to fix only the
final UTS timestamp. This avoids relying on recursive `MAKEFLAGS` old-file
exceptions. The owner should rerun that targeted kernel sequence and verify
the locked hash before artifact verification.
~~~
