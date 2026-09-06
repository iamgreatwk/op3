/* SPDX-License-Identifier: MIT */
#ifndef OP3_RECOVERY_DRM_H
#define OP3_RECOVERY_DRM_H

#include <stddef.h>
#include <stdint.h>

#include <drm/drm_mode.h>

struct recovery_drm_display {
	int fd;
	uint32_t connector_id;
	uint32_t crtc_id;
	struct drm_mode_modeinfo mode;
	uint32_t framebuffer_id;
	uint32_t handle;
	uint32_t pitch;
	uint32_t width;
	uint32_t height;
	size_t map_size;
	void *pixels;
};

int recovery_drm_open(struct recovery_drm_display *display);
int recovery_drm_activate(struct recovery_drm_display *display);
int recovery_drm_present(struct recovery_drm_display *display);
void recovery_drm_close(struct recovery_drm_display *display);

#endif
