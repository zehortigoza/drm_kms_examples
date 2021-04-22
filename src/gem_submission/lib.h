#pragma once

#include <stdint.h>

#include <i915_drm.h>

#define GEM_PAGE_SIZE 4096
#define UNUSED __attribute__((unused))
#define PACKED __attribute__((__packed__))

#define MI_BATCH_BUFFER_END (0xA << 23)
#define MI_FLUSH_DW (0x26 << 23)
#define MI_LOAD_REGISTER_IMM ((0x22 << 23) | 1)
#define MI_NOOP 0x00

struct gem_buffer {
    uint32_t handle;
    uint32_t *mmap_ptr;
    uint64_t size;

    enum type {
        GEM_BUFFER_IMAGE = 0,
        GEM_BUFFER_BATCH
    } type;

    union {
        struct {
            uint32_t stride;
            uint32_t w, h;
            uint32_t bpp;
        } image;
        struct {
            uint32_t *cmds;
            unsigned cmds_index;
            unsigned cmds_len;
        } batch;
    };
};

struct  color_32_bits {
    union {
        struct PACKED {
            uint8_t r;
            uint8_t g;
            uint8_t b;
            uint8_t a;
        } rgb;
        uint32_t value;
    };
};

int gem_buffer_create(int drm_fd, uint64_t size, uint32_t *handler);
int gem_buffer_destroy(int drm_fd, uint32_t handle);

void *gem_buffer_mmap(int drm_fd, uint32_t handle, uint64_t size);
void gem_buffer_unmap(int UNUSED drm_fd, void *mmapped_gem_buffer, uint64_t size);

int gem_get_caching(int fd, uint32_t handle, uint32_t *caching);

int gem_set_domain(int fd, uint32_t handle, uint32_t read, uint32_t write);

int gem_context_get_param(int fd, struct drm_i915_gem_context_param *p);
int gem_get_param(int fd, struct drm_i915_getparam *p);