#include <errno.h>
#include <stdio.h>

#include "common.h"

#define BOX_SIZE 50

int main()
{
	int fd;
	struct modeset_dev *list, *iter;

	fd = drm_open(DEFAULT_DRM_DEVICE);
	if (fd < 0) {
		return -1;
	}

	list = drm_modeset(fd);

	// draw red in all screens
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
				p->red = 255;
				p->green = 0;
				p->blue = 0;
			}
		}

		drmModeDirtyFB(iter->drm_fd, iter->buffers->fb, NULL, 0);
	}

	printf("Full red screens\n");
	printf("Press enter to continue... 1/4\n");
	getchar();

	for (iter = list; iter; iter = iter->next) {
		struct modeset_buf *buf = iter->buffers;
		uint32_t y, box_y_start, box_y_end, box_x_start, box_x_end;

		box_y_start = (buf->height - BOX_SIZE) / 2;
		box_y_end = box_y_start + BOX_SIZE;
		box_x_start = (buf->width - BOX_SIZE) / 2;
		box_x_end = box_x_start + BOX_SIZE;

		for (y = 0; y < buf->height; y++) {
			uint32_t x;
			uint32_t line_offset = buf->stride * y;

			for (x = 0; x < buf->width; x++) {
				// 32bpp = 4bytes
				uint32_t pixel_offset = line_offset + (x * 4);
				struct pixel *p = (struct pixel *)&(buf->map[pixel_offset]);
				p->red = 255;
				p->green = 0;


				if (y > box_y_start && y < box_y_end && x > box_x_start
					&& x < box_x_end)
					p->blue = 255;
				else
					p->blue = 0;
			}
		}

		drmModeDirtyFB(iter->drm_fd, iter->buffers->fb, NULL, 0);
	}

	printf("Pink box in the middle of screen\n");
	printf("Press enter to continue... 2/4\n");
	getchar();

	for (iter = list; iter; iter = iter->next) {
		struct modeset_buf *buf = iter->buffers;
		uint32_t y, box_y_start, box_y_end, box_x_start, box_x_end;

		box_y_start = (buf->height - BOX_SIZE) / 2;
		box_y_end = box_y_start + BOX_SIZE;
		box_x_start = (buf->width - BOX_SIZE) / 2;
		box_x_end = box_x_start + BOX_SIZE;

		for (y = 0; y < buf->height; y++) {
			uint32_t x;
			uint32_t line_offset = buf->stride * y;

			for (x = 0; x < buf->width; x++) {
				// 32bpp = 4bytes
				uint32_t pixel_offset = line_offset + (x * 4);
				struct pixel *p = (struct pixel *)&(buf->map[pixel_offset]);
				p->red = 255;
				p->blue = 0;


				if (y > box_y_start && y < box_y_end && x > box_x_start
					&& x < box_x_end)
					p->green = 255;
				else
					p->green = 0;
			}
		}

		drmModeDirtyFB(iter->drm_fd, iter->buffers->fb, NULL, 0);
	}

	printf("Yellow box in the middle of screen\n");
	printf("Press enter to continue... 3/4\n");
	getchar();

	for (iter = list; iter; iter = iter->next) {
		struct modeset_buf *buf = iter->buffers;
		uint32_t y, box_y_start, box_y_end, box_x_start, box_x_end;

		box_y_start = (buf->height - BOX_SIZE) / 2;
		box_y_start += BOX_SIZE;
		box_y_end = box_y_start + BOX_SIZE;
		box_x_start = (buf->width - BOX_SIZE) / 2;
		box_x_start += BOX_SIZE;
		box_x_end = box_x_start + BOX_SIZE;

		for (y = 0; y < buf->height; y++) {
			uint32_t x;
			uint32_t line_offset = buf->stride * y;

			for (x = 0; x < buf->width; x++) {
				// 32bpp = 4bytes
				uint32_t pixel_offset = line_offset + (x * 4);
				struct pixel *p = (struct pixel *)&(buf->map[pixel_offset]);
				p->red = 255;
				p->blue = 0;


				if (y > box_y_start && y < box_y_end && x > box_x_start
					&& x < box_x_end)
					p->green = 255;
				else
					p->green = 0;
			}
		}

		drmModeDirtyFB(iter->drm_fd, iter->buffers->fb, NULL, 0);
	}

	printf("Yellow box in the middle+%dpx of screen\n", BOX_SIZE);
	printf("Press enter to continue... 4/4\n");
	getchar();

	drm_cleanup(list);
	drm_close(fd);

	return 0;
}
