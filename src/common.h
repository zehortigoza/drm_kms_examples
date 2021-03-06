#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include "intel_bufmgr.h"

#define UNUSED __attribute__((unused))

#define DEFAULT_DRM_DEVICE "/dev/dri/card0"

struct modeset_buf {
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	/* Size of the memory mapped buffer */
	uint32_t size;
	/* A DRM handle to the buffer object that we can draw into */
	uint32_t handle;
	/* Pointer to the memory mapped buffer */
	uint8_t *map;
	/* Framebuffer handle with our buffer object as scanout buffer */
	uint32_t fb;

	bool frontbuffer;
	drm_intel_bo *bo;
};

struct modeset_dev {
	struct modeset_dev *next;

	struct modeset_buf buffers[3];
	struct modeset_buf cursor;

	/* Display mode that we want to use */
	drmModeModeInfo mode;
	/* Connector ID that we want to use with this buffer */
	uint32_t conn;
	/* Crtc ID that we want to use with this connector */
	uint32_t crtc;
	/* Configuration of the crtc before we changed it. We use it so we can
	 * restore the same mode when we exit
	 */
	drmModeCrtc *saved_crtc;

	int drm_fd;
	bool enabled;
};

struct pixel {
	uint8_t blue;
	uint8_t green;
	uint8_t red;
	uint8_t pad_or_alpha;
};

int drm_open(const char *drm_device);
void drm_cleanup(struct modeset_dev *list);
void drm_close(int fd);

struct modeset_dev *drm_modeset(int fd);
struct modeset_dev *drm_modeset_with_mode(int fd, const drmModeModeInfo *mode);
