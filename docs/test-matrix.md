# Test matrix

| ID | Layer | Commit | Artifact SHA256 | Device result | Evidence path |
| --- | --- | --- | --- | --- | --- |
| OP3-BOOT-001 | Android boot handoff | `d73a640c6` kernel / `bea302b` packaging docs | `88c91c749c87f32d78df374dd3bcc57af81512b1c2dc1b67aee7bd8f6fd8ad59` | `fastboot boot` sent and booted with `OKAY`, then device returned to fastboot; no Linux/display PASS claimed. | Owner terminal output, 2026-08-26 |
| OP3-BOOT-002 | Initramfs A/B | `d73a640c6` kernel / `977d79c` minimal initramfs | `d3a2f539893019309f706a5bfb4af7d684625f8faa1d271526f5cc884282b87a` | `fastboot boot` sent and booted with `OKAY`, then device returned to fastboot. FAIL: replacing the legacy initrd did not change the result. | Owner terminal output, 2026-08-26 |
| OP3-BOOT-003 | v100 repack validation | `47b5d04` documentation checkpoint; no Linux 7.2 source change | `be26aa0826389fc8c16f3acc7aa77782c7436b390f2cd650adff35c20f5ee099` | PASS: owner `fastboot boot` of v100 kernel payload + DTB + ramdisk repacked by `scripts/pack-boot.sh` entered the known v100 system. | Owner report, 2026-08-27 |
