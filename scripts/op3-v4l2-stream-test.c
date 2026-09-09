// SPDX-License-Identifier: GPL-2.0-only
/*
 * Small OnePlus 3 CAMSS smoke test.
 *
 * It intentionally uses only the V4L2 video-node UAPI so it can be uploaded
 * to the recovery rootfs without rebuilding Buildroot or depending on
 * media-ctl/v4l2-ctl.
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <linux/v4l2-mediabus.h>
#include <linux/v4l2-subdev.h>
#include <linux/videodev2.h>

struct mapped_buffer {
	void *address;
	size_t length;
};

static int xioctl(int fd, unsigned long request, void *arg)
{
	int ret;

	do
		ret = ioctl(fd, request, arg);
	while (ret < 0 && errno == EINTR);

	return ret;
}

static void fourcc(char out[5], uint32_t value)
{
	out[0] = value & 0xff;
	out[1] = (value >> 8) & 0xff;
	out[2] = (value >> 16) & 0xff;
	out[3] = (value >> 24) & 0xff;
	out[4] = '\0';
}

static int inspect_node(const char *path)
{
	struct v4l2_capability cap = { 0 };
	struct v4l2_format fmt = { .type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE };
	char pixfmt[5] = { 0 };
	int fd;

	fd = open(path, O_RDWR | O_NONBLOCK);
	if (fd < 0)
		return -errno;

	if (xioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
		int ret = -errno;

		close(fd);
		return ret;
	}

	fourcc(pixfmt, fmt.fmt.pix_mp.pixelformat);
	printf("%s: driver=%s card=%s caps=0x%08x device_caps=0x%08x\n",
	       path, cap.driver, cap.card, cap.capabilities,
	       cap.device_caps);

	if (xioctl(fd, VIDIOC_G_FMT, &fmt) < 0) {
		printf("%s: G_FMT failed: %s\n", path, strerror(errno));
		close(fd);
		return 0;
	}

	fourcc(pixfmt, fmt.fmt.pix_mp.pixelformat);
	printf("%s: mplane %ux%u pixelformat=%s planes=%u bpl=%u size=%u\n",
	       path, fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height, pixfmt,
	       fmt.fmt.pix_mp.num_planes,
	       fmt.fmt.pix_mp.plane_fmt[0].bytesperline,
	       fmt.fmt.pix_mp.plane_fmt[0].sizeimage);

	close(fd);
	return 0;
}

static int configure_subdev(const char *path, const char *name)
{
	struct v4l2_subdev_format fmt;
	char code[5];
	int fd;
	int configured = 0;
	unsigned int pad;

	fd = open(path, O_RDWR);
	if (fd < 0)
		return -errno;

	printf("%s (%s):\n", path, name);
	for (pad = 0; pad < 8; pad++) {
		memset(&fmt, 0, sizeof(fmt));
		fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
		fmt.pad = pad;
		if (xioctl(fd, VIDIOC_SUBDEV_G_FMT, &fmt) < 0)
			continue;

		fourcc(code, fmt.format.code);
		printf("  pad=%u before=%ux%u code=0x%08x(%s)\n", pad,
		       fmt.format.width, fmt.format.height, fmt.format.code, code);
		fmt.format.width = 1476;
		fmt.format.height = 834;
		fmt.format.code = MEDIA_BUS_FMT_SRGGB10_1X10;
		fmt.format.field = V4L2_FIELD_NONE;
		fmt.format.colorspace = V4L2_COLORSPACE_RAW;
		fmt.format.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
		fmt.format.quantization = V4L2_QUANTIZATION_FULL_RANGE;
		fmt.format.xfer_func = V4L2_XFER_FUNC_NONE;
		if (xioctl(fd, VIDIOC_SUBDEV_S_FMT, &fmt) < 0) {
			printf("  pad=%u S_FMT failed: %s\n", pad, strerror(errno));
			continue;
		}

		fourcc(code, fmt.format.code);
		printf("  pad=%u after=%ux%u code=0x%08x(%s)\n", pad,
		       fmt.format.width, fmt.format.height, fmt.format.code, code);
		configured++;
	}

	close(fd);
	return configured;
}

static int configure_camera_subdevs(void)
{
	char path[32];
	char name_path[96];
	char name[128];
	FILE *name_file;
	int configured = 0;
	int fd;
	int i;

	for (i = 0; i < 32; i++) {
		snprintf(path, sizeof(path), "/dev/v4l-subdev%d", i);
		if (access(path, R_OK | W_OK) != 0)
			continue;

		snprintf(name_path, sizeof(name_path),
			 "/sys/class/video4linux/v4l-subdev%d/name", i);
		name_file = fopen(name_path, "r");
		if (!name_file)
			continue;
		if (!fgets(name, sizeof(name), name_file)) {
			fclose(name_file);
			continue;
		}
		fclose(name_file);
		name[strcspn(name, "\r\n")] = '\0';

		if (!strstr(name, "msm_csiphy0") && !strstr(name, "msm_csid0") &&
		    !strstr(name, "msm_ispif0") && !strstr(name, "msm_vfe0_rdi0") &&
		    !strstr(name, "imx298"))
			continue;

		fd = configure_subdev(path, name);
		if (fd > 0)
			configured += fd;
	}

	return configured;
}

static int list_nodes(void)
{
	char path[32];
	int found = 0;
	int i;

	for (i = 0; i < 16; i++) {
		snprintf(path, sizeof(path), "/dev/video%d", i);
		if (access(path, R_OK | W_OK) != 0)
			continue;
		inspect_node(path);
		found++;
	}

	return found ? 0 : ENODEV;
}

static int stream_node(const char *path, const char *output)
{
	struct mapped_buffer buffers[4] = { 0 };
	struct v4l2_requestbuffers req = {
		.count = 4,
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.memory = V4L2_MEMORY_MMAP,
	};
	struct v4l2_format fmt = {
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
	};
	struct v4l2_buffer buf = {
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.memory = V4L2_MEMORY_MMAP,
	};
	struct v4l2_plane planes[VIDEO_MAX_PLANES] = { 0 };
	struct pollfd pfd;
	unsigned int i;
	unsigned int buffer_count;
	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	int fd = -1;
	int ret = 0;
	int output_fd = -1;

	fd = open(path, O_RDWR | O_NONBLOCK);
	if (fd < 0)
		return -errno;

	if (configure_camera_subdevs() <= 0) {
		ret = ENODEV;
		fprintf(stderr, "no camera subdevice format was configured\n");
		goto out;
	}

	fmt.fmt.pix_mp.width = 1476;
	fmt.fmt.pix_mp.height = 834;
	fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_SRGGB10P;
	fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
	fmt.fmt.pix_mp.num_planes = 1;

	if (xioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
		ret = -errno;
		fprintf(stderr, "%s: S_FMT failed: %s\n", path,
			strerror(errno));
		goto out;
	}

	printf("%s: S_FMT -> %ux%u pixelformat=0x%08x planes=%u bpl=%u size=%u\n",
	       path, fmt.fmt.pix_mp.width, fmt.fmt.pix_mp.height,
	       fmt.fmt.pix_mp.pixelformat, fmt.fmt.pix_mp.num_planes,
	       fmt.fmt.pix_mp.plane_fmt[0].bytesperline,
	       fmt.fmt.pix_mp.plane_fmt[0].sizeimage);

	if (xioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
		ret = -errno;
		fprintf(stderr, "%s: REQBUFS failed: %s\n", path,
			strerror(errno));
		goto out;
	}
	if (req.count < 2) {
		ret = ENOBUFS;
		fprintf(stderr, "%s: only %u MMAP buffers\n", path, req.count);
		goto out;
	}

	buffer_count = req.count > 4 ? 4 : req.count;
	for (i = 0; i < buffer_count; i++) {
		struct v4l2_buffer query = {
			.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
			.memory = V4L2_MEMORY_MMAP,
			.index = i,
			.length = VIDEO_MAX_PLANES,
			.m.planes = planes,
		};

		if (xioctl(fd, VIDIOC_QUERYBUF, &query) < 0) {
			ret = -errno;
			fprintf(stderr, "%s: QUERYBUF %u failed: %s\n", path, i,
				strerror(errno));
			goto out_unmap;
		}

		if (query.length != 1) {
			ret = EINVAL;
			fprintf(stderr, "%s: expected one plane, got %u\n", path,
				query.length);
			goto out_unmap;
		}

		buffers[i].length = query.m.planes[0].length;
		buffers[i].address = mmap(NULL, buffers[i].length,
					 PROT_READ | PROT_WRITE, MAP_SHARED, fd,
					 query.m.planes[0].m.mem_offset);
		if (buffers[i].address == MAP_FAILED) {
			buffers[i].address = NULL;
			ret = errno;
			fprintf(stderr, "%s: mmap %u failed: %s\n", path, i,
				strerror(errno));
			goto out_unmap;
		}
	}

	for (i = 0; i < buffer_count; i++) {
		struct v4l2_buffer queue = {
			.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
			.memory = V4L2_MEMORY_MMAP,
			.index = i,
			.length = 1,
			.m.planes = planes,
		};

		if (xioctl(fd, VIDIOC_QBUF, &queue) < 0) {
			ret = -errno;
			fprintf(stderr, "%s: QBUF %u failed: %s\n", path, i,
				strerror(errno));
			goto out_unmap;
		}
	}

	if (xioctl(fd, VIDIOC_STREAMON, &type) < 0) {
		ret = -errno;
		fprintf(stderr, "%s: STREAMON failed: %s\n", path,
			strerror(errno));
		goto out_unmap;
	}

	pfd.fd = fd;
	pfd.events = POLLIN;
	if (poll(&pfd, 1, 3000) <= 0) {
		ret = errno ? errno : ETIMEDOUT;
		fprintf(stderr, "%s: frame wait failed: %s\n", path,
			strerror(ret));
		goto out_streamoff;
	}

	memset(&buf, 0, sizeof(buf));
	memset(planes, 0, sizeof(planes));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.length = 1;
	buf.m.planes = planes;
	if (xioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
		ret = -errno;
		fprintf(stderr, "%s: DQBUF failed: %s\n", path,
			strerror(errno));
		goto out_streamoff;
	}

	output_fd = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (output_fd < 0) {
		ret = -errno;
		fprintf(stderr, "%s: output open failed: %s\n", output,
			strerror(errno));
		goto out_streamoff;
	}
	if (write(output_fd, buffers[buf.index].address,
		  planes[0].bytesused) != (ssize_t)planes[0].bytesused) {
		ret = errno ? errno : EIO;
		fprintf(stderr, "%s: output write failed: %s\n", output,
			strerror(ret));
	}
	close(output_fd);
	output_fd = -1;
	printf("%s: captured buffer=%u bytes=%u to %s\n", path, buf.index,
	       planes[0].bytesused, output);

out_streamoff:
	if (xioctl(fd, VIDIOC_STREAMOFF, &type) < 0 && !ret)
		ret = -errno;
out_unmap:
	for (i = 0; i < buffer_count; i++)
		if (buffers[i].address)
			munmap(buffers[i].address, buffers[i].length);
out:
	if (output_fd >= 0)
		close(output_fd);
	if (fd >= 0)
		close(fd);
	return ret;
}

int main(int argc, char **argv)
{
	int ret;

	if (argc == 2 && !strcmp(argv[1], "list"))
		return list_nodes();
	if (argc != 3) {
		fprintf(stderr, "usage: %s list | %s /dev/videoX output.raw\n",
			argv[0], argv[0]);
		return EINVAL;
	}

	ret = stream_node(argv[1], argv[2]);
	return ret ? 1 : 0;
}
