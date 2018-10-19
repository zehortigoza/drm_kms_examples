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
		struct modeset_buf *buf = &iter->buffers[1];
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

		drmModePageFlip(fd, iter->crtc, buf->fb, 0, NULL);
		buf->frontbuffer = true;
		iter->buffers[0].frontbuffer = false;
	}

	printf("Full red screens\n");
	printf("Press enter to continue...\n");
	getchar();

	// half screen blue half screen green
	for (iter = list; iter; iter = iter->next) {
		struct modeset_buf *buf = &iter->buffers[0];
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

		drmModePageFlip(fd, iter->crtc, buf->fb, 0, NULL);
		buf->frontbuffer = true;
		iter->buffers[1].frontbuffer = false;
	}

	printf("Pink box in the middle of screen\n");
	printf("Press enter to continue...\n");
	getchar();

	for (iter = list; iter; iter = iter->next) {
		struct modeset_buf *buf = &iter->buffers[1];
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

		drmModePageFlip(fd, iter->crtc, buf->fb, 0, NULL);
		buf->frontbuffer = true;
		iter->buffers[0].frontbuffer = false;
	}

	printf("Yellow box in the middle of screen\n");
	printf("Press enter to continue...\n");
	getchar();

	for (iter = list; iter; iter = iter->next) {
		struct modeset_buf *buf = &iter->buffers[0];
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

		drmModePageFlip(fd, iter->crtc, buf->fb, 0, NULL);
		buf->frontbuffer = true;
		iter->buffers[1].frontbuffer = false;
	}

	printf("Yellow box in the middle+%dpx of screen\n", BOX_SIZE);
	printf("Press enter to continue...\n");
	getchar();

	drm_cleanup(list);
	drm_close(fd);

	return 0;
}
