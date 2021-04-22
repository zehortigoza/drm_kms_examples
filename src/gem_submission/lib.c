#include "lib.h"

#include <stdlib.h>
#include <sys/mman.h>

#include <i915_drm.h>
#include <xf86drm.h>

int gem_buffer_create(int drm_fd, uint64_t size, uint32_t *handler)
{
    struct drm_i915_gem_create gc = {
        .size = size,
    };
    int ret;

    ret = drmIoctl(drm_fd, DRM_IOCTL_I915_GEM_CREATE, &gc);
    if (ret)
        return ret;

    *handler = gc.handle;
    return 0;
}

int gem_buffer_destroy(int drm_fd, uint32_t handle)
{
    struct drm_gem_close gc = {
        .handle = handle,
    };

    return drmIoctl(drm_fd, DRM_IOCTL_GEM_CLOSE, &gc);
}

void *gem_buffer_mmap(int drm_fd, uint32_t handle, uint64_t size)
{
    struct drm_i915_gem_mmap_gtt mm = {
        .handle = handle,
    };
    void *ptr;
    int ret;

    ret = drmIoctl(drm_fd, DRM_IOCTL_I915_GEM_MMAP_GTT, &mm);
    if (ret)
        return NULL;

    ptr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, drm_fd, mm.offset);
    if (ptr == (void *)-1)
        return NULL;

    return ptr;
}

void gem_buffer_unmap(int UNUSED drm_fd, void *mmapped_gem_buffer, uint64_t size)
{
    munmap(mmapped_gem_buffer, size);
}

int gem_get_caching(int fd, uint32_t handle, uint32_t *caching)
{
    struct drm_i915_gem_caching arg = {
        .handle = handle,
    };
    int ret;

    ret = drmIoctl(fd, DRM_IOCTL_I915_GEM_GET_CACHING, &arg);
    if (ret)
        return ret;

    *caching = arg.caching;
    return 0;
}

int gem_set_domain(int fd, uint32_t handle, uint32_t read, uint32_t write)
{
	struct drm_i915_gem_set_domain set_domain = {
        .handle = handle,
        .read_domains = read,
        .write_domain = write,
    };

    return drmIoctl(fd, DRM_IOCTL_I915_GEM_SET_DOMAIN, &set_domain);
}

int gem_context_get_param(int fd, struct drm_i915_gem_context_param *p)
{
    return drmIoctl(fd, DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM, p);
}

int gem_get_param(int fd, struct drm_i915_getparam *p)
{
    return drmIoctl(fd, DRM_IOCTL_I915_GETPARAM, p);
}