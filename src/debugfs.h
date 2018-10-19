#pragma once

#include <stdint.h>

int i915_psr_debugfs_read_init();
int i915_psr_debugfs_shutdown(int fd);

int i915_psr_debugfs_print_statistics();
int i915_psr_debugfs_reset_statistics();

int i915_psr_debugfs_read(int fd, char *buffer, uint16_t len);
int i915_psr_debugfs_read_and_process_statistics(int fd, char *buffer, uint16_t len);

int i915_psr_debugfs_process_statistics(char *buffer);

int i915_psr_debugfs_read_source_status_id(char *buffer, uint8_t *status);
int i915_psr_debugfs_read_sink_status_id(char *buffer, uint8_t *status);
int i915_psr_debugfs_got_su_entry(char *buffer, uint8_t *got);
int i915_psr_debugfs_got_su_blocks(char *buffer, uint8_t *got);
int i915_psr_debugfs_got_su_blocks_val(char *buffer, uint8_t *got);

const char *i915_psr_debugfs_source_status_string_get(uint8_t status);
const char *i915_psr_debugfs_sink_status_string_get(uint8_t status);
