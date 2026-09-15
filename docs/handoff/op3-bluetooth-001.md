# Agent handoff: OP3 QCA6174 Bluetooth UART/HCI bring-up

Task / GitHub Issue: [#14 — OP3 QCA6174 Bluetooth UART/HCI bring-up](https://github.com/iamgreatwk/op3/issues/14)
Role: Implementation
Baseline commit: top-level `ec00554`; formal kernel baseline is pmOS MSM8996 Linux 6.12.1. The existing OP3 DTS already describes the Bluetooth UART and is unchanged.
Working branch: `agent/implementation/op3-bluetooth-001`
Changed files at this checkpoint: handoff documentation only in the project repository; external Bluetooth firmware is stored outside GitHub under `$OP3_EXTERNAL_INPUTS/bluetooth/`.
Commit SHA: `6d1490b` plus the owner-authorized temporary-test record below

## Scope and hypothesis

Layer: Bluetooth UART/HCI, firmware staging, and userspace validation.

Hypothesis: the existing `blsp1_uart2` / `qcom,qca6174-bt` device-tree path is
usable; the missing runtime pieces are the kernel Bluetooth module closure,
the controller firmware, and BlueZ userspace.

Only variable changed in the diagnostic: temporary loading of the existing
Bluetooth modules and temporary firmware filename mapping. No kernel source,
DTS, DRM, camera, sensor, or default Buildroot artifact was changed during
the device diagnosis.

## Static evidence

The OP3 common DTS already enables `serial@7570000` as `BT-UART`, enables
RTS/CTS, and declares `qcom,qca6174-bt` with PM8994 GPIO19 and `divclk4`.
The integrated kernel configuration already contains `CONFIG_BT`,
`CONFIG_BT_QCA`, `CONFIG_BT_HCIUART`, `CONFIG_BT_HCIUART_SERDEV`,
`CONFIG_BT_HCIUART_H4`, and `CONFIG_BT_HCIUART_QCA` as modules or built-in
support. The existing owner-built module tree contains the closure:

```text
kernel/net/bluetooth/bluetooth.ko
kernel/drivers/bluetooth/btqca.ko
kernel/drivers/bluetooth/hci_uart.ko
kernel/net/rfkill/rfkill.ko
```

The default Buildroot profile did not yet select BlueZ or install the
Bluetooth modules and firmware. That is the next source change, kept separate
from the kernel/DTS line.

## Device diagnostic

The owner device was reached over SSH at `172.16.42.1`. The existing DTS node
was present, but the recovery rootfs had no Bluetooth modules, BlueZ client, or
HCI tools. Loading the existing three Bluetooth modules manually proved the
UART and controller path reaches the QCA device:

```text
QCA Product ID 0x00000008
QCA SOC Version 0x00000044
QCA ROM Version 0x00000302
QCA Patch Version 0x00000111
QCA controller version 0x00440302
```

The preserved vendor backup
`firmware/bluetooth.img` supplied the two controller files. They are kept
outside GitHub at:

```text
$OP3_EXTERNAL_INPUTS/bluetooth/lib/firmware/qca/rampatch_00440302.bin
$OP3_EXTERNAL_INPUTS/bluetooth/lib/firmware/qca/nvm_00440302.bin
```

Their hashes are:

```text
83750a09722272c7a42fae7ca23edf55f5d3d0e15b221c12241afd4ece51084a  rampatch_00440302.bin
dda2c085ffff278d97a473116477df05e8b150ee78cc1ba4c3e039653b7a8351  nvm_00440302.bin
```

With temporary firmware aliases, the driver created `/sys/class/bluetooth/hci0`
and completed setup, but logged `Frame reassembly failed (-84)` while loading
the patch. This is **INCONCLUSIVE**, not a Bluetooth pass. The next test must
use the staged files in the normal init sequence and retain `btmon`/`dmesg`
evidence; do not claim pairing success from the temporary module load.

## Owner-authorized temporary runtime test (2026-09-15)

The existing owner-built module artifacts were copied with `scp -O` to the
running recovery system at `172.16.42.1`; no Buildroot build or flash was
performed. The uploaded module hashes were:

```text
499d85fe1fe7156a2a59ea51d9902316dcf808715e2295f33ffd42b0774caae3  rfkill.ko
12ffd171d64806016912ab9afada57288ae7bdfa31a2ebad9bea3edbc8ed20ba  bluetooth.ko
87a8a9a849cd94405730f1e95ba055a6824632010c4d3c8f0ea6b338d1f2d115  btqca.ko
bc719414a676ad12ffc0307362681787061d7d14e8dfdf4871dbaa1ce431f15b  hci_uart.ko
```

The two staged firmware hashes matched the external-input manifest. Loading
`bluetooth`, `btqca`, and `hci_uart` in that order created:

```text
/sys/class/bluetooth/hci0
```

The controller identification was stable:

```text
QCA Product ID   0x00000008
QCA SOC Version  0x00000044
QCA ROM Version  0x00000302
QCA Patch Version 0x00000111
QCA controller version 0x00440302
```

With the staged files present, the driver reported:

```text
QCA Downloading qca/rampatch_00440302.bin
Frame reassembly failed (-84)
QCA Downloading qca/nvm_00440302.bin
QCA setup on UART is completed
```

The earlier `-2` file-not-found message occurred before the temporary firmware
was copied and is not the current failure. Result: **UART/HCI enumeration
DIAGNOSTIC PASS; firmware patch transport INCONCLUSIVE; Bluetooth feature
NOT ACCEPTED**. BlueZ was not present in this running rootfs, so scanning and
pairing were not tested.

## Read-only follow-up and NVM A/B test (2026-09-15)

The live device-tree node was inspected on the running phone. It contains
`qcom,qca6174-bt`, `enable-gpios`, `clocks`, and the RTS/CTS UART property, but
no `vdd*-supply` properties. This matches the 6.12 driver path: the
`qcom,qca6174-bt` match has no `qca_device_data`, so the controller uses the
QCA_ROME path, which toggles BT_EN and SUSCLK but does not initialize the
newer generic regulator table. The runtime HCI device is bound to
`hci_uart_qca` and exposes an rfkill node.

The preserved downstream DTS documents four vendor power consumers (S3 core,
S4 I/O, L30 XTAL, and a board-specific `rome_vreg` PA rail), but the latter is
not defined in the mainline DTS. The runtime regulator states alone do not
prove which consumer owns those rails, so no guessed `rome_vreg` node or
unrelated power change was added.

One reversible runtime A/B test copied the preserved `btnv32.b15` over the
temporary `qca/nvm_00440302.bin` path, unloaded and reloaded the temporary
Bluetooth modules, and then restored the original `btnv32.bin`. The result was
unchanged:

```text
QCA Downloading qca/rampatch_00440302.bin
Frame reassembly failed (-84)
QCA Downloading qca/nvm_00440302.bin
QCA setup on UART is completed
```

This isolates the visible `-84` from the NVM variant: it occurs before the NVM
download and does not prevent the controller setup from completing. The same
QCA6174/Rome `0x00440302` sequence, including a single `-84` followed by the
NVM step, is also present in independent Linux runtime logs. Treat `-84` as a
transport diagnostic until a real HCI operation fails; it is not by itself a
reason to alter H4 parsing or add guessed power rails.

## Temporary BlueZ scan and S01 test (2026-09-15)

The running phone was booted with the existing recovery image and was not
flashed. To avoid a Buildroot rebuild, only a temporary BlueZ tool bundle,
the three Bluetooth kernel modules, and the two verified QCA firmware files
were copied into `/tmp` and `/lib/firmware/qca` on the phone. No persistent
rootfs or Buildroot artifact was changed.

The controller's stock temporary NVM contained the invalid address
`00:00:00:00:5A:AD`, so the volatile test used:

```text
btmgmt --index 0 public-addr 02:00:00:00:00:01
```

After that workaround, `hci0` initialized and a real BR/EDR/LE discovery
passed. The S01 speaker was found as:

```text
name S01
address 16:6E:52:FA:45:A0
type BR/EDR
rssi -52
```

Pairing was attempted twice. Each attempt established a BR/EDR connection,
then disconnected with reason `1` and ended with status `0x08 (Timeout)`.
There was no PIN request or confirmation event. Therefore the result is:

```text
scan: PASS
S01 discovery: PASS
S01 pairing: INCONCLUSIVE / NOT ACCEPTED
```

The most likely remaining test condition is that S01 was powered on but not
in its explicit pairing mode. The next test must be performed only after the
speaker is placed in pairing mode, using the same temporary runtime setup.
The previously attempted iPhone pairing is deliberately excluded from this
result at the owner's request.

No Buildroot compilation was performed for this test.

## Next isolated implementation

Add only the userspace/runtime integration:

1. stage the two verified QCA firmware files;
2. select BlueZ `bluetoothd`, `bluetoothctl`, `btmon`, and `btattach` tools in
   the optional Bluetooth Buildroot change;
3. copy the exact module closure from the owner-built kernel module tree;
4. load `bluetooth`, `btqca`, and `hci_uart` before Buildroot's DBus and BlueZ
   startup scripts;
5. boot a clean owner-built image and check `hci0`, `bluetoothctl show`,
   `btmon`, and the phone's ability to discover/pair with a real peer.

PASS requires a clean boot with `hci0` automatically present, no firmware or
HCI frame errors, a working `bluetoothctl show`, and a successful scan or
pairing test. Until then the task remains **DIAGNOSTIC / INCONCLUSIVE**.
