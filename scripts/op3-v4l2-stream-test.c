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

#include <linux/media.h>
#include <linux/v4l2-mediabus.h>
#include <linux/v4l2-subdev.h>
#include <linux/videodev2.h>

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

static int media_graph_open(struct media_graph *graph, const char *path)
{
	struct media_v2_topology topology = { 0 };
	int saved_errno;

	memset(graph, 0, sizeof(*graph));
	graph->fd = open(path, O_RDWR);
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

static int media_list(const char *path)
{
	struct media_graph graph;
	uint32_t i;
	int ret;

	ret = media_graph_open(&graph, path);
	if (ret)
		return ret;

	printf("%s: entities=%u pads=%u links=%u\n", path,
	       graph.topology.num_entities, graph.topology.num_pads,
	       graph.topology.num_links);
	for (i = 0; i < graph.topology.num_entities; i++)
		printf("entity id=%u function=0x%x name=%s\n",
		       graph.entities[i].id, graph.entities[i].function,
		       graph.entities[i].name);
	for (i = 0; i < graph.topology.num_links; i++) {
		const struct media_v2_pad *source;
		const struct media_v2_pad *sink;
		const struct media_v2_entity *source_entity;
		const struct media_v2_entity *sink_entity;

		source = media_find_pad(&graph, graph.links[i].source_id);
		sink = media_find_pad(&graph, graph.links[i].sink_id);
		if (!source || !sink)
			continue;
		source_entity = media_find_entity(&graph, source->entity_id);
		sink_entity = media_find_entity(&graph, sink->entity_id);
		if (!source_entity || !sink_entity)
			continue;
		printf("link %s:%u -> %s:%u flags=0x%x\n",
		       source_entity->name, source->index, sink_entity->name,
		       sink->index, graph.links[i].flags);
	}

	media_graph_close(&graph);
	return 0;
}

static int media_graph_enable_path(const char *path)
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

	ret = media_graph_open(&graph, path);
	if (ret)
		return ret;

	for (i = 0; i < graph.topology.num_entities; i++) {
		if (strstr(graph.entities[i].name, "imx298"))
			start = (int)i;
		if (strstr(graph.entities[i].name, "msm_vfe0_rdi0"))
			target = (int)i;
	}
	if (start < 0 || target < 0) {
		fprintf(stderr, "media path endpoints not found: imx298=%d vfe0_rdi0=%d\n",
			start, target);
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
		fprintf(stderr, "no media data path from %s to %s\n",
			graph.entities[start].name, graph.entities[target].name);
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

		if (!source || !sink || !source_entity || !sink_entity) {
			ret = -EINVAL;
			goto out;
		}
		printf("  link %s:%u -> %s:%u flags=0x%x\n",
		       source_entity->name, source->index, sink_entity->name,
		       sink->index, link->flags);
		if (link->flags & MEDIA_LNK_FL_ENABLED)
			continue;
		if (link->flags & MEDIA_LNK_FL_IMMUTABLE) {
			fprintf(stderr, "  link is immutable but disabled\n");
			ret = -EINVAL;
			goto out;
		}

		{
			struct media_link_desc desc = { 0 };

			desc.source.entity = source->entity_id;
			desc.source.index = source->index;
			desc.source.flags = source->flags;
			desc.sink.entity = sink->entity_id;
			desc.sink.index = sink->index;
			desc.sink.flags = sink->flags;
			desc.flags = link->flags | MEDIA_LNK_FL_ENABLED;
			if (xioctl(graph.fd, MEDIA_IOC_SETUP_LINK, &desc) < 0) {
				ret = -errno;
				fprintf(stderr, "  SETUP_LINK failed: %s\n",
					strerror(errno));
				goto out;
			}
			printf("  link enabled\n");
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

	ret = media_graph_enable_path("/dev/media0");
	if (ret) {
		ret = -ret;
		fprintf(stderr, "media graph setup failed: %s\n", strerror(ret));
		goto out;
	}

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
	{
		int poll_ret = poll(&pfd, 1, 3000);

		if (poll_ret <= 0) {
			ret = poll_ret == 0 ? ETIMEDOUT : errno;
		fprintf(stderr, "%s: frame wait failed: %s\n", path,
			strerror(ret));
		goto out_streamoff;
		}
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
	if (argc == 3 && !strcmp(argv[1], "media-list"))
		return media_list(argv[2]);
	if (argc != 3) {
		fprintf(stderr, "usage: %s list | %s media-list /dev/media0 | %s /dev/videoX output.raw\n",
			argv[0], argv[0], argv[0]);
		return EINVAL;
	}

	ret = stream_node(argv[1], argv[2]);
	return ret ? 1 : 0;
}
