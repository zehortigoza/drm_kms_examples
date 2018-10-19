#include <stdio.h>
#include <signal.h>
#include <stdint.h>

#include "debugfs.h"

static volatile uint8_t run;

static void signal_handler(int sig)
{
	printf("Got signal=%i\n", sig);
	run = 0;
}

int main()
{
	int file_fd;

	signal(SIGINT, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGTERM, signal_handler);

	file_fd = i915_psr_debugfs_read_init();
	if (file_fd < 0)
		return file_fd;

	run = 1;

	while (run) {
		char buffer[1024];

		if (i915_psr_debugfs_read_and_process_statistics(file_fd, buffer, sizeof(buffer)))
			run = 0;
	}

	i915_psr_debugfs_print_statistics();
	i915_psr_debugfs_shutdown(file_fd);

	return 0;
}
