#include <errno.h>
#include <stdio.h>
#include <sys/timerfd.h>
#include <poll.h>
#include <unistd.h>

#include "common.h"

#define BOX_SIZE 100
#define INCREMENT (BOX_SIZE / 3)

#define NSEC_PER_SEC 1000000000ULL

static void move_box(struct modeset_dev *list)
{
	struct modeset_dev *iter;
	static uint32_t box_x_begin = 0;
	static uint32_t box_y_begin = 0;
	uint32_t y, box_y_start, box_y_end, box_x_start, box_x_end;

	printf("move box\n");

	if (box_x_begin + BOX_SIZE > list->buffers->width) {
		box_x_begin = 0;
		box_y_begin += INCREMENT;

		if (box_y_begin + BOX_SIZE > list->buffers->height) {
			box_y_begin = 0;
		}
	} else {
		box_x_begin += INCREMENT;
	}

	box_y_start = box_y_begin;
	box_y_end = box_y_start + BOX_SIZE;
	box_x_start = box_x_begin;
	box_x_end = box_x_start + BOX_SIZE;

	for (iter = list; iter; iter = iter->next) {
		struct modeset_buf *buf = iter->buffers;

		for (y = 0; y < buf->height; y++) {
			uint32_t x;
			uint32_t line_offset = buf->stride * y;

			for (x = 0; x < buf->width; x++) {
				// 32bpp = 4bytes
				uint32_t pixel_offset = line_offset + (x * 4);
				struct pixel *p = (struct pixel *)&(buf->map[pixel_offset]);
				p->red = 0;
				p->green = 0;

				if (y >= box_y_start && y < box_y_end && x >= box_x_start
					&& x < box_x_end)
					p->blue = 0;
				else
					p->blue = 255;
			}
		}

		drmModeDirtyFB(iter->drm_fd, iter->buffers->fb, NULL, 0);
	}
}

int main()
{
	int fd, timerfd, r;
	struct modeset_dev *list, *iter;
	struct itimerspec new_value;
	struct pollfd pollfds[1];

	fd = drm_open(DEFAULT_DRM_DEVICE);
	if (fd < 0) {
		return -1;
	}

	list = drm_modeset(fd);

	// draw blue in all screens
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
				p->green = 0;
				p->blue = 255;
			}
		}

		drmModeDirtyFB(iter->drm_fd, iter->buffers->fb, NULL, 0);
	}

	timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	new_value.it_value.tv_nsec = NSEC_PER_SEC / 5;
	new_value.it_value.tv_sec = 0;
	new_value.it_interval.tv_nsec = new_value.it_value.tv_nsec;
	new_value.it_interval.tv_sec = new_value.it_value.tv_sec;
	r = timerfd_settime(timerfd, 0, &new_value, NULL);

	pollfds[0].fd = timerfd;
	pollfds[0].events = POLLIN | POLLPRI | POLLOUT | POLLERR | POLLHUP | POLLNVAL;
	pollfds[0].revents = 0;

	while (1) {
		uint64_t exp;

		r = poll(pollfds, 1, -1);
		if (r <= 0) {
			printf("poll returned r=%i, breaking\n", r);
			break;
		}

		if (pollfds[0].revents & POLLIN) {
			r = read(pollfds[0].fd, &exp, sizeof(exp));

			if (r != sizeof(uint64_t))
				printf("read a not expected number of bytes: %i\n", r);
			if (exp)
				move_box(list);
			if (exp > 1)
				printf("events missed: %lu\n", exp - 1);
		} else {
			printf("pollfds[0].revents=%d\n", pollfds[0].revents);
		}
	}

	drm_cleanup(list);
	drm_close(fd);

	return 0;
}
