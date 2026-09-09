# pmOS 6.12 OnePlus 3 IMX298 camera patch series

This directory is the project-owned GitHub archive of the OP3 rear IMX298
probe work. It is the durable source input; the nested kernel worktree is not
required for a clean rebuild and its GitLab remote is not used for publishing.

## Base and order

Start with the formal pmOS MSM8996 Linux v6.12.1 baseline pinned in
`BASELINE.env`. First apply the integrated recovery series
`patches/pmos612-op3-recovery-audio-full/` through patch `0033`, then apply
this camera series in lexical order:

```bash
git am --3way patches/pmos612-op3-recovery-audio-full/00*.patch
git am --3way patches/pmos612-op3-camera-imx298/000*.patch
```

The camera series was exported from nested-kernel commits
`c37102be3c5b..0274f7062cfe`. Patch `0012` deliberately removes the
experimental PM8994 `lvs1 {}` DT child and keeps IMX298 VIO on the known-good
`vreg_s4a_1p8`; the two preceding direct-SPMI LVS1 experiments rebooted
before userspace. Patch `0013` is a new, isolated registration-only test using
the OP3 board's actual `qcom,rpm-pm8994-regulators` SMD-RPM node; it does not
route IMX298 VIO to LVS1. Patch `0014` is the follow-up A/B that changes only
the IMX298 `vio-supply` phandle to the now-registered LVS1 output.
Patch `0009` is a separate CCI-bus-speed experiment: it changes only CCI0
from the previous 1 MHz setting to the OP3 Android camera stack's 400 kHz
fast mode. Patch `0010` is diagnostic-only: it adds power, reset, MCLK, rail,
and chip-ID read logging without changing the existing behavior. Patch `0011`
changes only the initial reset request from `GPIOD_OUT_LOW` to
`GPIOD_OUT_HIGH`. Patch `0012` follows the original OP3 IMX298
power-resource order and adds only the documented auxiliary S5 rail and
GPIO39 VAF control; it does not add the crashing `lvs1` node.
Patch `0015` adds one minimum 1476x834 RAW10 RGGB mode, V4L2 format
negotiation, and `s_stream` register programming. It is derived from the
phone's vendor mode data as explicit register writes; no vendor binary is
linked into the kernel. Exposure is covered by patch `0017`; gain, additional
modes, and capture userspace remain separate tasks.
Patch `0016` adds the fixed CSI-2 link-frequency and pixel-rate controls
required by CAMSS to configure the sensor's four-lane transmitter. It keeps
the experiment limited to the existing 1476x834 RAW10 mode. Patch `0017` adds
only the V4L2 exposure control and applies the cached value immediately before
stream-on; it does not change the mode table, power sequence, DTS, or CAMSS.
Patch `0018` adds only the V4L2 analogue-gain control and writes the sensor's
`0x0204/0x0205` gain register pair; it does not change exposure, mode timing,
power sequencing, DTS, or CAMSS.

| Patch | Original commit | SHA256 | Purpose |
| --- | --- | --- | --- |
| `0001` | `c37102be3c5b` | `bcf6c7c2d101552fc230c23ebb446241313bba98d806aa2f0cc8e08d6c943553` | Add probe-only IMX298 driver. |
| `0002` | `4b7e09c7c851` | `506e88e843ca5770e8240d37f5ea14572ac20c41df0b70051728b88003bd6075` | Add IMX298 DT binding. |
| `0003` | `f50ebce60238` | `a306be3e993c6dceb54ebb1e734fdfdadccee02a26f8dd80d39ddc3ec35aa4c1` | Enable OP3 IMX298 CCI/CAMSS DT wiring. |
| `0004` | `58974c2450dd` | `dd0dd8b4c5caf05fc4b144a276d624728b4b5d96824908a8d2e4dda9845cf041` | Make reset GPIO mandatory. |
| `0005` | `42da3ad95668` | `0e941817bc58812b44600d61abd5f7a41442baca75401f629138a18759381bb8` | Fix binding maintainer metadata. |
| `0006` | `ec5025c75ff4` | `dd59ccb4b74f1d7b28162bf32b4ec59b5f539884eb78f2893bc267333893f647` | Experimental LVS1 VIO mapping. |
| `0007` | `a112a6f19fa2` | `f939375bc3c72b83eb3e9ba39946b4b7929a2d1d72112cd7d597e89699281977` | Isolate the LVS1 node boot test. |
| `0008` | `306d4a364565` | `fdc67826a0690187d857f1e8e02750b06a4f87ca94a6fd20c967c7969a33c868` | Remove the crashing LVS1 node. |
| `0009` | `b0594dbd5bf4` | `8c6a4424aa02e8ee81627e8ca9ba0ba2db6ff7305dff411549ec2dc150555f3b` | Use OP3 IMX298 CCI fast mode at 400 kHz. |
| `0010` | `c79909f9a448` | `e6c9f869e12545c1875889616fa27668ffb0981efb310c91bc170ffc43e4bd73` | Add power-on, reset, MCLK, rail, and chip-ID diagnostics. |
| `0011` | `d247ce811242` | `4195cf9be185e6b7cead5baf6a8f56dfb2e61b6ac340772a9a9830a29992ede9` | Hold IMX298 in reset during probe setup. |
| `0012` | `7fe1f2f950b2` | `5f066b1e2a1870a8142ac7204dfcd6322c56dda6ab020fdd25656214908c0ddf` | Follow the original OP3 IMX298 power order; add S5/CUSTOM1 and GPIO39/VAF. |
| `0013` | `67630ec3b9e5` | `0113d8a19b38670460536dc8103f23d125b8740380d32969a16673491c3eae1e` | Register PM8994 LVS1 through the SMD-RPM topology, without changing camera VIO. |
| `0014` | `e5332d149d85` | `0b95a97359ffd75e7ec7850eb8c97ebc8c0c6c4dca24a1862d9f2bfd70ca91d9` | Route only IMX298 VIO from S4 to the registered SMD-RPM LVS1 output. |
| `0015` | `a059fc816af6` | `0cd6ecb9c66ff05e74abf1f7e2eaec38eaca645be43b5e872d3e4c35a2d0f09c` | Add one minimum 1476x834 RAW10 mode and V4L2 stream operations. |
| `0016` | `cc609f6d10a1` | `05e09e6786fd9b4825c3251eff66f36cf64fc17a232411f54837225f7f4bac84` | Expose IMX298 CSI-2 link-frequency and pixel-rate controls. |
| `0017` | `e96efb4b1dd3` | `b093e2213449d0aca6ac8efdfb8bce046f8abbc2cfbcec3eeee280e838b665c2` | Add V4L2 exposure control for the minimum RAW10 mode. |
| `0018` | `0274f7062cfe` | `e22a369100495a92aeba83c9602eafacff9b9cfaf4bf7bdb6c826077e2c18946` | Add V4L2 analogue-gain control for the minimum RAW10 mode. |

These patches provide one experimental stream mode plus exposure and analogue-gain
controls. They do not provide additional modes, capture userspace, front IMX179,
OIS, actuator, flash, or EEPROM support. The device result and build commands
are recorded in `docs/handoff/op3-camera-imx298-001.md`.
