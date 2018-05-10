#include <errno.h>
#include <stdio.h>

#include "common.h"

int main()
{
	int fd;
	struct modeset_dev *list, *iter;

	fd = drm_open(DEFAULT_DRM_DEVICE);
	if (fd < 0) {
		return -1;
	}

	list = drm_modeset(fd);

	// half screen blue half screen green
	for (iter = list; iter; iter = iter->next) {
		struct modeset_buf *buf = iter->buffers;
		uint32_t y;

		for (y = 0; y < buf->height; y++) {
			uint32_t x;
			uint32_t line_offset = buf->stride * y;

			for (x = 0; x < buf->width; x++) {
				// 32bpp = 4bytes
				uint32_t pixel_offset = line_offset + (x * 4);
				struct pixel *p = (struct pixel *)&(buf->map[pixel_offset]);
				p->red = 0;
				p->green = x >= (buf->width / 2) ? 255 : 0;
				p->blue = x < (buf->width / 2) ? 255 : 0;
			}
		}
	}

	// white cursor
	for (iter = list; iter; iter = iter->next) {
		struct modeset_buf *cursor = &iter->cursor;
		uint32_t y;

		for (y = 0; y < cursor->height; y++) {
			uint32_t x;
			uint32_t line_offset = cursor->stride * y;

			for (x = 0; x < cursor->width; x++) {
				// 32bpp = 4bytes
				uint32_t pixel_offset = line_offset + (x * 4);
				struct pixel *p = (struct pixel *)&(cursor->map[pixel_offset]);
				p->red = 255;
				p->green = 255;
				p->blue = 255;
				p->pad_or_alpha = 255;
			}
		}

		drmModeSetCursor(iter->drm_fd, iter->crtc, cursor->handle, cursor->width, cursor->height);
		drmModeMoveCursor(iter->drm_fd, iter->crtc, 100, 100);
	}
	printf("Full red screens with a white cursor\n");
	printf("Press enter to continue...\n");
	getchar();

	for (iter = list; iter; iter = iter->next) {
		struct modeset_buf *cursor = &iter->cursor;
		uint32_t y;

		for (y = 0; y < cursor->height; y++) {
			uint32_t x;
			uint32_t line_offset = cursor->stride * y;

			for (x = 0; x < cursor->width; x++) {
				// 32bpp = 4bytes
				uint32_t pixel_offset = line_offset + (x * 4);
				struct pixel *p = (struct pixel *)&(cursor->map[pixel_offset]);
				p->red = 0;
				p->green = 255;
				p->blue = 0;
				p->pad_or_alpha = 255;
			}
		}
	}
	printf("Green cursor\n");
	printf("Press enter to continue...\n");
	getchar();

	for (iter = list; iter; iter = iter->next) {
		struct modeset_buf *cursor = &iter->cursor;
		uint32_t y;

		for (y = 0; y < cursor->height; y++) {
			uint32_t x;
			uint32_t line_offset = cursor->stride * y;

			for (x = 0; x < cursor->width; x++) {
				// 32bpp = 4bytes
				uint32_t pixel_offset = line_offset + (x * 4);
				struct pixel *p = (struct pixel *)&(cursor->map[pixel_offset]);
				p->red = 0;
				p->green = 0;
				p->blue = 255;
				p->pad_or_alpha = 255;
			}
		}
	}
	printf("Blue cursor\n");
	printf("Press enter to continue...\n");
	getchar();

	for (iter = list; iter; iter = iter->next) {
		struct modeset_buf *cursor = &iter->cursor;
		uint32_t y;

		for (y = 0; y < cursor->height; y++) {
			uint32_t x;
			uint32_t line_offset = cursor->stride * y;

			for (x = 0; x < cursor->width; x++) {
				// 32bpp = 4bytes
				uint32_t pixel_offset = line_offset + (x * 4);
				struct pixel *p = (struct pixel *)&(cursor->map[pixel_offset]);
				p->red = 0;
				p->green = 0;
				p->blue = 255;
				p->pad_or_alpha = 125;
			}
		}
	}
	printf("Blue cursor with 50%% of transparency\n");
	printf("Press enter to continue...\n");
	getchar();

	for (iter = list; iter; iter = iter->next) {
		drmModeMoveCursor(iter->drm_fd, iter->crtc, iter->buffers[0].width / 2 + 100, 100);
	}
	printf("Cursor moved to other half of screen\n");
	printf("Press enter to continue...\n");
	getchar();

	drm_cleanup(list);
	drm_close(fd);

	return 0;
}
