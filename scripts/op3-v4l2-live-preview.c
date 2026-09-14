// SPDX-License-Identifier: GPL-2.0-only
/*
 * Temporary OP3 IMX298 live preview.
 *
 * Reuse the validated media-graph and V4L2 setup from the AF diagnostic
 * helper.  The recovery process releases card0 through its existing browser
 * handoff while this program owns a direct DRM dumb buffer.  This is a
 * preview/alignment tool, not an autofocus implementation.
 */

#define main op3_af_diagnostic_main
#include "op3-v4l2-af-stream-test.c"
#undef main

#include "../recovery/recovery_drm.h"

#include <linux/input.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define PREVIEW_FPS_LIMIT_US 66666
#define PREVIEW_MARGIN 30
#define PREVIEW_STATUS_H 110
#define PREVIEW_MAX_INPUTS 32
#define PREVIEW_FRAME_TIMEOUT_US 3000000
#define PREVIEW_KEY_BITS ((KEY_MAX + 8U) / 8U)

struct preview_input {
	int fd;
	char path[64];
	char name[128];
};

enum preview_stop_reason {
	PREVIEW_STOP_NONE,
	PREVIEW_STOP_POWER,
	PREVIEW_STOP_BACK,
	PREVIEW_STOP_SIGNAL,
	PREVIEW_STOP_FRAME_TIMEOUT,
	PREVIEW_STOP_VIDEO_POLL_ERROR,
	PREVIEW_STOP_DQBUF_ERROR,
	PREVIEW_STOP_DISPLAY_ERROR,
	PREVIEW_STOP_NO_FRAME,
};

static volatile sig_atomic_t preview_stop;
static volatile sig_atomic_t preview_signal_number;

static void preview_signal(int signal_number)
{
	preview_signal_number = signal_number;
	preview_stop = 1;
}

static const char *preview_stop_reason_name(enum preview_stop_reason reason)
{
	switch (reason) {
	case PREVIEW_STOP_POWER:
		return "power-key";
	case PREVIEW_STOP_BACK:
		return "back-key";
	case PREVIEW_STOP_SIGNAL:
		return "signal";
	case PREVIEW_STOP_FRAME_TIMEOUT:
		return "frame-timeout";
	case PREVIEW_STOP_VIDEO_POLL_ERROR:
		return "video-poll-error";
	case PREVIEW_STOP_DQBUF_ERROR:
		return "dqbuf-error";
	case PREVIEW_STOP_DISPLAY_ERROR:
		return "display-error";
	case PREVIEW_STOP_NO_FRAME:
		return "no-frame";
	default:
		return "none";
	}
}

static uint16_t raw10_pixel(const uint8_t *row, unsigned int x)
{
	const uint8_t *group = row + (x / 4U) * 5U;
	uint8_t low = group[4];

	switch (x & 3U) {
	case 0:
		return ((uint16_t)group[0] << 2) | (low & 0x03U);
	case 1:
		return ((uint16_t)group[1] << 2) | ((low >> 2) & 0x03U);
	case 2:
		return ((uint16_t)group[2] << 2) | ((low >> 4) & 0x03U);
	default:
		return ((uint16_t)group[3] << 2) | ((low >> 6) & 0x03U);
	}
}

static void preview_fill(struct recovery_drm_display *display,
			 unsigned int x, unsigned int y, unsigned int width,
			 unsigned int height, uint32_t colour)
{
	uint32_t *pixels = display->pixels;
	unsigned int stride = display->pitch / sizeof(uint32_t);

	if (x >= display->width || y >= display->height)
		return;
	if (x + width > display->width)
		width = display->width - x;
	if (y + height > display->height)
		height = display->height - y;
	for (unsigned int row = 0; row < height; row++) {
		uint32_t *line = pixels + (y + row) * stride + x;
		for (unsigned int col = 0; col < width; col++)
			line[col] = colour;
	}
}

static void preview_rect(struct recovery_drm_display *display,
			 unsigned int x, unsigned int y, unsigned int width,
			 unsigned int height, unsigned int thickness, uint32_t colour)
{
	preview_fill(display, x, y, width, thickness, colour);
	preview_fill(display, x, y + height - thickness, width, thickness, colour);
	preview_fill(display, x, y, thickness, height, colour);
	preview_fill(display, x + width - thickness, y, thickness, height, colour);
}

static void preview_scale_bar(struct recovery_drm_display *display,
			      unsigned int x, unsigned int y, unsigned int width,
			      int focus)
{
	unsigned int marker = x + (width * (unsigned int)focus) / 1023U;

	preview_fill(display, x, y, width, 8, 0xffb0b0b0U);
	for (unsigned int i = 0; i <= 10; i++) {
		unsigned int tick = x + (width * i) / 10U;
		preview_fill(display, tick, y - 12, 3, 32, 0xfff0f0f0U);
	}
	preview_fill(display, marker > 5 ? marker - 5 : x, y - 28, 11, 48,
		     0xff00ff00U);
}

static int preview_render(struct recovery_drm_display *display,
			  const void *data, unsigned int bytesperline,
			  unsigned int width, unsigned int height, int focus)
{
	const uint8_t *source = data;
	unsigned int output_width;
	unsigned int output_height;
	unsigned int output_x;
	unsigned int output_y;
	unsigned int output_stride = display->pitch / sizeof(uint32_t);
	uint32_t *pixels = display->pixels;

	/* Keep the landscape camera image centred on the portrait OP3 panel. */
	output_width = display->width - PREVIEW_MARGIN * 2U;
	output_height = (output_width * height) / width;
	if (output_height > display->height - PREVIEW_STATUS_H - PREVIEW_MARGIN * 2U) {
		output_height = display->height - PREVIEW_STATUS_H - PREVIEW_MARGIN * 2U;
		output_width = (output_height * width) / height;
	}
	output_x = (display->width - output_width) / 2U;
	output_y = (display->height - PREVIEW_STATUS_H - output_height) / 2U;

	preview_fill(display, 0, 0, display->width, display->height, 0xff101010U);
	for (unsigned int y = 0; y < output_height; y++) {
		unsigned int source_y = (y * height) / output_height;
		const uint8_t *row = source + source_y * bytesperline;
		uint32_t *destination = pixels + (output_y + y) * output_stride + output_x;

		for (unsigned int x = 0; x < output_width; x++) {
			unsigned int source_x = (x * width) / output_width;
			unsigned int grey = (raw10_pixel(row, source_x) * 255U) / 1023U;

			destination[x] = 0xff000000U | (grey << 16) |
				(grey << 8) | grey;
		}
	}

	preview_rect(display, output_x, output_y, output_width, output_height,
		     4, 0xffffb000U);
	/* Red centre crosshair: align the optical target to this intersection. */
	preview_fill(display, display->width / 2U - 2U,
		     output_y, 5, output_height, 0xffff2020U);
	preview_fill(display, output_x, output_y + output_height / 2U - 2U,
		     output_width, 5, 0xffff2020U);
	preview_scale_bar(display, PREVIEW_MARGIN * 2U,
			  display->height - PREVIEW_STATUS_H + 45U,
			  display->width - PREVIEW_MARGIN * 4U, focus);

	if (msync(display->pixels, display->map_size, MS_SYNC) < 0)
		return -errno;
	__sync_synchronize();
	return recovery_drm_present(display);
}

static int preview_input_has_code(int fd, unsigned int code)
{
	unsigned char key_bits[PREVIEW_KEY_BITS];

	if (code > KEY_MAX)
		return 0;
	memset(key_bits, 0, sizeof(key_bits));
	if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0)
		return 0;
	return !!(key_bits[code / 8U] & (1U << (code % 8U)));
}

static int preview_open_inputs(struct preview_input *inputs,
			       unsigned int *count)
{
	*count = 0;
	for (unsigned int i = 0; i < PREVIEW_MAX_INPUTS; i++) {
		char path[64];
		char name[128] = "unknown";
		int fd;

		snprintf(path, sizeof(path), "/dev/input/event%u", i);
		fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (fd < 0)
			continue;
		if (!preview_input_has_code(fd, KEY_POWER) &&
		    !preview_input_has_code(fd, KEY_BACK) &&
		    !preview_input_has_code(fd, KEY_VOLUMEUP) &&
		    !preview_input_has_code(fd, KEY_VOLUMEDOWN)) {
			close(fd);
			continue;
		}
		(void)ioctl(fd, EVIOCGNAME(sizeof(name)), name);
		inputs[*count].fd = fd;
		snprintf(inputs[*count].path, sizeof(inputs[*count].path),
			 "%s", path);
		snprintf(inputs[*count].name, sizeof(inputs[*count].name),
			 "%s", name);
		printf("preview input path=%s name=%s\n", inputs[*count].path,
		       inputs[*count].name);
		(*count)++;
	}
	return 0;
}

static void preview_close_inputs(const struct preview_input *inputs,
				 unsigned int count)
{
	for (unsigned int i = 0; i < count; i++)
		close(inputs[i].fd);
}

static int preview_focus_set(struct test_context *context, int *focus,
			     int delta)
{
	struct v4l2_control control = {
		.id = V4L2_CID_FOCUS_ABSOLUTE,
		.value = *focus + delta,
	};
	int ret;

	if (control.value < 0)
		control.value = 0;
	if (control.value > 1023)
		control.value = 1023;
	ret = xioctl(context->lens_fd, VIDIOC_S_CTRL, &control);
	if (ret < 0) {
		fprintf(stderr, "preview focus=%d failed errno=%d(%s)\n",
			control.value, errno, strerror(errno));
		return -errno;
	}
	*focus = control.value;
	printf("preview focus=%d ioctl_rc=0\n", *focus);
	return 0;
}

static int preview_handle_inputs(const struct preview_input *inputs,
				 unsigned int count, struct test_context *context,
				 int *focus, enum preview_stop_reason *reason)
{
	struct input_event event;

	for (unsigned int i = 0; i < count; i++) {
		while (read(inputs[i].fd, &event, sizeof(event)) == sizeof(event)) {
			if (event.type != EV_KEY || event.value != 1)
				continue;
			if (event.code == KEY_POWER || event.code == KEY_BACK) {
				*reason = event.code == KEY_POWER ? PREVIEW_STOP_POWER :
					PREVIEW_STOP_BACK;
				fprintf(stderr, "preview input exit path=%s name=%s code=%u\n",
					inputs[i].path, inputs[i].name, event.code);
				return 1;
			}
			if (context->lens_fd >= 0 && event.code == KEY_VOLUMEUP)
				(void)preview_focus_set(context, focus, 16);
			else if (context->lens_fd >= 0 && event.code == KEY_VOLUMEDOWN)
				(void)preview_focus_set(context, focus, -16);
		}
	}
	return 0;
}

int main(void)
{
	struct test_context context;
	struct recovery_drm_display display;
	struct sigaction signal_action;
	struct pollfd pollfds[1 + PREVIEW_MAX_INPUTS];
	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	struct v4l2_buffer buffer;
	struct preview_input inputs[PREVIEW_MAX_INPUTS];
	unsigned int input_count = 0;
	unsigned int good_frames = 0;
	int display_active = 0;
	int focus = DEFAULT_FOCUS_POSITION;
	int stream_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	enum preview_stop_reason stop_reason = PREVIEW_STOP_NONE;
	int ret = 0;
	long long last_present = 0;
	long long last_frame;

	memset(&context, 0, sizeof(context));
	context.media_fd = -1;
	context.video_fd = -1;
	context.lens_fd = -1;
	context.options.exposure = DEFAULT_EXPOSURE;
	context.options.gain = DEFAULT_GAIN;
	memset(&signal_action, 0, sizeof(signal_action));
	signal_action.sa_handler = preview_signal;
	sigemptyset(&signal_action.sa_mask);
	sigaction(SIGINT, &signal_action, NULL);
	sigaction(SIGTERM, &signal_action, NULL);
	pthread_mutex_init(&context.lock, NULL);
	pthread_cond_init(&context.condition, NULL);

	ret = select_media_graph(&context);
	if (ret)
		goto out_sync;
	ret = find_named_subdev("imx298", context.sensor_path,
				       sizeof(context.sensor_path));
	if (ret)
		goto out_sync;
	ret = find_named_subdev("bu63165gwl", context.lens_path,
				       sizeof(context.lens_path));
	if (ret)
		goto out_sync;
	ret = find_rdi_video(context.video_path, sizeof(context.video_path));
	if (ret)
		goto out_sync;
	printf("preview discover media=%s sensor=%s lens=%s video=%s\n",
	       context.media_path, context.sensor_path, context.lens_path,
	       context.video_path);
	context.video_fd = open(context.video_path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
	if (context.video_fd < 0) {
		ret = -errno;
		goto out_sync;
	}
	ret = enable_media_path(&context);
	if (ret)
		goto out_video;
	if (configure_pipeline_subdevs(&context) <= 0) {
		ret = -ENODEV;
		goto out_video;
	}
	ret = set_sensor_controls(&context);
	if (ret)
		goto out_video;

	memset(&display, 0, sizeof(display));
	display.fd = -1;
	if (recovery_drm_open(&display) < 0) {
		ret = -ENODEV;
		goto out_video;
	}
	/* Keep the DRM buffer allocated, but defer SETCRTC until the first camera
	 * buffer is ready.  This isolates KMS activation from the VFE startup path
	 * and avoids presenting a misleading gray frame before capture works. */
	ret = prepare_video(&context);
	if (ret)
		goto out_display;
	preview_open_inputs(inputs, &input_count);
	printf("preview ready %ux%u; volume +/- focus step=16; power/back exits\n",
	       context.width, context.height);
	last_frame = monotonic_us();

	while (!preview_stop) {
		int poll_count;
		unsigned int index;
		long long now = monotonic_us();

		if (now - last_frame >= PREVIEW_FRAME_TIMEOUT_US) {
			fprintf(stderr, "preview frame timeout after %lld us\n",
				now - last_frame);
			stop_reason = PREVIEW_STOP_FRAME_TIMEOUT;
			ret = -ETIMEDOUT;
			break;
		}

		memset(pollfds, 0, sizeof(pollfds));
		pollfds[0].fd = context.video_fd;
		pollfds[0].events = POLLIN;
		for (unsigned int i = 0; i < input_count; i++) {
			pollfds[1 + i].fd = inputs[i].fd;
			pollfds[1 + i].events = POLLIN;
		}
		poll_count = poll(pollfds, 1 + input_count, 1000);
		if (poll_count < 0) {
			if (errno == EINTR)
				continue;
			fprintf(stderr, "preview poll failed errno=%d(%s)\n",
				errno, strerror(errno));
			stop_reason = PREVIEW_STOP_VIDEO_POLL_ERROR;
			ret = -errno;
			break;
		}
		if (!poll_count)
			continue;
		if (preview_handle_inputs(inputs, input_count, &context, &focus,
					  &stop_reason) > 0)
			break;
		if (pollfds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
			fprintf(stderr, "preview video poll revents=0x%x\n",
				pollfds[0].revents);
			stop_reason = PREVIEW_STOP_VIDEO_POLL_ERROR;
			ret = -EIO;
			break;
		}
		if (!(pollfds[0].revents & POLLIN))
			continue;

		memset(&buffer, 0, sizeof(buffer));
		memset(planes, 0, sizeof(planes));
		buffer.type = stream_type;
		buffer.memory = V4L2_MEMORY_MMAP;
		buffer.length = 1;
		buffer.m.planes = planes;
		if (xioctl(context.video_fd, VIDIOC_DQBUF, &buffer) < 0) {
			if (errno == EAGAIN)
				continue;
			fprintf(stderr, "preview VIDIOC_DQBUF failed errno=%d(%s)\n",
				errno, strerror(errno));
			stop_reason = PREVIEW_STOP_DQBUF_ERROR;
			ret = -errno;
			break;
		}
		index = buffer.index;
		if (index >= context.buffer_count) {
			fprintf(stderr, "preview invalid buffer index=%u count=%u\n",
				index, context.buffer_count);
			stop_reason = PREVIEW_STOP_DQBUF_ERROR;
			ret = -EINVAL;
			break;
		}
		if (!(buffer.flags & V4L2_BUF_FLAG_ERROR) && planes[0].bytesused) {
			long long now = monotonic_us();

			good_frames++;
			last_frame = now;
			if (good_frames == 1)
				printf("preview first frame sequence=%u bytes=%u flags=0x%x\n",
				       buffer.sequence, planes[0].bytesused,
				       buffer.flags);
			if (!display_active) {
				if (recovery_drm_activate(&display) < 0) {
					stop_reason = PREVIEW_STOP_DISPLAY_ERROR;
					ret = -EIO;
					break;
				}
				display_active = 1;
				printf("preview DRM activated after first frame\n");
			}
			if (now - last_present >= PREVIEW_FPS_LIMIT_US) {
				ret = preview_render(&display,
						     context.buffers[index].address,
						     context.bytesperline, context.width,
						     context.height, focus);
				if (ret)
					stop_reason = PREVIEW_STOP_DISPLAY_ERROR;
				if (ret)
					break;
				last_present = now;
			}
			if (good_frames == MIN_READY_FRAMES && context.lens_fd < 0) {
				context.lens_fd = open(context.lens_path, O_RDWR | O_CLOEXEC);
				if (context.lens_fd < 0) {
					ret = -errno;
					break;
				}
				printf("preview lens opened path=%s fd=%d\n",
				       context.lens_path, context.lens_fd);
				(void)preview_focus_set(&context, &focus, 0);
			}
		}
		else {
			fprintf(stderr, "preview rejected frame sequence=%u flags=0x%x "
				"bytes=%u\n", buffer.sequence, buffer.flags,
				planes[0].bytesused);
		}

		memset(&buffer, 0, sizeof(buffer));
		memset(planes, 0, sizeof(planes));
		buffer.type = stream_type;
		buffer.memory = V4L2_MEMORY_MMAP;
		buffer.index = index;
		buffer.length = 1;
		buffer.m.planes = planes;
		if (xioctl(context.video_fd, VIDIOC_QBUF, &buffer) < 0) {
			fprintf(stderr, "preview VIDIOC_QBUF failed errno=%d(%s)\n",
				errno, strerror(errno));
			stop_reason = PREVIEW_STOP_DQBUF_ERROR;
			ret = -errno;
			break;
		}
	}
	if (preview_signal_number) {
		stop_reason = PREVIEW_STOP_SIGNAL;
		if (!ret)
			fprintf(stderr, "preview stopped by signal=%d\n",
				preview_signal_number);
	}
	if (!ret && good_frames == 0) {
		stop_reason = stop_reason == PREVIEW_STOP_NONE ?
			PREVIEW_STOP_NO_FRAME : stop_reason;
		ret = -EIO;
	}

	preview_close_inputs(inputs, input_count);
out_display:
	recovery_drm_close(&display);
out_video:
	if (context.lens_fd >= 0)
		close(context.lens_fd);
	cleanup_video(&context);
out_sync:
	if (context.media_fd >= 0)
		close(context.media_fd);
	pthread_cond_destroy(&context.condition);
	pthread_mutex_destroy(&context.lock);
	if (ret < 0)
		fprintf(stderr, "preview result=FAIL reason=%s rc=%d errno=%d(%s) "
			"frames=%u\n", preview_stop_reason_name(stop_reason), ret,
			-ret, strerror(-ret), good_frames);
	else if (stop_reason != PREVIEW_STOP_NONE)
		printf("preview result=STOP reason=%s frames=%u\n",
		       preview_stop_reason_name(stop_reason), good_frames);
	else
		printf("preview result=PASS frames=%u\n", good_frames);
	return ret < 0 ? 1 : 0;
}
