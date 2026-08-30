// SPDX-License-Identifier: MIT
/*
 * Minimal libdrm-free KMS dumb-buffer test for OnePlus 3 bring-up.
 *
 * This program intentionally uses only the DRM UAPI and libc.  It finds the
 * first connected connector on /dev/dri/card0, allocates an XRGB8888 dumb
 * buffer, modesets it to a solid colour, and keeps it visible until timeout
 * or SIGINT/SIGTERM.  It is not a compositor and does not exercise EGL, GBM,
 * Wayland, Cog, or WPE.
 */

#include <drm/drm.h>
#include <drm/drm_mode.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define CARD_PATH "/dev/dri/card0"
#define DEFAULT_SECONDS 30
/*
 * Bounded retries for the two-pass "ask for the counts, then fetch the arrays"
 * ioctls.  A list can grow between the two passes, so the second pass must be
 * repeated with larger arrays if the kernel reports more entries than fit.
 */
#define KMS_MAX_RETRY 4
/* drm_mode_get_connector.connection ABI value; libdrm normally supplies it. */
#define DRM_CONNECTOR_STATUS_CONNECTED 2

static volatile sig_atomic_t stop;

struct kms_target {
	uint32_t connector_id;
	uint32_t crtc_id;
	struct drm_mode_modeinfo mode;
};

struct kms_resources {
	uint32_t *fb_ids;
	uint32_t *crtc_ids;
	uint32_t *connector_ids;
	uint32_t *encoder_ids;
	uint32_t count_fbs;
	uint32_t count_crtcs;
	uint32_t count_connectors;
	uint32_t count_encoders;
	uint32_t capacity_fbs;
	uint32_t capacity_crtcs;
	uint32_t capacity_connectors;
	uint32_t capacity_encoders;
};

struct kms_connector {
	struct drm_mode_modeinfo *modes;
	uint32_t *encoder_ids;
	uint32_t *prop_ids;
	uint64_t *prop_values;
	uint32_t connection;
	uint32_t encoder_id;
	uint32_t count_modes;
	uint32_t count_encoders;
	uint32_t count_props;
	uint32_t capacity_modes;
	uint32_t capacity_encoders;
	uint32_t capacity_props;
};

static void usage(const char *program)
{
	fprintf(stderr,
		"Usage: %s [black|red|green|blue|white] [--seconds N|--hold]\n",
		program);
}

static void stop_handler(int signal_number)
{
	(void)signal_number;
	stop = 1;
}

static int drm_call(int fd, unsigned long request, void *argument,
		    const char *name)
{
	if (ioctl(fd, request, argument) == 0)
		return 0;

	fprintf(stderr, "%s: %s\n", name, strerror(errno));
	return -1;
}

static void free_kms_resources(struct kms_resources *resources)
{
	free(resources->fb_ids);
	free(resources->crtc_ids);
	free(resources->connector_ids);
	free(resources->encoder_ids);
	memset(resources, 0, sizeof(*resources));
}

static void free_kms_connector(struct kms_connector *connector)
{
	free(connector->modes);
	free(connector->encoder_ids);
	free(connector->prop_ids);
	free(connector->prop_values);
	memset(connector, 0, sizeof(*connector));
}

static int reserve_u32(uint32_t **array, uint32_t *capacity, uint32_t count,
		       const char *name)
{
	uint32_t *grown;

	if (count <= *capacity)
		return 0;

	grown = realloc(*array, (size_t)count * sizeof(*grown));
	if (!grown) {
		fprintf(stderr, "cannot allocate %s list (%" PRIu32 " entries)\n",
			name, count);
		return -1;
	}

	*array = grown;
	*capacity = count;
	return 0;
}

static int reserve_modes(struct drm_mode_modeinfo **modes, uint32_t *capacity,
			 uint32_t count)
{
	struct drm_mode_modeinfo *grown;

	if (count <= *capacity)
		return 0;

	grown = realloc(*modes, (size_t)count * sizeof(*grown));
	if (!grown) {
		fprintf(stderr, "cannot allocate mode list (%" PRIu32 " modes)\n",
			count);
		return -1;
	}

	*modes = grown;
	*capacity = count;
	return 0;
}

static int reserve_props(struct kms_connector *connector, uint32_t count)
{
	uint32_t *ids;
	uint64_t *values;

	if (count <= connector->capacity_props)
		return 0;

	ids = realloc(connector->prop_ids, (size_t)count * sizeof(*ids));
	if (!ids) {
		fprintf(stderr,
			"cannot allocate property list (%" PRIu32 " entries)\n",
			count);
		return -1;
	}
	connector->prop_ids = ids;

	values = realloc(connector->prop_values,
			 (size_t)count * sizeof(*values));
	if (!values) {
		fprintf(stderr,
			"cannot allocate property value list (%" PRIu32 " entries)\n",
			count);
		return -1;
	}
	connector->prop_values = values;

	connector->capacity_props = count;
	return 0;
}

/*
 * The kernel copies an array entry while its running count is below the count
 * the caller supplied, and it does not validate the matching pointer.  A second
 * pass that raises some counts while leaving their pointers NULL therefore
 * makes the kernel write to NULL and fail with -EFAULT.  Both passes below
 * always set every pointer together with the capacity it belongs to.
 */
static int fetch_resources(int fd, struct kms_resources *resources)
{
	struct drm_mode_card_res card = { 0 };
	int attempt;

	for (attempt = 0; attempt < KMS_MAX_RETRY; attempt++) {
		card.count_fbs = resources->capacity_fbs;
		card.count_crtcs = resources->capacity_crtcs;
		card.count_connectors = resources->capacity_connectors;
		card.count_encoders = resources->capacity_encoders;
		card.fb_id_ptr = (uintptr_t)resources->fb_ids;
		card.crtc_id_ptr = (uintptr_t)resources->crtc_ids;
		card.connector_id_ptr = (uintptr_t)resources->connector_ids;
		card.encoder_id_ptr = (uintptr_t)resources->encoder_ids;

		if (drm_call(fd, DRM_IOCTL_MODE_GETRESOURCES, &card,
			     "DRM_IOCTL_MODE_GETRESOURCES"))
			return -1;

		if (card.count_fbs <= resources->capacity_fbs &&
		    card.count_crtcs <= resources->capacity_crtcs &&
		    card.count_connectors <= resources->capacity_connectors &&
		    card.count_encoders <= resources->capacity_encoders)
			break;

		if (reserve_u32(&resources->fb_ids, &resources->capacity_fbs,
				card.count_fbs, "framebuffer") ||
		    reserve_u32(&resources->crtc_ids, &resources->capacity_crtcs,
				card.count_crtcs, "CRTC") ||
		    reserve_u32(&resources->connector_ids,
				&resources->capacity_connectors,
				card.count_connectors, "connector") ||
		    reserve_u32(&resources->encoder_ids,
				&resources->capacity_encoders,
				card.count_encoders, "encoder"))
			return -1;
	}

	if (attempt == KMS_MAX_RETRY) {
		fprintf(stderr, "DRM resource counts keep growing\n");
		return -1;
	}

	resources->count_fbs = card.count_fbs;
	resources->count_crtcs = card.count_crtcs;
	resources->count_connectors = card.count_connectors;
	resources->count_encoders = card.count_encoders;
	return 0;
}

static int fetch_connector(int fd, uint32_t connector_id,
			   struct kms_connector *info)
{
	struct drm_mode_get_connector connector = {
		.connector_id = connector_id,
	};
	int attempt;

	for (attempt = 0; attempt < KMS_MAX_RETRY; attempt++) {
		connector.count_modes = info->capacity_modes;
		connector.count_encoders = info->capacity_encoders;
		connector.count_props = info->capacity_props;
		connector.modes_ptr = (uintptr_t)info->modes;
		connector.encoders_ptr = (uintptr_t)info->encoder_ids;
		connector.props_ptr = (uintptr_t)info->prop_ids;
		connector.prop_values_ptr = (uintptr_t)info->prop_values;

		if (drm_call(fd, DRM_IOCTL_MODE_GETCONNECTOR, &connector,
			     "DRM_IOCTL_MODE_GETCONNECTOR"))
			return -1;

		if (connector.count_modes <= info->capacity_modes &&
		    connector.count_encoders <= info->capacity_encoders &&
		    connector.count_props <= info->capacity_props)
			break;

		if (reserve_modes(&info->modes, &info->capacity_modes,
				  connector.count_modes) ||
		    reserve_u32(&info->encoder_ids, &info->capacity_encoders,
				connector.count_encoders, "connector encoder") ||
		    reserve_props(info, connector.count_props))
			return -1;
	}

	if (attempt == KMS_MAX_RETRY) {
		fprintf(stderr, "connector %" PRIu32 " keeps growing\n",
			connector_id);
		return -1;
	}

	info->connection = connector.connection;
	info->encoder_id = connector.encoder_id;
	info->count_modes = connector.count_modes;
	info->count_encoders = connector.count_encoders;
	info->count_props = connector.count_props;
	return 0;
}

static int choose_crtc(int fd, const struct kms_resources *resources,
		       uint32_t encoder_id, uint32_t *crtc_id)
{
	struct drm_mode_get_encoder encoder = {
		.encoder_id = encoder_id,
	};
	uint32_t index;

	if (drm_call(fd, DRM_IOCTL_MODE_GETENCODER, &encoder,
		     "DRM_IOCTL_MODE_GETENCODER"))
		return -1;

	if (encoder.crtc_id) {
		*crtc_id = encoder.crtc_id;
		return 0;
	}

	for (index = 0; index < resources->count_crtcs; index++) {
		if (encoder.possible_crtcs & (1U << index)) {
			*crtc_id = resources->crtc_ids[index];
			return 0;
		}
	}

	fprintf(stderr, "connected encoder has no usable CRTC\n");
	return -1;
}

static int try_connector(int fd, const struct kms_resources *resources,
			 uint32_t connector_id, struct kms_target *target)
{
	struct kms_connector connector = { 0 };
	uint32_t encoder_id;
	uint32_t mode_index;
	int result = -1;

	if (fetch_connector(fd, connector_id, &connector))
		goto out;

	if (connector.connection != DRM_CONNECTOR_STATUS_CONNECTED ||
	    !connector.count_modes || !connector.count_encoders)
		goto out;

	encoder_id = connector.encoder_id ? connector.encoder_id
					  : connector.encoder_ids[0];
	if (choose_crtc(fd, resources, encoder_id, &target->crtc_id))
		goto out;

	target->connector_id = connector_id;
	target->mode = connector.modes[0];
	for (mode_index = 0; mode_index < connector.count_modes; mode_index++) {
		if (connector.modes[mode_index].type & DRM_MODE_TYPE_PREFERRED) {
			target->mode = connector.modes[mode_index];
			break;
		}
	}

	printf("connector=%" PRIu32 " crtc=%" PRIu32 " mode=%ux%u@%u\n",
	       target->connector_id, target->crtc_id, target->mode.hdisplay,
	       target->mode.vdisplay, target->mode.vrefresh);
	result = 0;
out:
	free_kms_connector(&connector);
	return result;
}

static int find_connected_target(int fd, struct kms_target *target)
{
	struct kms_resources resources = { 0 };
	uint32_t connector_index;
	int result = -1;

	if (fetch_resources(fd, &resources))
		goto out;

	if (!resources.count_connectors || !resources.count_crtcs) {
		fprintf(stderr, "DRM has no connectors or CRTCs\n");
		goto out;
	}

	for (connector_index = 0; connector_index < resources.count_connectors;
	     connector_index++) {
		if (!try_connector(fd, &resources,
				   resources.connector_ids[connector_index],
				   target)) {
			result = 0;
			break;
		}
	}

	if (result)
		fprintf(stderr, "no connected connector with a usable CRTC\n");
out:
	free_kms_resources(&resources);
	return result;
}

static uint32_t parse_colour(const char *name)
{
	if (!strcmp(name, "black"))
		return 0x00000000;
	if (!strcmp(name, "red"))
		return 0x00ff0000;
	if (!strcmp(name, "green"))
		return 0x0000ff00;
	if (!strcmp(name, "blue"))
		return 0x000000ff;
	if (!strcmp(name, "white"))
		return 0x00ffffff;

	fprintf(stderr, "unknown colour: %s\n", name);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	struct kms_target target = { 0 };
	struct drm_mode_create_dumb create = { 0 };
	struct drm_mode_destroy_dumb destroy = { 0 };
	struct drm_mode_map_dumb map = { 0 };
	struct drm_mode_fb_cmd framebuffer = { 0 };
	struct drm_mode_crtc previous = { 0 };
	struct drm_mode_crtc set = { 0 };
	uint32_t colour = 0x00ff0000;
	uint32_t *pixels = MAP_FAILED;
	uint32_t row;
	uint32_t column;
	uint32_t framebuffer_id = 0;
	int fd = -1;
	int seconds = DEFAULT_SECONDS;
	bool hold = false;
	int argument;
	int exit_status = EXIT_FAILURE;

	for (argument = 1; argument < argc; argument++) {
		if (!strcmp(argv[argument], "--hold")) {
			hold = true;
		} else if (!strcmp(argv[argument], "--seconds")) {
			char *end = NULL;

			if (++argument == argc) {
				usage(argv[0]);
				return EXIT_FAILURE;
			}
			seconds = (int)strtol(argv[argument], &end, 10);
			if (!end || *end || seconds < 1) {
				fprintf(stderr, "invalid duration: %s\n", argv[argument]);
				return EXIT_FAILURE;
			}
		} else if (argv[argument][0] == '-') {
			usage(argv[0]);
			return EXIT_FAILURE;
		} else {
			colour = parse_colour(argv[argument]);
		}
	}

	fd = open(CARD_PATH, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "open %s: %s\n", CARD_PATH, strerror(errno));
		goto out;
	}

	if (find_connected_target(fd, &target))
		goto out;

	previous.crtc_id = target.crtc_id;
	if (drm_call(fd, DRM_IOCTL_MODE_GETCRTC, &previous,
		     "DRM_IOCTL_MODE_GETCRTC"))
		goto out;

	create.width = target.mode.hdisplay;
	create.height = target.mode.vdisplay;
	create.bpp = 32;
	if (drm_call(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create,
		     "DRM_IOCTL_MODE_CREATE_DUMB"))
		goto out;

	framebuffer.width = create.width;
	framebuffer.height = create.height;
	framebuffer.pitch = create.pitch;
	framebuffer.bpp = 32;
	framebuffer.depth = 24;
	framebuffer.handle = create.handle;
	if (drm_call(fd, DRM_IOCTL_MODE_ADDFB, &framebuffer,
		     "DRM_IOCTL_MODE_ADDFB"))
		goto out_destroy_dumb;
	framebuffer_id = framebuffer.fb_id;

	map.handle = create.handle;
	if (drm_call(fd, DRM_IOCTL_MODE_MAP_DUMB, &map,
		     "DRM_IOCTL_MODE_MAP_DUMB"))
		goto out_remove_fb;

	pixels = mmap(NULL, create.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
		      map.offset);
	if (pixels == MAP_FAILED) {
		fprintf(stderr, "mmap dumb buffer: %s\n", strerror(errno));
		goto out_remove_fb;
	}

	for (row = 0; row < create.height; row++) {
		uint32_t *line = (uint32_t *)((uint8_t *)pixels + row * create.pitch);

		for (column = 0; column < create.width; column++)
			line[column] = colour;
	}

	set.crtc_id = target.crtc_id;
	set.fb_id = framebuffer_id;
	set.set_connectors_ptr = (uintptr_t)&target.connector_id;
	set.count_connectors = 1;
	set.mode = target.mode;
	set.mode_valid = 1;
	if (drm_call(fd, DRM_IOCTL_MODE_SETCRTC, &set,
		     "DRM_IOCTL_MODE_SETCRTC"))
		goto out_unmap;

	printf("solid colour active on %s; press Ctrl-C or wait %s\n", CARD_PATH,
	       hold ? "for signal" : "for timeout");
	signal(SIGINT, stop_handler);
	signal(SIGTERM, stop_handler);
	while (!stop && (hold || seconds-- > 0))
		sleep(1);

	if (previous.mode_valid && previous.fb_id) {
		previous.set_connectors_ptr = (uintptr_t)&target.connector_id;
		previous.count_connectors = 1;
		if (drm_call(fd, DRM_IOCTL_MODE_SETCRTC, &previous,
			     "restore DRM_IOCTL_MODE_SETCRTC"))
			goto out_unmap;
	}

	exit_status = EXIT_SUCCESS;
out_unmap:
	if (pixels != MAP_FAILED)
		munmap(pixels, create.size);
out_remove_fb:
	if (framebuffer_id)
		drm_call(fd, DRM_IOCTL_MODE_RMFB, &framebuffer_id,
			 "DRM_IOCTL_MODE_RMFB");
out_destroy_dumb:
	if (create.handle) {
		destroy.handle = create.handle;
		drm_call(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy,
			 "DRM_IOCTL_MODE_DESTROY_DUMB");
	}
out:
	if (fd >= 0)
		close(fd);
	return exit_status;
}
