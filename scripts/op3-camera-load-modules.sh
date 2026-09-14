#!/bin/sh
# Load the complete temporary OP3 camera module chain in dependency order.
#
# This script is intended to run on the recovery device as root.  It uses
# absolute-path insmod so a stale or missing modules.dep cannot hide an
# omitted module.  The module directory is supplied explicitly because the
# initramfs and /newroot are separate filesystems.

set -eu

if [ "$#" -ne 1 ]; then
	printf 'usage: %s MODULE_ROOT\n' "$0" >&2
	exit 2
fi

module_root=$1
[ -d "$module_root" ] || {
	printf 'missing module root: %s\n' "$module_root" >&2
	exit 1
}

find_module()
{
	name=$1
	find "$module_root" -type f -name "$name.ko" 2>/dev/null | head -n 1
}

load_module()
{
	name=$1
	path=$(find_module "$name")
	[ -n "$path" ] || {
		printf 'missing required module: %s.ko\n' "$name" >&2
		exit 1
	}

	printf 'module %s path=%s sha256=' "$name" "$path"
	sha256sum "$path" | cut -d ' ' -f 1
	if [ -x /sbin/insmod ]; then
		/sbin/insmod "$path"
	else
		/bin/busybox insmod "$path"
	fi

	sys_name=$(printf '%s' "$name" | tr '-' '_')
	[ -d "/sys/module/$sys_name" ] || {
		printf 'module %s did not appear in /sys/module\n' "$name" >&2
		exit 1
	}
}

# Keep this order synchronized with docs/device-shell-compat.md.
load_module mc
load_module videodev
load_module v4l2-async
load_module v4l2-fwnode
load_module videobuf2-common
load_module videobuf2-memops
load_module videobuf2-dma-sg
load_module videobuf2-v4l2
load_module i2c-qcom-cci
load_module qcom-camss
load_module bu63165gwl
load_module imx298

printf 'camera modules loaded; verify media and video nodes now\n'
