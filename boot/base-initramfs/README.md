# OP3 reproducible base-initramfs control

`scripts/make-reproducible-base-initrd.sh` is the first migration step away
from an opaque historical ramdisk. It verifies the immutable historical input,
extracts it under `fakeroot` so that the `/dev/console` character device and
the `www-data` ownership survive, and writes a deterministically ordered
`gzip`/`newc` archive plus a content manifest.

This is deliberately a control experiment, not the final source migration:
the historical archive remains the pinned input. A successful device test
proves that reserializing this complete filesystem preserves boot behavior.
Later tasks can replace one declared input group at a time (firmware, then
base userspace/configuration) while retaining this recipe and manifest.

The fixed control archive has SHA256
`c3358a1cadb747996ddaa492e636827f2d72974040e8fd40d81f8a213e676366`.

Run the script from the repository root; it refuses to overwrite outputs.
Its `.manifest` sidecar lists every entry's type, mode, owner, group, size,
path, link target, and the SHA256 of every regular file. The script also
verifies that the generated archive has exactly the same pathname list as the
control archive.
