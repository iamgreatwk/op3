# Standalone kbuild wrapper for the probe-only IMX298 module.
# The build command supplies imx298.c through a symlink in a temporary
# external-module directory and resolves symbols from the known-good kernel's
# Module.symvers.
obj-m := imx298.o
