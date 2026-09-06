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

Build run: OWNER-AUTHORIZED CODEX EXECUTION
Build result: The clean rebuild completed after correcting the reproducible
kernel CPIO step. Buildroot generated `rootfs.cpio.gz` with SHA256
`3a704c8f64dde483204f4391997cb60230bf736478009330bf27df2085e0bf6c`.
The kernel's empty default CPIO was regenerated with the locked numeric
timestamp and has SHA256
`e15eb1c349081c2650e535ec9774c6ed0afa178226ec89af09eed62be21e14c9`.
`Image.gz` has the locked SHA256
`5c89259d9340071c9c8684d361042482ca25c8f76b2de4d33cdacf2105f78861`, and
the repacked boot image has SHA256
`f0aed8d6e62c6702f68b28003eebc657ef0871d88d4aa3769f21a0dbd13fed46`.
Both source and artifact manifest verification passed.
Artifacts and SHA256: The Buildroot initramfs and boot image are present at
the manifest paths above and their hashes are now recorded in
`manifests/op3-recovery-audio-full.env`.

Device test run: NOT_RUN
Device result: NOT_RUN
Evidence links / log paths: `verify-op3-recovery-manifest.sh --source` and
`--artifacts` both returned PASS in the clean rebuild. The checked outputs are
under `/home/kai/op3-rebuild-clean-20260906/out/` and the packed artifacts
under its `artifacts/` directory. `/home/kai/op3-rebuild-clean-20260906/host-tools/install`
points to `/usr/bin/gnuinstall`; `source/buildroot/dl/libtool/libtool-2.4.6.tar.xz`
is cached. No device action was taken.

Conclusion: INCONCLUSIVE
Uncertainties: Device boot behavior, devtmpfs/inittab startup, firmware
requests from the self-built CPIO, and recovery/Wi-Fi/audio runtime behavior
remain untested on the OnePlus 3. Proprietary firmware remains outside GitHub
and is still required.

Follow-up check: earlier attempts produced invalid Image.gz hashes because
`make -B` forced the top-level `.config` error guard and a later retry kept an
already-normalized CPIO command unchanged. The corrected sequence uses
`set -e`, invokes the kernel's `usr/gen_initramfs.sh` directly with the locked
numeric timestamp, normalizes the saved `.cmd` record, and completed with the
locked CPIO and Image.gz hashes recorded above.

The reproducibility fix uses numeric `+0800` for the CPIO mtime, compiles
`init/version.o` with the normal temporary value, and uses the tracked
`scripts/op3-repro-date.sh` wrapper to fix only the final UTS timestamp. This
avoids relying on recursive `MAKEFLAGS` old-file exceptions. Build and
artifact verification are complete; the remaining follow-up is the OnePlus 3
device test.
~~~
