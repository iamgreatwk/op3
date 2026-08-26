# OnePlus 3 USB ACM diagnostic console

This temporary initramfs PID 1 creates a configfs CDC ACM gadget, waits for
`/dev/ttyGS0`, streams `/dev/kmsg` to it, and provides the limited `help`,
`cmdline`, and `udc` commands. It intentionally is not a full shell and does
not mount a root filesystem or use pmOS UUIDs.

Apply `kernel/configs/oneplus3-usb-acm-debug.fragment` in addition to the FA5
fragment. The parent configfs symbols must be built in, because a boot.img-only
initramfs cannot load modules. Build this source statically with the project
ARM64 compiler and package it as `/init` in a gzip `newc` initramfs.
