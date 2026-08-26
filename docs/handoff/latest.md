# Latest handoff

```text
PROJECT MODE: CLEAN REBUILD
TARGET KERNEL: Linux 7.2 pristine upstream
LEGACY KERNEL: Linux 6.3.1 pmOS-derived
DO NOT BUILD LEGACY unless explicitly requested
HOST TARGET: Ubuntu 26.04 LTS x86_64
DEVICE: OnePlus 3 / MSM8996
```

## Current state

- Host tools and GCC 11 ARM64/ARM32 cross compilers have been verified.
- `BASELINE.env` is the authoritative machine-readable baseline.
- GitHub repository: `https://github.com/iamgreatwk/op3`.
- Initial governance commit: `b8ec809f5f97f9db9de5deaa3a4f75a200aa0659`.
- A pmOS 6.3.1 checkout exists only as ignored local legacy evidence under
  `source/linux-pmos-msm8996-6.3.1/`.
- No kernel image has been built, packed, flashed, or booted in this project.
- The user performs all kernel and other large-project compilation.

## Next action

Review `scripts/fetch-kernel.sh` and `scripts/build-kernel.sh`. The user may
then fetch Linux 7.2 and run the printed build command. Do not bring any legacy
DTS or pmOS patch into that first build.
