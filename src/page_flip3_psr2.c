#include <errno.h>
#include <stdio.h>
#include <sys/timerfd.h>
#include <poll.h>
#include <unistd.h>

#include "common.h"
#include "debugfs.h"

#define BOX_SIZE 100
#define INCREMENT (BOX_SIZE / 3)

#define NSEC_PER_SEC 1000000000ULL

static int psr_debugfs;

static void draw_frames(struct modeset_dev *list)
{
	const uint8_t buffers_count = sizeof(list->buffers) / sizeof(list->buffers[0]);
	struct modeset_dev *iter;

	for (iter = list; iter; iter = iter->next) {
		uint8_t i;

		for (i = 0; i < buffers_count; i++) {
			struct modeset_buf *buf = &iter->buffers[i];
			uint32_t y;
			uint32_t box_y_begin = (buf->height - BOX_SIZE) / 2;
			uint32_t box_y_end = box_y_begin + BOX_SIZE;

			for (y = 0; y < buf->height; y++) {
				uint32_t x;
				uint8_t draw_box_in_y;
				uint32_t line_offset = buf->stride * y;
				uint32_t box_x_begin, box_x_end;

				if (y > box_y_begin && y < box_y_end) {
					draw_box_in_y = 1;
					box_x_begin = (buf->width / buffers_count) * i;
					box_x_end = box_x_begin + BOX_SIZE;
				} else {
					draw_box_in_y = box_x_begin = box_x_end = 0;
				}

				for (x = 0; x < buf->width; x++) {
					// 32bpp = 4bytes
					uint32_t pixel_offset = line_offset + (x * 4);
					struct pixel *p = (struct pixel *)&(buf->map[pixel_offset]);

					p->red = 0;
					p->green = 0;

					if (draw_box_in_y && x > box_x_begin && x < box_x_end)
						p->blue = 0;
					else
						p->blue = 255;
				}
			}
		}
	}
}

static void psr_debugfs_parse()
{
	uint16_t count;
	uint8_t status, changed, first_status_printed = 0;
	char buffer[1024];

	for (count = 0, changed = 0; count < 512; count++) {
		int r;
		uint8_t sink_status;

		r = i915_psr_debugfs_read(psr_debugfs, buffer, sizeof(buffer));
		if (r < 0) {
			printf("Error reading debugfs\n");
			continue;
		}
		r = i915_psr_debugfs_read_source_status_id(buffer, &status);
		if (r < 0) {
			printf("Error getting source status\n");
			continue;
		}

		if (!first_status_printed) {
			printf("\tsource status initial=%s\n", i915_psr_debugfs_source_status_string_get(status));
			first_status_printed = 1;
		}

		if (!changed) {
			if (status == 3 || status == 8)
				continue;
			changed = 1;
		}

		printf("\tsource status=%s\n", i915_psr_debugfs_source_status_string_get(status));

		i915_psr_debugfs_got_su_entry(buffer, &sink_status);
		if (sink_status)
			printf("\tgo su entry\n");

		i915_psr_debugfs_got_su_blocks(buffer, &sink_status);
		if (sink_status)
			printf("\tgo su blocks\n");

		if (!i915_psr_debugfs_read_sink_status_id(buffer, &sink_status))
			printf("\tsink status=%s\n", i915_psr_debugfs_sink_status_string_get(sink_status));

		printf("\n");

		// sleep
		if (status == 3)
			break;

		// deep sleep
		if (status == 8)
			break;
	}

	if (count == 512)
		printf("\tcount=%d | status=%d\n", count, status);
}

static void flip_frame(struct modeset_dev *list, uint8_t *active_frame)
{
	const uint8_t buffers_count = sizeof(list->buffers) / sizeof(list->buffers[0]);
	struct modeset_dev *iter;
	uint8_t next_frame = (*active_frame) + 1;

	if (next_frame == buffers_count)
		next_frame = 0;

	for (iter = list; iter; iter = iter->next) {
		struct modeset_buf *buf = &iter->buffers[next_frame];

		drmModePageFlip(iter->drm_fd, iter->crtc, buf->fb, 0, NULL);
		buf->frontbuffer = true;
		iter->buffers[*active_frame].frontbuffer = false;
	}
	*active_frame = next_frame;

	printf("flip_frame\n");
	psr_debugfs_parse();
}

int main()
{
	int fd, timerfd, r;
	struct modeset_dev *list;
	struct itimerspec new_value;
	struct pollfd pollfds[1];
	uint8_t active_frame = 0;

	fd = drm_open(DEFAULT_DRM_DEVICE);
	if (fd < 0) {
		return -1;
	}

	list = drm_modeset(fd);
	psr_debugfs = i915_psr_debugfs_read_init();
	if (psr_debugfs < 0)
		goto end;

	draw_frames(list);

	timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	//new_value.it_value.tv_nsec = NSEC_PER_SEC / 15;
	new_value.it_value.tv_nsec = NSEC_PER_SEC / 10;
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
				flip_frame(list, &active_frame);
			if (exp > 1)
				printf("events missed: %lu\n", exp - 1);
		} else {
			printf("pollfds[0].revents=%d\n", pollfds[0].revents);
		}
	}

	i915_psr_debugfs_shutdown(psr_debugfs);
end:
	drm_cleanup(list);
	drm_close(fd);

	return 0;
}
