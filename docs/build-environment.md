# Build environment

This file records the host environment used for the clean OnePlus 3 mainline rebuild.

| Item | Value |
| --- | --- |
| Host OS | Ubuntu 26.04 LTS (Resolute) |
| Host architecture | x86_64 |
| Target architecture | arm64 |
| Kernel baseline | Linux v7.2 pristine upstream (not yet fetched) |
| Kernel remote | `https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git` |
| Legacy reference kernel | pmOS MSM8996 Linux v6.3.1, `9895e7e38b829a810b9f75d1f98c9e4349ae454a` |
| Cross compiler command | `aarch64-linux-gnu-gcc-11` |
| Cross compiler version | GCC 11.5.0 |
| `ARCH` | `arm64` |
| `CROSS_COMPILE` | `aarch64-linux-gnu-` |
| Legacy build host | Ubuntu 22.04.5 LTS under WSL2 |
| Legacy ARM64 compiler | GCC 11.4.0 |
| Legacy environment manifest | `docs/environment/wsl-environment-manifest-20260826.tar.gz` |
| Legacy manifest SHA256 | `d574d51bc89fabe0dae523b4811cb0c5bdec54fa365aa7838df9e535867c33b6` |

The current compiler is intentionally GCC 11 to stay close to the legacy environment.
The host distribution and package versions are not expected to be bit-for-bit identical.

Linux 7.2 pristine upstream is the only default bring-up baseline. The pmOS
MSM8996 6.3.1 tree is legacy evidence only and must not be built or patched
unless an explicitly marked legacy task requests it.
