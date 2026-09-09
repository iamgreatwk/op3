// SPDX-License-Identifier: GPL-2.0-only
/*
 * Minimal OP3 V4L2 lens test helper.
 *
 * Keep the sub-device open while applying a position sequence so the VCM
 * runtime-power rail remains enabled for the whole sweep.  This is a motor
 * test, not a closed-loop autofocus algorithm.
 */

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int set_focus(int fd, int position)
{
	struct v4l2_control control = {
		.id = V4L2_CID_FOCUS_ABSOLUTE,
		.value = position,
	};

	if (ioctl(fd, VIDIOC_S_CTRL, &control) < 0) {
		fprintf(stderr, "VIDIOC_S_CTRL focus=%d: %s\n", position,
			errno ? strerror(errno) : "unknown error");
		return -1;
	}

	if (ioctl(fd, VIDIOC_G_CTRL, &control) < 0) {
		fprintf(stderr, "VIDIOC_G_CTRL focus=%d: %s\n", position,
			errno ? strerror(errno) : "unknown error");
		return -1;
	}

	printf("focus requested=%d applied=%d\n", position, control.value);
	return 0;
}

int main(int argc, char **argv)
{
	static const int default_positions[] = { 0, 256, 512, 768, 1023, 512 };
	const int *positions = default_positions;
	int position_count = (int)(sizeof(default_positions) /
					    sizeof(default_positions[0]));
	int fd;
	int i;

	if (argc < 2) {
		fprintf(stderr,
			"usage: %s /dev/v4l-subdevX [position ...]\n",
			argv[0]);
		return 2;
	}

	if (argc > 2) {
		position_count = argc - 2;
		positions = NULL;
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "open %s: %s\n", argv[1], strerror(errno));
		return 1;
	}

	for (i = 0; i < position_count; i++) {
		char *end;
		long value;

		if (positions) {
			value = positions[i];
		} else {
			value = strtol(argv[i + 2], &end, 10);
			if (*argv[i + 2] == '\0' || *end != '\0' ||
			    value < 0 || value > 1023) {
				fprintf(stderr, "invalid focus position: %s\n",
					argv[i + 2]);
				close(fd);
				return 2;
			}
		}

		if (set_focus(fd, (int)value)) {
			close(fd);
			return 1;
		}
		usleep(150000);
	}

	close(fd);
	return 0;
}
