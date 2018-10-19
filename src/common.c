#include "common.h"

#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

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
	close(fd);
}

static void _delete_buffer(int drm_fd, struct modeset_buf *buf)
{
	struct drm_mode_destroy_dumb dreq;

	munmap(buf->map, buf->size);

	memset(&dreq, 0, sizeof(dreq));
	dreq.handle = buf->handle;
	drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
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

static int _create_buffer(struct modeset_dev *dev, struct modeset_buf *buf, uint32_t w, uint32_t h, bool map_fb)
{
	struct drm_mode_create_dumb creq;
	struct drm_mode_map_dumb mreq;
	struct drm_mode_destroy_dumb dreq;
	int ret;

	memset(&creq, 0, sizeof(creq));
	creq.width = w;
	creq.height = h;
	creq.bpp = 32;

	ret = drmIoctl(dev->drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq);
	if (ret < 0) {
		fprintf(stderr, "cannot create dumb buffer (%d): %m\n", errno);
		return -errno;
	}
	buf->stride = creq.pitch;
	buf->size = creq.size;
	buf->handle = creq.handle;
	buf->width = creq.width;
	buf->height = creq.height;

	if (map_fb) {
		ret = drmModeAddFB(dev->drm_fd, buf->width, buf->height, 24, creq.bpp,
						   buf->stride, buf->handle, &buf->fb);
		if (ret) {
			fprintf(stderr, "cannot create framebuffer (%d): %m\n", errno);
			ret = -errno;
			goto err_map_to_fb;
		}
	}

	memset(&mreq, 0, sizeof(mreq));
	mreq.handle = buf->handle;
	ret = drmIoctl(dev->drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
	if (ret) {
		fprintf(stderr, "cannot map dumb buffer (%d): %m\n", errno);
		ret = -errno;
		goto err_mmap;
	}

	buf->map = mmap(0, buf->size, PROT_READ | PROT_WRITE, MAP_SHARED,
					dev->drm_fd, mreq.offset);
	if (buf->map == MAP_FAILED) {
		fprintf(stderr, "cannot mmap dumb buffer (%d): %m\n", errno);
		ret = -errno;
		goto err_mmap;
	}

	memset(buf->map, 0x77, buf->size);
	return 0;

err_mmap:
	if (map_fb)
		drmModeRmFB(dev->drm_fd, buf->fb);
err_map_to_fb:
	memset(&dreq, 0, sizeof(dreq));
	dreq.handle = buf->handle;
	drmIoctl(dev->drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
	return ret;
}

static int _create_fbs(struct modeset_dev *dev)
{
	unsigned i;

	for (i = 0; i < (sizeof(dev->buffers) / sizeof(dev->buffers[0])); i++) {
		if (_create_buffer(dev, &dev->buffers[i], dev->mode.hdisplay, dev->mode.vdisplay, true))
			return -1;
	}

	if (_create_buffer(dev, &dev->cursor, 64, 64, false))
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
	return drm_modeset_with_mode(fd, NULL);
}
