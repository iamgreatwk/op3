// SPDX-License-Identifier: MIT
/*
 * Small libdrm-free direct KMS backend for recovery_mainline.
 *
 * The existing OP3 DRM gate uses the same UAPI sequence.  Keeping this code
 * separate from the terminal renderer makes the display boundary explicit:
 * recovery owns card0 only while this object is open, and closing it releases
 * DRM ownership before another compositor is started.
 */

#include "recovery_drm.h"

#include <drm/drm.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#define CARD_PATH "/dev/dri/card0"
#define KMS_MAX_RETRY 4
#define DRM_CONNECTOR_STATUS_CONNECTED 1

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

static int drm_call(int fd, unsigned long request, void *argument,
			    const char *name)
{
	if (ioctl(fd, request, argument) == 0)
		return 0;

	fprintf(stderr, "recovery-drm: %s: %s\n", name, strerror(errno));
	return -1;
}

static int reserve_u32(uint32_t **array, uint32_t *capacity, uint32_t count,
			       const char *name)
{
	uint32_t *grown;

	if (count <= *capacity)
		return 0;

	grown = realloc(*array, (size_t)count * sizeof(*grown));
	if (!grown) {
		fprintf(stderr, "recovery-drm: cannot allocate %s list (%" PRIu32 ")\n",
			name, count);
		return -1;
	}

	*array = grown;
	*capacity = count;
	return 0;
}

static int reserve_modes(struct drm_mode_modeinfo **array, uint32_t *capacity,
				 uint32_t count)
{
	struct drm_mode_modeinfo *grown;

	if (count <= *capacity)
		return 0;

	grown = realloc(*array, (size_t)count * sizeof(*grown));
	if (!grown) {
		fprintf(stderr, "recovery-drm: cannot allocate mode list (%" PRIu32 ")\n",
			count);
		return -1;
	}

	*array = grown;
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
		fprintf(stderr, "recovery-drm: cannot allocate property ids\n");
		return -1;
	}
	connector->prop_ids = ids;

	values = realloc(connector->prop_values,
				(size_t)count * sizeof(*values));
	if (!values) {
		fprintf(stderr, "recovery-drm: cannot allocate property values\n");
		return -1;
	}
	connector->prop_values = values;

	connector->capacity_props = count;
	return 0;
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

static int fetch_resources(int fd, struct kms_resources *resources)
{
	struct drm_mode_card_res card;
	int attempt;

	for (attempt = 0; attempt < KMS_MAX_RETRY; attempt++) {
		memset(&card, 0, sizeof(card));
		card.count_fbs = resources->capacity_fbs;
		card.count_crtcs = resources->capacity_crtcs;
		card.count_connectors = resources->capacity_connectors;
		card.count_encoders = resources->capacity_encoders;
		card.fb_id_ptr = (uintptr_t)resources->fb_ids;
		card.crtc_id_ptr = (uintptr_t)resources->crtc_ids;
		card.connector_id_ptr = (uintptr_t)resources->connector_ids;
		card.encoder_id_ptr = (uintptr_t)resources->encoder_ids;

		if (drm_call(fd, DRM_IOCTL_MODE_GETRESOURCES, &card,
				     "GETRESOURCES"))
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
		fprintf(stderr, "recovery-drm: resource counts keep growing\n");
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
	struct drm_mode_get_connector connector;
	int attempt;

	for (attempt = 0; attempt < KMS_MAX_RETRY; attempt++) {
		memset(&connector, 0, sizeof(connector));
		connector.connector_id = connector_id;
		connector.count_modes = info->capacity_modes;
		connector.count_encoders = info->capacity_encoders;
		connector.count_props = info->capacity_props;
		connector.modes_ptr = (uintptr_t)info->modes;
		connector.encoders_ptr = (uintptr_t)info->encoder_ids;
		connector.props_ptr = (uintptr_t)info->prop_ids;
		connector.prop_values_ptr = (uintptr_t)info->prop_values;

		if (drm_call(fd, DRM_IOCTL_MODE_GETCONNECTOR, &connector,
				     "GETCONNECTOR"))
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
		fprintf(stderr, "recovery-drm: connector counts keep growing\n");
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
	struct drm_mode_get_encoder encoder;
	uint32_t index;

	memset(&encoder, 0, sizeof(encoder));
	encoder.encoder_id = encoder_id;
	if (drm_call(fd, DRM_IOCTL_MODE_GETENCODER, &encoder, "GETENCODER"))
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

	fprintf(stderr, "recovery-drm: connected encoder has no usable CRTC\n");
	return -1;
}

static int find_connected_target(int fd, uint32_t *connector_id,
					 uint32_t *crtc_id,
					 struct drm_mode_modeinfo *mode)
{
	struct kms_resources resources;
	uint32_t connector_index;
	int result = -1;

	memset(&resources, 0, sizeof(resources));
	if (fetch_resources(fd, &resources))
		goto out;
	if (!resources.count_connectors || !resources.count_crtcs) {
		fprintf(stderr, "recovery-drm: no connectors or CRTCs\n");
		goto out;
	}

	for (connector_index = 0;
	     connector_index < resources.count_connectors;
	     connector_index++) {
		struct kms_connector connector;
		uint32_t encoder_id;
		uint32_t mode_index;

		memset(&connector, 0, sizeof(connector));
		if (fetch_connector(fd, resources.connector_ids[connector_index],
					&connector)) {
			free_kms_connector(&connector);
			continue;
		}
		if (connector.connection != DRM_CONNECTOR_STATUS_CONNECTED ||
		    !connector.count_modes || !connector.count_encoders) {
			free_kms_connector(&connector);
			continue;
		}

		encoder_id = connector.encoder_id ? connector.encoder_id
						 : connector.encoder_ids[0];
		if (choose_crtc(fd, &resources, encoder_id, crtc_id)) {
			free_kms_connector(&connector);
			continue;
		}

		*connector_id = resources.connector_ids[connector_index];
		*mode = connector.modes[0];
		for (mode_index = 0; mode_index < connector.count_modes;
		     mode_index++) {
			if (connector.modes[mode_index].type & DRM_MODE_TYPE_PREFERRED) {
				*mode = connector.modes[mode_index];
				break;
			}
		}
		fprintf(stderr, "recovery-drm: connector=%" PRIu32
			" crtc=%" PRIu32 " mode=%ux%u@%u\n",
			*connector_id, *crtc_id, mode->hdisplay, mode->vdisplay,
			mode->vrefresh);
		free_kms_connector(&connector);
		result = 0;
		break;
	}

	if (result)
		fprintf(stderr, "recovery-drm: no connected usable connector\n");
out:
	free_kms_resources(&resources);
	return result;
}

static void destroy_buffer(struct recovery_drm_display *display)
{
	struct drm_mode_destroy_dumb destroy;
	uint32_t framebuffer_id;

	if (display->fd < 0)
		return;
	if (display->pixels && display->map_size)
		munmap(display->pixels, display->map_size);
	if (display->framebuffer_id) {
		framebuffer_id = display->framebuffer_id;
		(void)drm_call(display->fd, DRM_IOCTL_MODE_RMFB, &framebuffer_id,
				"RMFB");
	}
	if (display->handle) {
		memset(&destroy, 0, sizeof(destroy));
		destroy.handle = display->handle;
		(void)drm_call(display->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy,
				"DESTROY_DUMB");
	}
}

int recovery_drm_open(struct recovery_drm_display *display)
{
	struct drm_mode_create_dumb create;
	struct drm_mode_fb_cmd framebuffer;
	struct drm_mode_map_dumb map;
	uint32_t connector_id;
	uint32_t crtc_id;
	int fd;

	if (!display)
		return -1;
	memset(display, 0, sizeof(*display));
	display->fd = -1;

	fd = open(CARD_PATH, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "recovery-drm: open %s: %s\n", CARD_PATH,
			strerror(errno));
		return -1;
	}
	display->fd = fd;

	if (find_connected_target(fd, &connector_id, &crtc_id,
				  &display->mode))
		goto fail;

	memset(&create, 0, sizeof(create));
	create.width = display->mode.hdisplay;
	create.height = display->mode.vdisplay;
	create.bpp = 32;
	if (drm_call(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create, "CREATE_DUMB"))
		goto fail;
	display->handle = create.handle;
	display->width = create.width;
	display->height = create.height;
	display->pitch = create.pitch;
	display->map_size = create.size;

	memset(&framebuffer, 0, sizeof(framebuffer));
	framebuffer.width = create.width;
	framebuffer.height = create.height;
	framebuffer.pitch = create.pitch;
	framebuffer.bpp = 32;
	framebuffer.depth = 24;
	framebuffer.handle = create.handle;
	if (drm_call(fd, DRM_IOCTL_MODE_ADDFB, &framebuffer, "ADDFB"))
		goto fail;
	display->framebuffer_id = framebuffer.fb_id;

	memset(&map, 0, sizeof(map));
	map.handle = create.handle;
	if (drm_call(fd, DRM_IOCTL_MODE_MAP_DUMB, &map, "MAP_DUMB"))
		goto fail;
	display->pixels = mmap(NULL, create.size, PROT_READ | PROT_WRITE,
				      MAP_SHARED, fd, map.offset);
	if (display->pixels == MAP_FAILED) {
		display->pixels = NULL;
		fprintf(stderr, "recovery-drm: mmap dumb buffer: %s\n",
			strerror(errno));
		goto fail;
	}

	display->connector_id = connector_id;
	display->crtc_id = crtc_id;
	/* Leave the CRTC untouched until the caller has rendered the first frame.
	 * On the OP3 command-mode panel, the validated KMS probe fills the dumb
	 * buffer before SETCRTC; modesetting an uninitialized buffer and relying on
	 * a later DIRTYFB update can leave the panel black after a handoff. */
	return 0;

fail:
	recovery_drm_close(display);
	return -1;
}

int recovery_drm_activate(struct recovery_drm_display *display)
{
	struct drm_mode_crtc set;

	if (!display || display->fd < 0 || !display->framebuffer_id)
		return -1;

	memset(&set, 0, sizeof(set));
	set.crtc_id = display->crtc_id;
	set.fb_id = display->framebuffer_id;
	set.set_connectors_ptr = (uintptr_t)&display->connector_id;
	set.count_connectors = 1;
	set.mode = display->mode;
	set.mode_valid = 1;
	if (drm_call(display->fd, DRM_IOCTL_MODE_SETCRTC, &set, "SETCRTC"))
		return -1;

	display->crtc_active = 1;
	return 0;
}

int recovery_drm_present(struct recovery_drm_display *display)
{
	struct drm_mode_fb_dirty_cmd dirty;

	if (!display || display->fd < 0 || !display->framebuffer_id)
		return -1;
	memset(&dirty, 0, sizeof(dirty));
	dirty.fb_id = display->framebuffer_id;
	/* num_clips=0 means a full-plane update for drm_atomic_helper_dirtyfb. */
	if (drm_call(display->fd, DRM_IOCTL_MODE_DIRTYFB, &dirty, "DIRTYFB"))
		return -1;
	return 0;
}

void recovery_drm_close(struct recovery_drm_display *display)
{
	struct drm_mode_crtc disable;

	if (!display)
		return;
	if (display->fd >= 0 && display->crtc_id && display->crtc_active) {
		memset(&disable, 0, sizeof(disable));
		disable.crtc_id = display->crtc_id;
		(void)drm_call(display->fd, DRM_IOCTL_MODE_SETCRTC, &disable,
				"disable CRTC");
		display->crtc_active = 0;
	}
	destroy_buffer(display);
	if (display->fd >= 0)
		close(display->fd);
	memset(display, 0, sizeof(*display));
	display->fd = -1;
}
