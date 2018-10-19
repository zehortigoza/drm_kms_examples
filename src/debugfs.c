#include "debugfs.h"

#include <fcntl.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PSR_DEBUG_FS "/sys/kernel/debug/dri/0/i915_edp_psr_status"

// sink regex
static regex_t regex_sink_status;

// source regex
static regex_t regex_source_status;
static regex_t regex_su_entry;
static regex_t regex_su_blocks;

static uint32_t su_entry_count;

static const char * const live_status[] = {
	"IDLE Reset state",
	"CAPTURE Send capture frame",
	"CAPTURE_FS Fast sleep after capture frame is sent",
	"SLEEP Selective Update",
	"BUFON_FW Turn Buffer on and Send Fast wake",
	"ML_UP Turn Main link up and send SR",
	"SU_STANDBY Selective update or Standby state",
	"FAST_SLEEP Send Fast sleep",
	"DEEP_SLEEP Enter Deep sleep",
	"BUF_ON Turn ON IO Buffer",
	"TG_ON Turn ON Timing Generator",
	"BUFON_FW_2 Turn Buffer on and Send Fast wake for 3 Block case"
};
static uint32_t status_count[12];

const char *i915_psr_debugfs_source_status_string_get(uint8_t status)
{
	return live_status[status];
}

#define  EDP_PSR2_STATUS_STATE_MASK     (0xf << 28)
#define  EDP_PSR2_STATUS_STATE_SHIFT    28

static const char * const sink_status[] = {
		"DP_PSR_SINK_INACTIVE",
		"DP_PSR_SINK_ACTIVE_SRC_SYNCED",
		"DP_PSR_SINK_ACTIVE_RFB",
		"DP_PSR_SINK_ACTIVE_SINK_SYNCED",
		"DP_PSR_SINK_ACTIVE_RESYNC",
		"unknown",
		"unknown",
		"DP_PSR_SINK_INTERNAL_ERROR"
};
static uint32_t sink_status_count[8];

const char *i915_psr_debugfs_sink_status_string_get(uint8_t status)
{
	return sink_status[status];
}

int i915_psr_debugfs_read_init()
{
	int r;

	r = regcomp(&regex_source_status, "Source PSR status: .* \\[0x", 0);
	if (r) {
		char error[512];

		regerror(r, &regex_source_status, error, sizeof(error));
		printf("Error compiling regex_source_status: %s\n", error);
		return -1;
	}

	r = regcomp(&regex_su_entry, "SU entry completion: yes", 0);
	if (r) {
		char error[512];

		regerror(r, &regex_su_entry, error, sizeof(error));
		printf("Error compiling regex_su_entry: %s\n", error);
		return -1;
	}

	r = regcomp(&regex_sink_status, "DP_PSR_STATUS: ", 0);
	if (r) {
		char error[512];

		regerror(r, &regex_sink_status, error, sizeof(error));
		printf("Error compiling regex_sink_status: %s\n", error);
		return -1;
	}

	// PSR2 SU status3: 0x00000000[
	r = regcomp(&regex_su_blocks, "PSR2 SU status: 0x", 0);
	if (r) {
		char error[512];

		regerror(r, &regex_su_blocks, error, sizeof(error));
		printf("Error compiling regex_su_blocks: %s\n", error);
		return -1;
	}

	r = open(PSR_DEBUG_FS, O_RDONLY | O_CLOEXEC);
	if (r < 0)
		printf("Error opening file %s\n", PSR_DEBUG_FS);

	return r;
}

int i915_psr_debugfs_read(int fd, char *buffer, uint16_t len)
{
	uint16_t index = 0;

	while (1) {
		ssize_t r;

		r = read(fd, buffer + index, len - index);
		if (r < 0) {
			printf("Error reading file\n");
			return r;
		}
		if (r > 0) {
			index += r;
			continue;
		}

		buffer[index] = 0;
		lseek(fd, 0, SEEK_SET);

		break;
	}

	return 0;
}

static uint32_t regex_match_hex_string_to_uint32(char *buffer, regmatch_t *match)
{
	char string_val[9];

	memcpy(string_val, &buffer[match->rm_eo], 8);
	string_val[8] = 0;

	return (uint32_t)strtol(string_val, NULL, 16);

}

int i915_psr_debugfs_read_source_status_id(char *buffer, uint8_t *status)
{
	regmatch_t match;

	if (!regexec(&regex_source_status, buffer, 1, &match, 0)) {
		uint32_t val = regex_match_hex_string_to_uint32(buffer, &match);

		val &= EDP_PSR2_STATUS_STATE_MASK;
		val >>= EDP_PSR2_STATUS_STATE_SHIFT;
		*status = val;

		return 0;
	}

	return -1;
}

int i915_psr_debugfs_read_and_process_statistics(int fd, char *buffer, uint16_t len)
{
	int r = i915_psr_debugfs_read(fd, buffer, len);
	if (r)
		return r;

	return i915_psr_debugfs_process_statistics(buffer);
}

int i915_psr_debugfs_read_sink_status_id(char *buffer, uint8_t *status)
{
	regmatch_t match;

	if (!regexec(&regex_sink_status, buffer, 1, &match, 0)) {
		uint32_t val = buffer[match.rm_eo] - '0';
		*status = val;

		return 0;
	}

	return -1;
}

int i915_psr_debugfs_got_su_entry(char *buffer, uint8_t *got)
{
	if (!regexec(&regex_su_entry, buffer, 0, NULL, 0))
		*got = 1;
	else
		*got = 0;

	return 0;
}

int i915_psr_debugfs_got_su_blocks(char *buffer, uint8_t *got)
{
	regmatch_t match;

	if (!regexec(&regex_su_blocks, buffer, 1, &match, 0)) {
		uint32_t val = regex_match_hex_string_to_uint32(buffer, &match);
		*got = !!val;

		return 0;
	}

	return -1;
}

#define  EDP_PSR2_SU_STATUS_NUM_SU_BLOCKS_IN_FRAME_SHIFT(i)	(i * 10)
#define  EDP_PSR2_SU_STATUS_NUM_SU_BLOCKS_IN_FRAME_MASK(i)	(0x3FF << (i * 10))

int i915_psr_debugfs_got_su_blocks_val(char *buffer, uint8_t *got)
{
	regmatch_t match;

	if (!regexec(&regex_su_blocks, buffer, 1, &match, 0)) {
		uint32_t val = regex_match_hex_string_to_uint32(buffer, &match);

		val &= EDP_PSR2_SU_STATUS_NUM_SU_BLOCKS_IN_FRAME_MASK(0);
		val >>= EDP_PSR2_SU_STATUS_NUM_SU_BLOCKS_IN_FRAME_SHIFT(0);
		*got = val;

		return 0;
	}

	*got = 0;

	return -1;
}

int i915_psr_debugfs_process_statistics(char *buffer)
{
	uint8_t status;

	if (!i915_psr_debugfs_read_source_status_id(buffer, &status))
		status_count[status]++;

	if (!i915_psr_debugfs_got_su_entry(buffer, &status) && status == 1)
		su_entry_count++;

	if (!i915_psr_debugfs_read_sink_status_id(buffer, &status))
		sink_status_count[status]++;

	return 0;
}

int i915_psr_debugfs_print_statistics()
{
	uint32_t total;
	uint16_t i;

	printf("Source status:\n");
	total = 0;
	for (i = 0; i < (sizeof(live_status) / sizeof(const char *)); i++) {
		printf("\t%s=%u\n", live_status[i], status_count[i]);
		total += status_count[i];
	}
	printf("total=%u\n", total);

	printf("\nSink status:\n");
	total = 0;
	for (i = 0; i < (sizeof(sink_status) / sizeof(const char *)); i++) {
		printf("\t%s=%u\n", sink_status[i], sink_status_count[i]);
		total += sink_status_count[i];
	}
	printf("total=%u\n", total);

	printf("\nSU entry count=%d\n", su_entry_count);

	return 0;
}

int i915_psr_debugfs_reset_statistics()
{
	uint16_t i;

	for (i = 0; i < (sizeof(live_status) / sizeof(const char *)); i++)
		status_count[i] = 0;

	for (i = 0; i < (sizeof(sink_status) / sizeof(const char *)); i++)
		sink_status_count[i] = 0;

	su_entry_count = 0;

	return 0;
}

int i915_psr_debugfs_shutdown(int fd)
{
	close(fd);
	i915_psr_debugfs_reset_statistics();

	return 0;
}
