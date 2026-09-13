// SPDX-License-Identifier: GPL-2.0-only
/*
 * OP3 G1 test: keep the IMX298 stream active while issuing one VCM move.
 *
 * This is deliberately a bounded diagnostic helper, not an autofocus
 * implementation.  It discovers the media, sensor, lens, and RDI nodes by
 * their names, starts one continuous stream, waits for four good frames, and
 * sends exactly one V4L2_CID_FOCUS_ABSOLUTE request while another thread
 * continues to DQBUF/QBUF.  It keeps the lens fd open until capture cleanup.
 */

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <linux/media.h>
#include <linux/v4l2-mediabus.h>
#include <linux/v4l2-subdev.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MAX_MEDIA_NODES 32
#define MAX_SUBDEV_NODES 64
#define MAX_VIDEO_NODES 64
#define MAX_BUFFERS 8
#define MAX_PIPELINE_ENTITIES 8
#define MAX_FOCUS_SWEEP 8
#define MAX_FRAME_COUNT 256
#define FOCUS_SETTLE_US 150000
#define FOCUS_SAMPLE_FRAMES 3
#define MIN_SWEEP_FRAMES_PER_POSITION 24
#define MIN_READY_FRAMES 4
#define DEFAULT_FRAME_COUNT 30
#define DEFAULT_FOCUS_POSITION 640
#define DEFAULT_EXPOSURE 893
#define DEFAULT_GAIN 240

struct mapped_buffer {
	void *address;
	size_t length;
};

struct media_graph {
	int fd;
	struct media_v2_topology topology;
	struct media_v2_entity *entities;
	struct media_v2_pad *pads;
	struct media_v2_link *links;
};

struct test_options {
	unsigned int frame_count;
	int focus_position;
	int exposure;
	int gain;
	int sweep_positions[MAX_FOCUS_SWEEP];
	unsigned int sweep_count;
	const char *output_prefix;
};

struct test_context {
	struct test_options options;
	struct mapped_buffer buffers[MAX_BUFFERS];
	unsigned int buffer_count;
	unsigned int frames_seen;
	unsigned int good_frames;
	unsigned int error_frames;
	unsigned int bytes_per_frame;
	unsigned int width;
	unsigned int height;
	unsigned int bytesperline;
	int media_fd;
	int video_fd;
	int lens_fd;
	int video_streaming;
	int focus_result;
	int focus_errno;
	int focus_cache;
	int focus_sent_position;
	int focus_thread_started;
	int capture_thread_started;
	int capture_error;
	int capture_errno;
	int capture_done;
	int stop;
	int ready;
	unsigned int pipeline_entity_count;
	char pipeline_entities[MAX_PIPELINE_ENTITIES][64];
	int sweep_save_position[MAX_FRAME_COUNT];
	char media_path[64];
	char sensor_path[64];
	char lens_path[64];
	char video_path[64];
	pthread_mutex_t lock;
	pthread_cond_t condition;
};

static volatile sig_atomic_t stop_signal;

static void on_signal(int signal_number)
{
	(void)signal_number;
	stop_signal = 1;
}

static int xioctl(int fd, unsigned long request, void *argument)
{
	int ret;

	do
		ret = ioctl(fd, request, argument);
	while (ret < 0 && errno == EINTR);

	return ret;
}

static long long monotonic_us(void)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
		return 0;

	return (long long)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

static int contains_ci(const char *haystack, const char *needle)
{
	size_t i;
	size_t j;

	if (!*needle)
		return 1;

	for (i = 0; haystack[i]; i++) {
		for (j = 0; needle[j] && haystack[i + j]; j++) {
			char a = haystack[i + j];
			char b = needle[j];

			if (a >= 'A' && a <= 'Z')
				a += 'a' - 'A';
			if (b >= 'A' && b <= 'Z')
				b += 'a' - 'A';
			if (a != b)
				break;
		}
		if (!needle[j])
			return 1;
	}

	return 0;
}

static void fourcc(char output[5], uint32_t value)
{
	output[0] = value & 0xff;
	output[1] = (value >> 8) & 0xff;
	output[2] = (value >> 16) & 0xff;
	output[3] = (value >> 24) & 0xff;
	output[4] = '\0';
}

static int read_line(const char *path, char *output, size_t output_size)
{
	FILE *file;

	if (!output_size)
		return -EINVAL;

	output[0] = '\0';
	file = fopen(path, "r");
	if (!file)
		return -errno;
	if (!fgets(output, output_size, file)) {
		int ret = errno ? -errno : -EIO;

		fclose(file);
		return ret;
	}
	fclose(file);
	output[strcspn(output, "\r\n")] = '\0';
	return 0;
}

static int subdev_name(unsigned int index, char *output, size_t output_size)
{
	char path[128];

	snprintf(path, sizeof(path), "/sys/class/video4linux/v4l-subdev%u/name",
		 index);
	return read_line(path, output, output_size);
}

static int video_name(unsigned int index, char *output, size_t output_size)
{
	char path[128];

	snprintf(path, sizeof(path), "/sys/class/video4linux/video%u/name", index);
	return read_line(path, output, output_size);
}

static void print_device_state(const char *tag, const char *path)
{
	const char *base;
	char runtime_path[256];
	char control_path[256];
	char runtime[64];
	char control[64];

	base = strrchr(path, '/');
	base = base ? base + 1 : path;
	snprintf(runtime_path, sizeof(runtime_path),
		 "/sys/class/video4linux/%s/device/power/runtime_status", base);
	snprintf(control_path, sizeof(control_path),
		 "/sys/class/video4linux/%s/device/power/control", base);
	if (read_line(runtime_path, runtime, sizeof(runtime)) < 0)
		strcpy(runtime, "unavailable");
	if (read_line(control_path, control, sizeof(control)) < 0)
		strcpy(control, "unavailable");
	printf("state tag=%s node=%s runtime_status=%s power_control=%s\n", tag,
	       path, runtime, control);
}

static void print_clock_state(void)
{
	FILE *file;
	char line[256];

	file = fopen("/sys/kernel/debug/clk/clk_summary", "r");
	if (!file) {
		printf("state tag=clock clk_summary=unavailable errno=%d\n", errno);
		return;
	}

	printf("state tag=clock clk_summary=available matching=\n");
	while (fgets(line, sizeof(line), file)) {
		if (contains_ci(line, "mclk") || contains_ci(line, "cam") ||
		    contains_ci(line, "cci"))
			printf("clock %s", line);
	}
	fclose(file);
}

static void print_runtime_state(struct test_context *context, const char *tag)
{
	print_device_state(tag, context->sensor_path);
	print_device_state(tag, context->lens_path);
	print_clock_state();
}

static int media_graph_open(struct media_graph *graph, const char *path)
{
	struct media_v2_topology topology = { 0 };
	int saved_errno;

	memset(graph, 0, sizeof(*graph));
	graph->fd = -1;
	graph->fd = open(path, O_RDWR | O_CLOEXEC);
	if (graph->fd < 0)
		return -errno;
	if (xioctl(graph->fd, MEDIA_IOC_G_TOPOLOGY, &topology) < 0)
		goto error;

	graph->entities = calloc(topology.num_entities, sizeof(*graph->entities));
	graph->pads = calloc(topology.num_pads, sizeof(*graph->pads));
	graph->links = calloc(topology.num_links, sizeof(*graph->links));
	if ((!graph->entities && topology.num_entities) ||
	    (!graph->pads && topology.num_pads) ||
	    (!graph->links && topology.num_links))
		goto error;

	graph->topology = topology;
	topology.ptr_entities = (uintptr_t)graph->entities;
	topology.ptr_pads = (uintptr_t)graph->pads;
	topology.ptr_links = (uintptr_t)graph->links;
	if (xioctl(graph->fd, MEDIA_IOC_G_TOPOLOGY, &topology) < 0)
		goto error;
	graph->topology = topology;
	return 0;

error:
	saved_errno = errno;
	free(graph->entities);
	free(graph->pads);
	free(graph->links);
	if (graph->fd >= 0)
		close(graph->fd);
	memset(graph, 0, sizeof(*graph));
	graph->fd = -1;
	return -saved_errno;
}

static void media_graph_close(struct media_graph *graph)
{
	free(graph->entities);
	free(graph->pads);
	free(graph->links);
	if (graph->fd >= 0)
		close(graph->fd);
	memset(graph, 0, sizeof(*graph));
	graph->fd = -1;
}

static const struct media_v2_entity *media_find_entity(
		const struct media_graph *graph, uint32_t id)
{
	uint32_t i;

	for (i = 0; i < graph->topology.num_entities; i++)
		if (graph->entities[i].id == id)
			return &graph->entities[i];
	return NULL;
}

static const struct media_v2_pad *media_find_pad(const struct media_graph *graph,
							 uint32_t id)
{
	uint32_t i;

	for (i = 0; i < graph->topology.num_pads; i++)
		if (graph->pads[i].id == id)
			return &graph->pads[i];
	return NULL;
}

static int media_entity_index(const struct media_graph *graph, uint32_t id)
{
	uint32_t i;

	for (i = 0; i < graph->topology.num_entities; i++)
		if (graph->entities[i].id == id)
			return (int)i;
	return -1;
}

static int pipeline_entity_selected(const struct test_context *context,
					    const char *name)
{
	unsigned int i;

	for (i = 0; i < context->pipeline_entity_count; i++)
		if (!strcmp(context->pipeline_entities[i], name))
			return 1;
	return 0;
}

static int pipeline_entity_add(struct test_context *context, const char *name)
{
	if (pipeline_entity_selected(context, name))
		return 0;
	if (context->pipeline_entity_count >= MAX_PIPELINE_ENTITIES)
		return -ENOSPC;
	strncpy(context->pipeline_entities[context->pipeline_entity_count], name,
		sizeof(context->pipeline_entities[0]) - 1);
	context->pipeline_entities[context->pipeline_entity_count][sizeof(context->pipeline_entities[0]) - 1] = '\0';
	context->pipeline_entity_count++;
	return 0;
}

static void media_print_graph(const struct media_graph *graph)
{
	uint32_t i;

	printf("media graph=%s entities=%u pads=%u links=%u\n", "selected",
	       graph->topology.num_entities, graph->topology.num_pads,
	       graph->topology.num_links);
	for (i = 0; i < graph->topology.num_entities; i++)
		printf("media entity id=%u function=0x%x name=%s\n",
		       graph->entities[i].id, graph->entities[i].function,
		       graph->entities[i].name);
	for (i = 0; i < graph->topology.num_links; i++) {
		const struct media_v2_pad *source;
		const struct media_v2_pad *sink;
		const struct media_v2_entity *source_entity;
		const struct media_v2_entity *sink_entity;

		source = media_find_pad(graph, graph->links[i].source_id);
		sink = media_find_pad(graph, graph->links[i].sink_id);
		if (!source || !sink)
			continue;
		source_entity = media_find_entity(graph, source->entity_id);
		sink_entity = media_find_entity(graph, sink->entity_id);
		if (!source_entity || !sink_entity)
			continue;
		printf("media link %s:%u -> %s:%u flags=0x%x\n",
		       source_entity->name, source->index, sink_entity->name,
		       sink->index, graph->links[i].flags);
	}
}

static int select_media_graph(struct test_context *context)
{
	struct media_graph graph;
	char path[64];
	uint32_t i;
	int ret;

	for (i = 0; i < MAX_MEDIA_NODES; i++) {
		snprintf(path, sizeof(path), "/dev/media%u", i);
		ret = media_graph_open(&graph, path);
		if (ret)
			continue;

		for (uint32_t entity = 0; entity < graph.topology.num_entities;
		     entity++) {
			if (contains_ci(graph.entities[entity].name, "imx298"))
				context->ready |= 1;
			if (contains_ci(graph.entities[entity].name, "vfe0_rdi0"))
				context->ready |= 2;
		}
		if (context->ready == 3) {
			snprintf(context->media_path, sizeof(context->media_path), "%s",
				 path);
			context->media_fd = graph.fd;
			graph.fd = -1;
			media_print_graph(&graph);
			media_graph_close(&graph);
			context->ready = 0;
			printf("discover media=%s\n", context->media_path);
			return 0;
		}
		context->ready = 0;
		media_graph_close(&graph);
	}

	fprintf(stderr, "discover failed: media graph with imx298 and vfe0_rdi0 not found\n");
	return -ENODEV;
}

static int find_named_subdev(const char *needle, char *path, size_t path_size)
{
	char name[128];
	unsigned int i;

	for (i = 0; i < MAX_SUBDEV_NODES; i++) {
		if (subdev_name(i, name, sizeof(name)) < 0)
			continue;
		if (!contains_ci(name, needle))
			continue;
		snprintf(path, path_size, "/dev/v4l-subdev%u", i);
		printf("discover subdev=%s name=%s\n", path, name);
		return 0;
	}
	return -ENODEV;
}

static int find_rdi_video(char *path, size_t path_size)
{
	char name[128];
	char card[32];
	struct v4l2_capability capability;
	int fd;
	unsigned int i;

	for (i = 0; i < MAX_VIDEO_NODES; i++) {
		if (video_name(i, name, sizeof(name)) == 0 &&
		    (contains_ci(name, "rdi") || contains_ci(name, "vfe"))) {
			snprintf(path, path_size, "/dev/video%u", i);
			printf("discover video=%s name=%s\n", path, name);
			return 0;
		}

		snprintf(path, path_size, "/dev/video%u", i);
		fd = open(path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
		if (fd < 0)
			continue;
		memset(&capability, 0, sizeof(capability));
		if (xioctl(fd, VIDIOC_QUERYCAP, &capability) == 0) {
			memcpy(card, capability.card, sizeof(card) - 1);
			card[sizeof(card) - 1] = '\0';
			if (contains_ci((char *)capability.card, "rdi") ||
			    contains_ci((char *)capability.card, "vfe")) {
				printf("discover video=%s card=%s driver=%s\n", path,
				       card, capability.driver);
				close(fd);
				return 0;
			}
		}
		close(fd);
	}

	fprintf(stderr, "discover failed: VFE RDI video node not found\n");
	return -ENODEV;
}

static int enable_media_path(struct test_context *context)
{
	struct media_graph graph;
	int *queue = NULL;
	int *previous_entity = NULL;
	int *previous_link = NULL;
	int *path_links = NULL;
	unsigned char *visited = NULL;
	int start = -1;
	int target = -1;
	unsigned int head = 0;
	unsigned int tail = 0;
	unsigned int path_length = 0;
	unsigned int i;
	int ret;

	ret = media_graph_open(&graph, context->media_path);
	if (ret)
		return ret;

	for (i = 0; i < graph.topology.num_entities; i++) {
		if (contains_ci(graph.entities[i].name, "imx298"))
			start = (int)i;
		if (contains_ci(graph.entities[i].name, "vfe0_rdi0"))
			target = (int)i;
	}
	if (start < 0 || target < 0) {
		ret = -ENODEV;
		goto out;
	}

	queue = calloc(graph.topology.num_entities, sizeof(*queue));
	previous_entity = calloc(graph.topology.num_entities,
					 sizeof(*previous_entity));
	previous_link = calloc(graph.topology.num_entities,
				       sizeof(*previous_link));
	visited = calloc(graph.topology.num_entities, sizeof(*visited));
	path_links = calloc(graph.topology.num_entities, sizeof(*path_links));
	if ((!queue && graph.topology.num_entities) ||
	    (!previous_entity && graph.topology.num_entities) ||
	    (!previous_link && graph.topology.num_entities) ||
	    (!visited && graph.topology.num_entities) ||
	    (!path_links && graph.topology.num_entities)) {
		ret = -ENOMEM;
		goto out;
	}
	for (i = 0; i < graph.topology.num_entities; i++) {
		previous_entity[i] = -1;
		previous_link[i] = -1;
	}
	queue[tail++] = start;
	visited[start] = 1;

	while (head < tail && !visited[target]) {
		int current = queue[head++];

		for (i = 0; i < graph.topology.num_links; i++) {
			const struct media_v2_pad *source;
			const struct media_v2_pad *sink;
			int source_index;
			int sink_index;

			if ((graph.links[i].flags & MEDIA_LNK_FL_LINK_TYPE) !=
			    MEDIA_LNK_FL_DATA_LINK)
				continue;
			source = media_find_pad(&graph, graph.links[i].source_id);
			sink = media_find_pad(&graph, graph.links[i].sink_id);
			if (!source || !sink)
				continue;
			source_index = media_entity_index(&graph, source->entity_id);
			sink_index = media_entity_index(&graph, sink->entity_id);
			if (source_index != current || sink_index < 0 ||
			    visited[sink_index])
				continue;
			visited[sink_index] = 1;
			previous_entity[sink_index] = current;
			previous_link[sink_index] = (int)i;
			queue[tail++] = sink_index;
		}
	}

	if (!visited[target]) {
		ret = -ENOLINK;
		goto out;
	}

	for (i = target; i != (unsigned int)start; i = previous_entity[i])
		path_links[path_length++] = previous_link[i];
	printf("media path %s -> %s links=%u\n", graph.entities[start].name,
	       graph.entities[target].name, path_length);
	for (i = path_length; i > 0; i--) {
		struct media_v2_link *link = &graph.links[path_links[i - 1]];
		const struct media_v2_pad *source = media_find_pad(&graph,
								   link->source_id);
		const struct media_v2_pad *sink = media_find_pad(&graph,
								 link->sink_id);
		const struct media_v2_entity *source_entity = source ?
			media_find_entity(&graph, source->entity_id) : NULL;
		const struct media_v2_entity *sink_entity = sink ?
			media_find_entity(&graph, sink->entity_id) : NULL;
		struct media_link_desc descriptor;

		if (!source || !sink || !source_entity || !sink_entity) {
			ret = -EINVAL;
			goto out;
		}
		ret = pipeline_entity_add(context, source_entity->name);
		if (ret)
			goto out;
		ret = pipeline_entity_add(context, sink_entity->name);
		if (ret)
			goto out;
		printf("media selected link %s:%u -> %s:%u flags=0x%x\n",
		       source_entity->name, source->index, sink_entity->name,
		       sink->index, link->flags);
		if (link->flags & MEDIA_LNK_FL_ENABLED)
			continue;
		if (link->flags & MEDIA_LNK_FL_IMMUTABLE) {
			ret = -EINVAL;
			goto out;
		}
		memset(&descriptor, 0, sizeof(descriptor));
		descriptor.source.entity = source->entity_id;
		descriptor.source.index = source->index;
		descriptor.source.flags = source->flags;
		descriptor.sink.entity = sink->entity_id;
		descriptor.sink.index = sink->index;
		descriptor.sink.flags = sink->flags;
		descriptor.flags = link->flags | MEDIA_LNK_FL_ENABLED;
		if (xioctl(graph.fd, MEDIA_IOC_SETUP_LINK, &descriptor) < 0) {
			ret = -errno;
			fprintf(stderr, "media SETUP_LINK failed: %s\n",
				strerror(errno));
			goto out;
		}
	}
	ret = 0;

out:
	free(queue);
	free(previous_entity);
	free(previous_link);
	free(visited);
	free(path_links);
	media_graph_close(&graph);
	return ret;
}

static int configure_subdev(const char *path, const char *name)
{
	struct v4l2_subdev_format format;
	char code[5];
	unsigned int pad;
	int fd;
	int configured = 0;

	fd = open(path, O_RDWR | O_CLOEXEC);
	if (fd < 0)
		return -errno;
	printf("configure subdev=%s name=%s\n", path, name);
	for (pad = 0; pad < 8; pad++) {
		memset(&format, 0, sizeof(format));
		format.which = V4L2_SUBDEV_FORMAT_ACTIVE;
		format.pad = pad;
		if (xioctl(fd, VIDIOC_SUBDEV_G_FMT, &format) < 0)
			continue;
		fourcc(code, format.format.code);
		printf("subdev=%s pad=%u before=%ux%u code=0x%08x(%s)\n", path,
		       pad, format.format.width, format.format.height,
		       format.format.code, code);
		format.format.width = 1476;
		format.format.height = 834;
		format.format.code = MEDIA_BUS_FMT_SRGGB10_1X10;
		format.format.field = V4L2_FIELD_NONE;
		format.format.colorspace = V4L2_COLORSPACE_RAW;
		format.format.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
		format.format.quantization = V4L2_QUANTIZATION_FULL_RANGE;
		format.format.xfer_func = V4L2_XFER_FUNC_NONE;
		if (xioctl(fd, VIDIOC_SUBDEV_S_FMT, &format) < 0) {
			printf("subdev=%s pad=%u S_FMT failed errno=%d(%s)\n", path,
			       pad, errno, strerror(errno));
			continue;
		}
		fourcc(code, format.format.code);
		printf("subdev=%s pad=%u after=%ux%u code=0x%08x(%s)\n", path,
		       pad, format.format.width, format.format.height,
		       format.format.code, code);
		configured++;
	}
	close(fd);
	return configured;
}

static int configure_pipeline_subdevs(const struct test_context *context)
{
	char path[64];
	char name[128];
	unsigned int i;
	int configured = 0;

	for (i = 0; i < MAX_SUBDEV_NODES; i++) {
		if (subdev_name(i, name, sizeof(name)) < 0)
			continue;
		if (!contains_ci(name, "imx298") &&
		    !contains_ci(name, "csiphy") &&
		    !contains_ci(name, "csid") &&
		    !contains_ci(name, "ispif") &&
		    !contains_ci(name, "rdi"))
			continue;
		if (!pipeline_entity_selected(context, name))
			continue;
		snprintf(path, sizeof(path), "/dev/v4l-subdev%u", i);
		if (configure_subdev(path, name) > 0)
			configured++;
	}
	return configured;
}

static int set_sensor_controls(struct test_context *context)
{
	struct v4l2_control control;
	int fd;

	fd = open(context->sensor_path, O_RDWR | O_CLOEXEC);
	if (fd < 0)
		return -errno;
	memset(&control, 0, sizeof(control));
	control.id = V4L2_CID_EXPOSURE;
	control.value = context->options.exposure;
	if (xioctl(fd, VIDIOC_S_CTRL, &control) < 0) {
		int ret = -errno;

		fprintf(stderr, "sensor exposure=%d failed: %s\n",
			context->options.exposure, strerror(errno));
		close(fd);
		return ret;
	}
	printf("sensor exposure=%d applied=%d\n", context->options.exposure,
	       control.value);
	memset(&control, 0, sizeof(control));
	control.id = V4L2_CID_ANALOGUE_GAIN;
	control.value = context->options.gain;
	if (xioctl(fd, VIDIOC_S_CTRL, &control) < 0) {
		int ret = -errno;

		fprintf(stderr, "sensor analogue_gain=%d failed: %s\n",
			context->options.gain, strerror(errno));
		close(fd);
		return ret;
	}
	printf("sensor analogue_gain=%d applied=%d\n", context->options.gain,
	       control.value);
	close(fd);
	return 0;
}

static int write_all(int fd, const void *buffer, size_t length)
{
	const unsigned char *bytes = buffer;

	while (length) {
		ssize_t written = write(fd, bytes, length);

		if (written < 0) {
			if (errno == EINTR)
				continue;
			return -errno;
		}
		if (!written)
			return -EIO;
		bytes += written;
		length -= written;
	}
	return 0;
}

static int save_frame(struct test_context *context, unsigned int frame,
			      const void *data, size_t length)
{
	char path[256];
	int fd;
	int ret;

	snprintf(path, sizeof(path), "%s-%04u.raw", context->options.output_prefix,
		 frame);
	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
	if (fd < 0)
		return -errno;
	ret = write_all(fd, data, length);
	if (close(fd) < 0 && !ret)
		ret = -errno;
	if (ret)
		fprintf(stderr, "frame=%u output=%s failed errno=%d(%s)\n", frame,
			path, -ret, strerror(-ret));
	return ret;
}

static int save_focus_frame(struct test_context *context, int position,
				    unsigned int frame, const void *data,
				    size_t length)
{
	char path[256];
	int fd;
	int ret;

	snprintf(path, sizeof(path), "%s-pos-%04d-frame-%04u.raw",
		 context->options.output_prefix, position, frame);
	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
	if (fd < 0)
		return -errno;
	ret = write_all(fd, data, length);
	if (close(fd) < 0 && !ret)
		ret = -errno;
	if (ret)
		fprintf(stderr, "focus sample position=%d output=%s failed errno=%d(%s)\n",
			position, path, -ret, strerror(-ret));
	return ret;
}

static void set_stop(struct test_context *context, int capture_error,
			     int error_number)
{
	pthread_mutex_lock(&context->lock);
	context->stop = 1;
	if (capture_error) {
		context->capture_error = 1;
		context->capture_errno = error_number;
	}
	pthread_cond_broadcast(&context->condition);
	pthread_mutex_unlock(&context->lock);
}

static int should_stop(struct test_context *context)
{
	int stop;

	if (stop_signal)
		set_stop(context, 0, 0);
	pthread_mutex_lock(&context->lock);
	stop = context->stop;
	pthread_mutex_unlock(&context->lock);
	return stop;
}

static void *capture_thread(void *argument)
{
	struct test_context *context = argument;
	struct pollfd pollfd = { .fd = context->video_fd, .events = POLLIN };
	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	struct v4l2_buffer buffer;
	unsigned int frame = 0;

	while (frame < context->options.frame_count) {
		int poll_result;
		int ret;
		unsigned int index;
		size_t bytes;
		long long host_timestamp;
		int focus_position = -1;

		if (should_stop(context))
			break;
		poll_result = poll(&pollfd, 1, 1000);
		if (poll_result < 0) {
			if (errno == EINTR) {
				continue;
			}
			set_stop(context, 1, errno);
			break;
		}
		if (!poll_result) {
			fprintf(stderr, "capture poll timeout frame=%u\n", frame);
			set_stop(context, 1, ETIMEDOUT);
			break;
		}

		memset(&buffer, 0, sizeof(buffer));
		memset(planes, 0, sizeof(planes));
		buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buffer.memory = V4L2_MEMORY_MMAP;
		buffer.length = 1;
		buffer.m.planes = planes;
		if (xioctl(context->video_fd, VIDIOC_DQBUF, &buffer) < 0) {
			if (errno == EAGAIN) {
				continue;
			}
			set_stop(context, 1, errno);
			break;
		}
		index = buffer.index;
		if (index >= context->buffer_count) {
			fprintf(stderr, "capture invalid buffer index=%u\n", index);
			set_stop(context, 1, EINVAL);
			break;
		}
		bytes = planes[0].bytesused;
		pthread_mutex_lock(&context->lock);
		context->frames_seen = frame + 1;
		if (frame < MAX_FRAME_COUNT)
			focus_position = context->sweep_save_position[frame];
		pthread_cond_broadcast(&context->condition);
		pthread_mutex_unlock(&context->lock);
		host_timestamp = monotonic_us();
		printf("frame=%u host_us=%lld sequence=%u timestamp=%lld.%06ld buffer=%u bytes=%zu flags=0x%x data_offset=%u\n",
		       frame, host_timestamp, buffer.sequence,
		       (long long)buffer.timestamp.tv_sec,
		       buffer.timestamp.tv_usec, index, bytes, buffer.flags,
		       planes[0].data_offset);
		if ((buffer.flags & V4L2_BUF_FLAG_ERROR) ||
		    !bytes || bytes > context->buffers[index].length) {
			context->error_frames++;
			fprintf(stderr, "frame=%u rejected error_flag=%u bytes=%zu mapped=%zu\n",
				frame, !!(buffer.flags & V4L2_BUF_FLAG_ERROR), bytes,
				context->buffers[index].length);
		} else {
			if (context->options.sweep_count && focus_position >= 0)
				ret = save_focus_frame(context, focus_position, frame,
						      context->buffers[index].address, bytes);
			else if (!context->options.sweep_count)
				ret = save_frame(context, frame, context->buffers[index].address,
						 bytes);
			else
				ret = 0;
			if (ret) {
				set_stop(context, 1, -ret);
				break;
			}
			context->good_frames++;
			if (context->good_frames == MIN_READY_FRAMES) {
				pthread_mutex_lock(&context->lock);
				context->ready = 1;
				pthread_cond_broadcast(&context->condition);
				pthread_mutex_unlock(&context->lock);
				printf("capture ready good_frames=%u\n", context->good_frames);
			}
		}

		memset(&buffer, 0, sizeof(buffer));
		memset(planes, 0, sizeof(planes));
		buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buffer.memory = V4L2_MEMORY_MMAP;
		buffer.index = index;
		buffer.length = 1;
		buffer.m.planes = planes;
		if (xioctl(context->video_fd, VIDIOC_QBUF, &buffer) < 0) {
			set_stop(context, 1, errno);
			break;
		}
		frame++;
	}

	pthread_mutex_lock(&context->lock);
	context->capture_done = 1;
	pthread_cond_broadcast(&context->condition);
	pthread_mutex_unlock(&context->lock);
	return NULL;
}

static int choose_focus_position(struct test_context *context,
					struct v4l2_control *control)
{
	int position = context->options.focus_position;

	if (xioctl(context->lens_fd, VIDIOC_G_CTRL, control) < 0) {
		fprintf(stderr, "lens VIDIOC_G_CTRL before focus failed errno=%d(%s)\n",
			errno, strerror(errno));
		return -errno;
	}
	context->focus_cache = control->value;
	if (position == context->focus_cache)
		position = position == 1023 ? position - 1 : position + 1;
	if (position < 0 || position > 1023)
		return -ERANGE;
	return position;
}

static int run_focus_sweep(struct test_context *context);

static void *focus_thread(void *argument)
{
	struct test_context *context = argument;
	struct v4l2_control control;
	int position;
	int ret;

	pthread_mutex_lock(&context->lock);
	while (!context->ready && !context->stop && !context->capture_done)
		pthread_cond_wait(&context->condition, &context->lock);
	ret = context->stop ? -ECANCELED : 0;
	if (context->capture_done)
		ret = -ECANCELED;
	pthread_mutex_unlock(&context->lock);
	if (ret)
		return NULL;

	print_runtime_state(context, "before-lens-open");
	context->lens_fd = open(context->lens_path, O_RDWR | O_CLOEXEC);
	if (context->lens_fd < 0) {
		ret = -errno;
		context->focus_result = -1;
		context->focus_errno = -ret;
		fprintf(stderr, "lens open failed path=%s errno=%d(%s)\n",
			context->lens_path, -ret, strerror(-ret));
		set_stop(context, 1, -ret);
		return NULL;
	}
	printf("lens opened path=%s fd=%d\n", context->lens_path,
	       context->lens_fd);
	print_runtime_state(context, "after-lens-open");
	if (context->options.sweep_count) {
		ret = run_focus_sweep(context);
		if (ret) {
			context->focus_result = -1;
			context->focus_errno = -ret;
			fprintf(stderr, "focus sweep failed errno=%d(%s)\n",
				-ret, strerror(-ret));
			set_stop(context, 1, -ret);
		} else {
			context->focus_result = 0;
		}
		goto wait_capture;
	}

	memset(&control, 0, sizeof(control));
	control.id = V4L2_CID_FOCUS_ABSOLUTE;
	position = choose_focus_position(context, &control);
	if (position < 0) {
		context->focus_result = -1;
		context->focus_errno = -position;
		set_stop(context, 1, -position);
		goto wait_capture;
	}
	printf("focus cache=%d requested=%d\n", context->focus_cache, position);
	print_runtime_state(context, "before-focus-write");
	control.value = position;
	ret = xioctl(context->lens_fd, VIDIOC_S_CTRL, &control);
	if (ret < 0) {
		context->focus_result = -1;
		context->focus_errno = errno;
		fprintf(stderr, "focus write position=%d failed errno=%d(%s)\n",
			position, errno, strerror(errno));
		set_stop(context, 1, errno);
		goto wait_capture;
	}
	context->focus_result = 0;
	context->focus_sent_position = position;
	printf("focus write position=%d ioctl_rc=%d\n", position, ret);
	print_runtime_state(context, "after-focus-write");

wait_capture:
	pthread_mutex_lock(&context->lock);
	while (!context->capture_done)
		pthread_cond_wait(&context->condition, &context->lock);
	pthread_mutex_unlock(&context->lock);
	print_runtime_state(context, "before-lens-close");
	close(context->lens_fd);
	context->lens_fd = -1;
	printf("lens closed\n");
	return NULL;
}

static int parse_focus_sweep(const char *text, struct test_options *options)
{
	char *copy;
	char *saveptr = NULL;
	char *token;

	copy = strdup(text);
	if (!copy)
		return -ENOMEM;
	for (token = strtok_r(copy, ",", &saveptr); token;
	     token = strtok_r(NULL, ",", &saveptr)) {
		char *end;
		long value;

		if (options->sweep_count >= MAX_FOCUS_SWEEP) {
			free(copy);
			return -E2BIG;
		}
		value = strtol(token, &end, 10);
		if (*token == '\0' || *end != '\0' || value < 0 || value > 1023) {
			free(copy);
			return -EINVAL;
		}
		options->sweep_positions[options->sweep_count++] = (int)value;
	}
	free(copy);
	return options->sweep_count >= 2 ? 0 : -EINVAL;
}

static int run_focus_sweep(struct test_context *context)
{
	unsigned int i;

	for (i = 0; i < context->options.sweep_count; i++) {
		struct v4l2_control control = {
			.id = V4L2_CID_FOCUS_ABSOLUTE,
			.value = context->options.sweep_positions[i],
		};
		unsigned int first_sample_frame;
		unsigned int last_sample_frame;
		struct timespec settle = {
			.tv_sec = FOCUS_SETTLE_US / 1000000,
			.tv_nsec = (FOCUS_SETTLE_US % 1000000) * 1000,
		};
		long long command_us;
		int ret;

		ret = xioctl(context->lens_fd, VIDIOC_S_CTRL, &control);
		command_us = monotonic_us();
		printf("sweep focus position=%d command_us=%lld ioctl_rc=%d\n",
		       control.value, command_us, ret);
		if (ret < 0)
			return -errno;

		/* Keep capture running while the VCM settles; do not hold its queue lock. */
		nanosleep(&settle, NULL);
		pthread_mutex_lock(&context->lock);
		if (context->capture_done || context->stop) {
			pthread_mutex_unlock(&context->lock);
			return -ECANCELED;
		}
		first_sample_frame = context->frames_seen;
		last_sample_frame = first_sample_frame + FOCUS_SAMPLE_FRAMES - 1;
		if (last_sample_frame >= MAX_FRAME_COUNT ||
		    last_sample_frame >= context->options.frame_count) {
			pthread_mutex_unlock(&context->lock);
			return -ENOSPC;
		}
		for (unsigned int frame = first_sample_frame;
		     frame <= last_sample_frame; frame++)
			context->sweep_save_position[frame] = control.value;
		pthread_cond_broadcast(&context->condition);
		while (context->frames_seen <= last_sample_frame &&
		       !context->capture_done && !context->stop)
			pthread_cond_wait(&context->condition, &context->lock);
		ret = context->frames_seen > last_sample_frame ? 0 : -ETIMEDOUT;
		pthread_mutex_unlock(&context->lock);
		if (ret)
			return ret;
		printf("sweep samples position=%d first_frame=%u last_frame=%u "
		       "count=%u settle_us=%u\n", control.value,
		       first_sample_frame, last_sample_frame,
		       FOCUS_SAMPLE_FRAMES, FOCUS_SETTLE_US);
	}
	return 0;
}

static int prepare_video(struct test_context *context)
{
	struct v4l2_requestbuffers request;
	struct v4l2_format format;
	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	unsigned int i;
	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	if (context->video_fd < 0) {
		context->video_fd = open(context->video_path, O_RDWR | O_NONBLOCK |
					 O_CLOEXEC);
		if (context->video_fd < 0)
			return -errno;
	}

	memset(&format, 0, sizeof(format));
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	format.fmt.pix_mp.width = 1476;
	format.fmt.pix_mp.height = 834;
	format.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_SRGGB10P;
	format.fmt.pix_mp.field = V4L2_FIELD_NONE;
	format.fmt.pix_mp.num_planes = 1;
	if (xioctl(context->video_fd, VIDIOC_S_FMT, &format) < 0)
		return -errno;
	context->width = format.fmt.pix_mp.width;
	context->height = format.fmt.pix_mp.height;
	context->bytesperline = format.fmt.pix_mp.plane_fmt[0].bytesperline;
	context->bytes_per_frame = format.fmt.pix_mp.plane_fmt[0].sizeimage;
	printf("video format=%s %ux%u planes=%u bytesperline=%u sizeimage=%u\n",
	       context->video_path, context->width, context->height,
	       format.fmt.pix_mp.num_planes, context->bytesperline,
	       context->bytes_per_frame);

	memset(&request, 0, sizeof(request));
	request.count = 4;
	request.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	request.memory = V4L2_MEMORY_MMAP;
	if (xioctl(context->video_fd, VIDIOC_REQBUFS, &request) < 0)
		return -errno;
	if (request.count < MIN_READY_FRAMES || request.count > MAX_BUFFERS)
		return -ENOBUFS;
	context->buffer_count = request.count;
	for (i = 0; i < context->buffer_count; i++) {
		struct v4l2_buffer query;

		memset(&query, 0, sizeof(query));
		memset(planes, 0, sizeof(planes));
		query.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		query.memory = V4L2_MEMORY_MMAP;
		query.index = i;
		query.length = VIDEO_MAX_PLANES;
		query.m.planes = planes;
		if (xioctl(context->video_fd, VIDIOC_QUERYBUF, &query) < 0)
			return -errno;
		if (query.length != 1)
			return -EINVAL;
		context->buffers[i].length = query.m.planes[0].length;
		context->buffers[i].address = mmap(NULL, context->buffers[i].length,
						  PROT_READ | PROT_WRITE, MAP_SHARED,
						  context->video_fd,
						  query.m.planes[0].m.mem_offset);
		if (context->buffers[i].address == MAP_FAILED) {
			context->buffers[i].address = NULL;
			return -errno;
		}
		printf("video buffer=%u mapped=%zu\n", i, context->buffers[i].length);
	}
	for (i = 0; i < context->buffer_count; i++) {
		struct v4l2_buffer buffer;

		memset(&buffer, 0, sizeof(buffer));
		memset(planes, 0, sizeof(planes));
		buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buffer.memory = V4L2_MEMORY_MMAP;
		buffer.index = i;
		buffer.length = 1;
		buffer.m.planes = planes;
		if (xioctl(context->video_fd, VIDIOC_QBUF, &buffer) < 0)
			return -errno;
	}
	if (xioctl(context->video_fd, VIDIOC_STREAMON, &type) < 0)
		return -errno;
	context->video_streaming = 1;
	printf("stream on continuous=1 buffers=%u\n", context->buffer_count);
	return 0;
}

static void cleanup_video(struct test_context *context)
{
	unsigned int i;
	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;

	if (context->video_streaming) {
		if (xioctl(context->video_fd, VIDIOC_STREAMOFF, &type) < 0)
			fprintf(stderr, "stream off failed errno=%d(%s)\n", errno,
				strerror(errno));
		else
			printf("stream off\n");
		context->video_streaming = 0;
	}
	for (i = 0; i < context->buffer_count; i++) {
		if (context->buffers[i].address)
			munmap(context->buffers[i].address, context->buffers[i].length);
		context->buffers[i].address = NULL;
	}
	if (context->video_fd >= 0) {
		close(context->video_fd);
		context->video_fd = -1;
	}
}

static void usage(const char *program)
{
	fprintf(stderr,
		"usage: %s [--frames N] [--focus N] [--exposure N] [--gain N] "
		"[--sweep P0,P1,...] [--output-prefix PATH]\n",
		program);
}

static int parse_options(int argc, char **argv, struct test_options *options)
{
	int i;

	options->frame_count = DEFAULT_FRAME_COUNT;
	options->focus_position = DEFAULT_FOCUS_POSITION;
	options->exposure = DEFAULT_EXPOSURE;
	options->gain = DEFAULT_GAIN;
	options->output_prefix = "/tmp/op3-af-g1";
	for (i = 1; i < argc; i++) {
		char *end;
		long value;
		const char *argument = argv[i];

		if (i + 1 >= argc && strcmp(argument, "--help"))
			return -EINVAL;
		if (!strcmp(argument, "--help")) {
			usage(argv[0]);
			return 1;
		}
		if (!strcmp(argument, "--output-prefix")) {
			options->output_prefix = argv[++i];
			continue;
		}
		if (!strcmp(argument, "--sweep")) {
			int ret = parse_focus_sweep(argv[++i], options);

			if (ret)
				return ret;
			continue;
		}
		if (i + 1 >= argc)
			return -EINVAL;
		value = strtol(argv[++i], &end, 10);
		if (*end)
			return -EINVAL;
		if (!strcmp(argument, "--frames"))
			options->frame_count = value;
		else if (!strcmp(argument, "--focus"))
			options->focus_position = value;
		else if (!strcmp(argument, "--exposure"))
			options->exposure = value;
		else if (!strcmp(argument, "--gain"))
			options->gain = value;
		else
			return -EINVAL;
	}
	if (options->frame_count < MIN_READY_FRAMES ||
	    options->frame_count > MAX_FRAME_COUNT || options->focus_position < 0 ||
	    options->focus_position > 1023)
		return -ERANGE;
	if (options->sweep_count &&
	    options->frame_count < MIN_READY_FRAMES +
	    options->sweep_count * MIN_SWEEP_FRAMES_PER_POSITION)
		return -ERANGE;
	return 0;
}

int main(int argc, char **argv)
{
	struct sigaction signal_action;
	struct test_context context;
	pthread_t capture_thread_id;
	pthread_t focus_thread_id;
	unsigned int i;
	int ret;

	memset(&context, 0, sizeof(context));
	context.media_fd = -1;
	context.video_fd = -1;
	context.lens_fd = -1;
	context.focus_result = 1;
	context.focus_cache = -1;
	context.focus_sent_position = -1;
	for (i = 0; i < MAX_FRAME_COUNT; i++)
		context.sweep_save_position[i] = -1;
	ret = parse_options(argc, argv, &context.options);
	if (ret == 1)
		return 0;
	if (ret) {
		usage(argv[0]);
		return 2;
	}
	memset(&signal_action, 0, sizeof(signal_action));
	signal_action.sa_handler = on_signal;
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
	printf("discover summary media=%s sensor=%s lens=%s video=%s\n",
	       context.media_path, context.sensor_path, context.lens_path,
	       context.video_path);
	context.video_fd = open(context.video_path, O_RDWR | O_NONBLOCK |
					O_CLOEXEC);
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
	print_runtime_state(&context, "before-stream-on");
	ret = prepare_video(&context);
	if (ret)
		goto out_video;
	if (pthread_create(&capture_thread_id, NULL, capture_thread, &context)) {
		ret = -EAGAIN;
		goto out_video;
	}
	context.capture_thread_started = 1;
	{
		int thread_error = pthread_create(&focus_thread_id, NULL, focus_thread,
						  &context);
		if (thread_error) {
			set_stop(&context, 1, thread_error);
			ret = -thread_error;
	} else {
		context.focus_thread_started = 1;
		ret = 0;
	}
	}
	if (context.capture_thread_started)
		pthread_join(capture_thread_id, NULL);
	pthread_mutex_lock(&context.lock);
	context.stop = 1;
	context.capture_done = 1;
	pthread_cond_broadcast(&context.condition);
	pthread_mutex_unlock(&context.lock);
	if (context.focus_thread_started)
		pthread_join(focus_thread_id, NULL);
	if (context.capture_error && !ret)
		ret = -context.capture_errno;
	if (context.focus_result == 1 && !ret)
		ret = -EIO;
	if (context.focus_result < 0 && !ret)
		ret = -context.focus_errno;
	printf("result frames_good=%u frames_error=%u focus_result=%d focus_position=%d focus_cache=%d\n",
	       context.good_frames, context.error_frames, context.focus_result,
	       context.focus_sent_position, context.focus_cache);
	if (!context.ready || context.good_frames < MIN_READY_FRAMES)
		ret = ret ? ret : -EIO;

out_video:
	cleanup_video(&context);
out_sync:
	if (context.media_fd >= 0)
		close(context.media_fd);
	pthread_cond_destroy(&context.condition);
	pthread_mutex_destroy(&context.lock);
	if (ret < 0)
		fprintf(stderr, "%s result=FAIL rc=%d errno=%d(%s)\n",
			context.options.sweep_count ? "G3" : "G1", ret, -ret,
			strerror(-ret));
	else
		printf("%s result=PASS capture-ready-and-focus-ioctl-returned\n",
		       context.options.sweep_count ? "G3" : "G1");
	return ret < 0 ? 1 : 0;
}
