#include "common.h"

#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#include <drm_fourcc.h>

#include "intel_bufmgr.h"

static drm_intel_bufmgr *bufmgr;

//#define TILING DRM_FORMAT_MOD_LINEAR
//#define TILING I915_FORMAT_MOD_X_TILED
#define TILING I915_FORMAT_MOD_Y_TILED

int drm_open(const char *drm_device)
{
	int fd;
	uint64_t has_dumb;

	fd = open(drm_device, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "cannot open '%s': %m\n", drm_device);
		return -errno;
	}

	if (drmGetCap(fd, DRM_CAP_DUMB_BUFFER, &has_dumb) < 0 ||
	    !has_dumb) {
		fprintf(stderr, "drm device '%s' does not support dumb buffers\n",
				drm_device);
		close(fd);
		return -EOPNOTSUPP;
	}

	return fd;
}

void drm_close(int fd)
{
	drm_intel_bufmgr_destroy(bufmgr);
	close(fd);
}

static void _delete_buffer(int drm_fd, struct modeset_buf *buf)
{
	drm_intel_gem_bo_unmap_gtt(buf->bo);
	drm_intel_bo_unreference(buf->bo);
}

void drm_cleanup(struct modeset_dev *list)
{
	while (list) {
		struct modeset_dev *it = list;
		unsigned i;

		list = it->next;

		/* restore previous CRTC state */
		drmModeSetCrtc(it->drm_fd, it->saved_crtc->crtc_id,
					   it->saved_crtc->buffer_id, it->saved_crtc->x,
					   it->saved_crtc->y, &it->conn, 1, &it->saved_crtc->mode);
		drmModeFreeCrtc(it->saved_crtc);

		for (i = 0; i < (sizeof(list->buffers) / sizeof(list->buffers[0])); i++) {
			drmModeRmFB(it->drm_fd, it->buffers[i].fb);
			_delete_buffer(it->drm_fd, &it->buffers[i]);
		}

		_delete_buffer(it->drm_fd, &it->cursor);

		free(it);
	}
}

static bool check_crtc_is_unused(struct modeset_dev *list, uint32_t crtc)
{
	struct modeset_dev *iter;

	for (iter = list; iter; iter = iter->next) {
		if (iter->crtc == crtc) {
			return false;
		}
	}

	return true;
}

static int _find_crtc(struct modeset_dev *list, drmModeRes *res, drmModeConnector *conn, struct modeset_dev *dev)
{
	drmModeEncoder *enc;
	int i;

	if (conn->encoder_id)
		enc = drmModeGetEncoder(dev->drm_fd, conn->encoder_id);
	else
		enc = NULL;

	if (enc) {
		if (enc->crtc_id && check_crtc_is_unused(list, enc->crtc_id)) {
			dev->crtc = enc->crtc_id;
			drmModeFreeEncoder(enc);
			return 0;
		}

		drmModeFreeEncoder(enc);
	}

	for (i = 0; i < conn->count_encoders; i++) {
		int j;

		enc = drmModeGetEncoder(dev->drm_fd, conn->encoders[i]);
		if (!enc) {
			printf("cannot retrieve encoder %u:%u (%d): %m\n", i,
				   conn->encoders[i], errno);
			continue;
		}

		for (j = 0; j < res->count_crtcs; j++) {
			if (!(enc->possible_crtcs & (1 << j)))
				continue;

			if (check_crtc_is_unused(list, res->crtcs[j])) {
				dev->crtc = res->crtcs[j];
				drmModeFreeEncoder(enc);
				return 0;
			}
		}

		drmModeFreeEncoder(enc);
	}

	fprintf(stderr, "cannot find suitable CRTC for connector %u\n",
			conn->connector_id);
	return -ENOENT;
}

static int _create_buffer(struct modeset_dev *dev, struct modeset_buf *buf,
			  uint32_t w, uint32_t h, bool change_buffer_to_fb, uint64_t tiling)
{
	uint32_t handles[4] = {0}, pitches[4] = {0}, offsets[4] = {0};
	uint64_t modifiers[4] = {0};
	unsigned long stride, size;
	drm_intel_bo *bo;
	int ret;

	stride = w * 32;
	size = stride * h;

	if (tiling == DRM_FORMAT_MOD_LINEAR) {
		printf("DRM_FORMAT_MOD_LINEAR\n");
		bo = drm_intel_bo_alloc(bufmgr, "buffer", size, 0);
	} else {
		uint32_t t;

		stride = 0;

		switch (tiling) {
		case I915_FORMAT_MOD_X_TILED:
			printf("I915_FORMAT_MOD_X_TILED\n");
			t = 1;
			break;
		case I915_FORMAT_MOD_Y_TILED:
			printf("I915_FORMAT_MOD_Y_TILED\n");
			t = 2;
			break;
		default:
			fprintf(stderr, "tiling not handled yet\n");
			t = 0;
		}

		bo = drm_intel_bo_alloc_tiled(bufmgr, "buffer tiled", w, h, 4, &t, &stride, 0);
		printf("tiled buffer stride=%lu\n", stride);
	}

	if (!bo) {
		fprintf(stderr, "cannot create buffer (%d): %m\n", errno);
		return -errno;
	}
	buf->stride = stride;
	buf->size = bo->size;
	buf->handle = bo->handle;
	buf->width = w;
	buf->height = h;
	buf->bo = bo;

	if (change_buffer_to_fb) {
		handles[0] = buf->handle;
		pitches[0] = buf->stride;
		modifiers[0] = TILING;

		ret = drmModeAddFB2WithModifiers(dev->drm_fd, buf->width, buf->height,
						 DRM_FORMAT_XRGB8888, handles, pitches, offsets,
						 modifiers, &buf->fb, DRM_MODE_FB_MODIFIERS);
		if (ret) {
			fprintf(stderr, "cannot create framebuffer (%d): %m\n", errno);
			ret = -errno;
			goto err_map_to_fb;
		}
	}

	ret = drm_intel_gem_bo_map_gtt(bo);
	if (ret) {
		fprintf(stderr, "cannot map buffer (%d): %m\n", errno);
		ret = -errno;
		goto err_mmap;
	}
	buf->map = bo->virtual;

	memset(buf->map, 0x77, buf->size);
	return 0;

err_mmap:
	if (change_buffer_to_fb)
		drmModeRmFB(dev->drm_fd, buf->fb);
err_map_to_fb:
	drm_intel_bo_unreference(bo);
	return ret;
}

static int _create_fbs(struct modeset_dev *dev)
{
	unsigned i;

	for (i = 0; i < (sizeof(dev->buffers) / sizeof(dev->buffers[0])); i++) {
		if (_create_buffer(dev, &dev->buffers[i], dev->mode.hdisplay, dev->mode.vdisplay, true, TILING))
			return -1;
	}

	if (_create_buffer(dev, &dev->cursor, 64, 64, false, DRM_FORMAT_MOD_LINEAR))
		return -1;

	return 0;
}

#define WIDTH 1024

static int _setup_conn(struct modeset_dev *list, drmModeRes *res, drmModeConnector *conn, struct modeset_dev *dev)
{
	int i;
	drmModeModeInfoPtr found;

	if (conn->connection != DRM_MODE_CONNECTED) {
		printf("ignoring unused connector %u\n", conn->connector_id);
		return -ENOENT;
	}

	if (conn->count_modes == 0) {
		printf("no valid mode for connector %u\n", conn->connector_id);
		return -EFAULT;
	}

	if (dev->mode.clock)
		goto jump_lookup;

	found = &conn->modes[0];
	printf("Modes found: %d\n", conn->count_modes);
	for (i = 0; i < conn->count_modes; i++) {
		printf("\t%ux%ux@%d\n", conn->modes[i].hdisplay, conn->modes[i].vdisplay, conn->modes[i].vrefresh);
		if (conn->modes[i].hdisplay == WIDTH) {
			found = &conn->modes[i];
			printf("\tpicking this mode\n");
			break;
		}
	}
	/* copy the mode information into our device structure */
	memcpy(&dev->mode, found, sizeof(dev->mode));

jump_lookup:
	printf("mode for connector %u is %ux%u refresh %d\n", conn->connector_id,
			dev->mode.hdisplay, dev->mode.vdisplay, dev->mode.vrefresh);

	/* find a crtc for this connector */
	if (_find_crtc(list, res, conn, dev)) {
		printf("no valid crtc for connector %u\n", conn->connector_id);
		return -1;
	}

	/* create a framebuffer for this CRTC */
	if (_create_fbs(dev)) {
		printf("cannot create framebuffer for connector %u\n", conn->connector_id);
		return -1;
	}

	return 0;
}

static void modeset_dev_append(struct modeset_dev **list, struct modeset_dev *item)
{
	struct modeset_dev *l = *list;

	if (!l) {
		*list = item;
		return;
	}

	while (l->next)
		l = l->next;

	l->next = item;
}

struct modeset_dev *drm_modeset_with_mode(int fd, const drmModeModeInfo *mode)
{
	drmModeRes *res;
	int i;
	struct modeset_dev *list = NULL, *iter;

	res = drmModeGetResources(fd);
	if (!res) {
			fprintf(stderr, "cannot retrieve DRM resources (%d): %m\n", errno);
			return NULL;
	}

	for (i = 0; i < res->count_connectors; i++) {
		drmModeConnector *conn;
		struct modeset_dev *dev;

		conn = drmModeGetConnector(fd, res->connectors[i]);
		if (!conn) {
			fprintf(stderr, "cannot retrieve DRM connector %u:%u (%d): %m\n",
					i, res->connectors[i], errno);
			continue;
		}

		dev = calloc(1, sizeof(*dev));
		if (!dev) {
			fprintf(stderr, "cannot allocate modeset_dev for connector %u:%u (%d): %m\n",
					i, res->connectors[i], errno);
			continue;
		}

		dev->conn = conn->connector_id;
		dev->drm_fd = fd;
		if (mode)
			memcpy(&dev->mode, mode, sizeof(*mode));
		if (_setup_conn(list, res, conn, dev)) {
			free(dev);
			drmModeFreeConnector(conn);
			continue;
		}

		drmModeFreeConnector(conn);
		modeset_dev_append(&list, dev);
	}

	drmModeFreeResources(res);

	for (iter = list; iter; iter = iter->next) {
		int ret;

		iter->saved_crtc = drmModeGetCrtc(iter->drm_fd, iter->crtc);
		iter->buffers[0].frontbuffer = true;
		ret = drmModeSetCrtc(iter->drm_fd, iter->crtc, iter->buffers[0].fb, 0, 0,
							 &iter->conn, 1, &iter->mode);
		if (ret)
			fprintf(stderr, "cannot set CRTC for connector %u (%d): %m\n",
					iter->conn, errno);
		else
			iter->enabled = true;
	}

	return list;
}

struct modeset_dev *drm_modeset(int fd)
{
	int ret = drmIoctl(fd, DRM_IOCTL_SET_MASTER, NULL);

	if (ret) {
		fprintf(stderr, "Not able to turn into master | ret=%i errno=%i\n", ret, errno);
		return NULL;
	}

	bufmgr = drm_intel_bufmgr_gem_init(fd, 4096);
	if (!bufmgr) {
		fprintf(stderr, "Unable to initialize drm_intel_bufmgr_gem_init() | errno=%i\n", errno);
		return NULL;
	}

	drm_intel_bufmgr_gem_enable_reuse(bufmgr);
	return drm_modeset_with_mode(fd, NULL);
}
