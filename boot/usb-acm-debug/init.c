// SPDX-License-Identifier: MIT
/* Minimal USB CDC ACM diagnostic console for OnePlus 3 early bring-up. */
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define GADGET "/sys/kernel/config/usb_gadget/op3debug"

static void put(const char *path, const char *value)
{
	int fd = open(path, O_WRONLY | O_CLOEXEC);

	if (fd < 0)
		return;
	if (write(fd, value, strlen(value)) < 0) {
		close(fd);
		return;
	}
	close(fd);
}

static void setup_gadget(void)
{
	char udc[128] = {};
	char path[256];
	DIR *dir;
	struct dirent *entry;

	mkdir("/dev", 0755);
	mkdir("/proc", 0555);
	mkdir("/sys", 0555);
	mount("devtmpfs", "/dev", "devtmpfs", 0, "mode=0755");
	mount("proc", "/proc", "proc", 0, NULL);
	mount("sysfs", "/sys", "sysfs", 0, NULL);
	mkdir("/sys/kernel/config", 0755);
	mount("configfs", "/sys/kernel/config", "configfs", 0, NULL);

	mkdir("/sys/kernel/config/usb_gadget", 0755);
	mkdir(GADGET, 0755);
	put(GADGET "/idVendor", "0x1d6b");
	put(GADGET "/idProduct", "0x0104");
	put(GADGET "/bcdDevice", "0x0100");
	put(GADGET "/bcdUSB", "0x0200");
	mkdir(GADGET "/strings", 0755);
	mkdir(GADGET "/strings/0x409", 0755);
	put(GADGET "/strings/0x409/serialnumber", "op3-linux72-debug");
	put(GADGET "/strings/0x409/manufacturer", "OnePlus 3 Linux");
	put(GADGET "/strings/0x409/product", "Linux 7.2 debug console");
	mkdir(GADGET "/configs", 0755);
	mkdir(GADGET "/configs/c.1", 0755);
	mkdir(GADGET "/configs/c.1/strings", 0755);
	mkdir(GADGET "/configs/c.1/strings/0x409", 0755);
	put(GADGET "/configs/c.1/strings/0x409/configuration", "CDC ACM");
	mkdir(GADGET "/functions", 0755);
	mkdir(GADGET "/functions/acm.usb0", 0755);
	if (symlink("../../functions/acm.usb0",
		    GADGET "/configs/c.1/acm.usb0") < 0)
		return;

	dir = opendir("/sys/class/udc");
	if (!dir)
		return;
	entry = readdir(dir);
	while (entry) {
		if (entry->d_name[0] != '.') {
			strncpy(udc, entry->d_name, sizeof(udc) - 1);
			break;
		}
		entry = readdir(dir);
	}
	closedir(dir);
	if (!udc[0])
		return;
	snprintf(path, sizeof(path), "%s/UDC", GADGET);
	put(path, udc);
}

static void stream_kmsg(int serial)
{
	char buf[512];
	int kmsg = open("/dev/kmsg", O_RDONLY | O_CLOEXEC);

	if (kmsg < 0)
		return;
	for (;;) {
		ssize_t count = read(kmsg, buf, sizeof(buf));

		if (count > 0 && write(serial, buf, count) < 0)
			break;
	}
	close(kmsg);
}

int main(void)
{
	char line[128];
	int serial;
	int i;

	setup_gadget();
	for (i = 0; i < 30; i++) {
		serial = open("/dev/ttyGS0", O_RDWR | O_CLOEXEC);
		if (serial >= 0)
			break;
		sleep(1);
	}
	if (i == 30)
		for (;;)
			pause();

	dprintf(serial, "op3 Linux 7.2 USB ACM diagnostic console\r\n");
	dprintf(serial, "commands: help, cmdline, udc\r\n");
	if (fork() == 0) {
		stream_kmsg(serial);
		return 0;
	}
	for (;;) {
		ssize_t count = read(serial, line, sizeof(line) - 1);

		if (count <= 0)
			continue;
		line[count] = '\0';
		if (!strncmp(line, "help", 4))
			dprintf(serial, "help cmdline udc\r\n");
		else if (!strncmp(line, "cmdline", 7)) {
			int fd = open("/proc/cmdline", O_RDONLY | O_CLOEXEC);
			if (fd >= 0) {
				count = read(fd, line, sizeof(line) - 1);
				if (count > 0) {
					line[count] = '\0';
					dprintf(serial, "%s\r\n", line);
				}
				close(fd);
			}
		} else if (!strncmp(line, "udc", 3)) {
			dprintf(serial, "configfs gadget path: " GADGET "\r\n");
		} else
			dprintf(serial, "unknown command\r\n");
	}
}
