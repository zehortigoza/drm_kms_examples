#include <errno.h>
#include <stdio.h>

#include "common.h"

int main()
{
	int fd;
	struct modeset_dev *list, *iter;
	const drmModeModeInfo std_1024_mode = {
		.clock = 65000,
		.hdisplay = 1024,
		.hsync_start = 1048,
		.hsync_end = 1184,
		.htotal = 1344,
		.hskew = 0,
		.vdisplay = 768,
		.vsync_start = 771,
		.vsync_end = 777,
		.vtotal = 806,
		.vscan = 0,
		.vrefresh = 60,
		.flags = 0xA,
		.type = 0x40,
		.name = "Handmade 1024x768",
	};

	fd = drm_open(DEFAULT_DRM_DEVICE);
	if (fd < 0) {
		return -1;
	}

	list = drm_modeset_with_mode(fd, &std_1024_mode);

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

		drmModePageFlip(fd, iter->crtc, buf->fb, 0, NULL);
		buf->frontbuffer = true;
		iter->buffers[1].frontbuffer = false;
	}

	printf("Half blue and green screens\n");
	printf("Press enter to continue...\n");
	getchar();

	drm_cleanup(list);
	drm_close(fd);

	return 0;
}
