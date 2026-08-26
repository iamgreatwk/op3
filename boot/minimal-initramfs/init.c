// SPDX-License-Identifier: MIT
/*
 * Minimal PID 1 for the OnePlus 3 Linux 7.2 boot A/B test.
 *
 * This deliberately has no root-device handling and does not interpret any
 * legacy pmOS command-line parameters.  Its sole purpose is to keep a booted
 * kernel alive so that an initramfs failure can be separated from early kernel
 * or device-tree failures.
 */
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

static void kmsg(const char *message)
{
	int fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);

	if (fd >= 0) {
		if (write(fd, message, __builtin_strlen(message)) < 0) {
			close(fd);
			return;
		}
		close(fd);
	}
}

int main(void)
{
	mkdir("/dev", 0755);
	mkdir("/proc", 0555);
	mkdir("/sys", 0555);

	/* Ignore EBUSY when the kernel has already mounted devtmpfs. */
	mount("devtmpfs", "/dev", "devtmpfs", 0, "mode=0755");
	mount("proc", "/proc", "proc", 0, NULL);
	mount("sysfs", "/sys", "sysfs", 0, NULL);

	kmsg("<6>op3-minimal-init: PID 1 alive; no pmOS root UUID used\n");

	/* PID 1 must not exit, otherwise the kernel panics or reboots. */
	for (;;)
		pause();
}
