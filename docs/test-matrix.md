# Test matrix

| ID | Layer | Commit | Artifact SHA256 | Device result | Evidence path |
| --- | --- | --- | --- | --- | --- |
| OP3-BOOT-001 | Android boot handoff | `d73a640c6` kernel / `bea302b` packaging docs | `88c91c749c87f32d78df374dd3bcc57af81512b1c2dc1b67aee7bd8f6fd8ad59` | `fastboot boot` sent and booted with `OKAY`, then device returned to fastboot; no Linux/display PASS claimed. | Owner terminal output, 2026-08-26 |
