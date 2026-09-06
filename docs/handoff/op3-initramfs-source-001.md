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

Follow-up check: the two timestamp-controlled attempts produced Image.gz
hashes `10aea4f8…` and `92ab271e…` because `KBUILD_BUILD_TIMESTAMP` also
changes the temporary `init/utsversion-tmp.h` used for `init/version.o`.
The locked image was produced with the normal temporary value `# SMP PREEMPT`,
while only the final `include/generated/utsversion.h` carries the fixed
timestamp. An external-O= `-o init/utsversion-tmp.h` attempt did not suppress
that regeneration. The rebuild instructions now leave the Kbuild variable
unset and provide the fixed timestamp through the tracked `date` wrapper only
for the final generated header. The owner should rerun that targeted sequence
and verify the temporary and final headers before artifact verification.
~~~
