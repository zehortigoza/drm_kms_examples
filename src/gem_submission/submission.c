#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "lib.h"

#include <i915_drm.h>
#include <xf86drm.h>

bool batch_buffer_cmd_push(struct gem_buffer *batch_buffer, uint32_t cmd)
{
    if (!batch_buffer->batch.cmds) {
        batch_buffer->batch.cmds = malloc(sizeof(uint32_t) * 64);
        if (!batch_buffer->batch.cmds)
            return false;
        batch_buffer->batch.cmds_len = 64;
    }

    if (batch_buffer->batch.cmds_index >= batch_buffer->batch.cmds_len)
        return false;

    batch_buffer->batch.cmds[batch_buffer->batch.cmds_index++] = cmd;
    return true;
}

/* TODO: Understand better */
static void blt_switch_tiling(struct gem_buffer *batch_buffer, bool on)
{
	uint32_t bcs_swctrl = (0x3 << 16) | (on ? 0x3 : 0x0);

	/*
     * To change the tile register, insert an MI_FLUSH_DW followed by an
	 * MI_LOAD_REGISTER_IMM
	 */
    batch_buffer_cmd_push(batch_buffer, MI_FLUSH_DW | 2);
    batch_buffer_cmd_push(batch_buffer, 0);
    batch_buffer_cmd_push(batch_buffer, 0);
    batch_buffer_cmd_push(batch_buffer, 0);

    batch_buffer_cmd_push(batch_buffer, MI_LOAD_REGISTER_IMM);
    batch_buffer_cmd_push(batch_buffer, 0x22200); /* BCS_SWCTRL */
    batch_buffer_cmd_push(batch_buffer, bcs_swctrl);
    batch_buffer_cmd_push(batch_buffer, MI_NOOP);
}

static bool batch_buffer_gem_create_and_write(int drm_fd, struct gem_buffer *batch_buffer)
{
    int ret;

    batch_buffer->size = batch_buffer->batch.cmds_index * 4;
    ret = gem_buffer_create(drm_fd, batch_buffer->size, &batch_buffer->handle);
    if (ret)
        return false;

    batch_buffer->mmap_ptr = gem_buffer_mmap(drm_fd, batch_buffer->handle, batch_buffer->size);
    if (!batch_buffer->mmap_ptr)
        goto gem_destroy;

    gem_set_domain(drm_fd, batch_buffer->handle, I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);

    memcpy(batch_buffer->mmap_ptr, batch_buffer->batch.cmds, batch_buffer->size);

    gem_buffer_unmap(drm_fd, batch_buffer->mmap_ptr, batch_buffer->size);
    batch_buffer->mmap_ptr = NULL;

    return true;

gem_destroy:
    gem_buffer_destroy(drm_fd, batch_buffer->handle);
    return false;
}

uint64_t gem_gtt_size_get(int drm_fd)
{
    static uint64_t gtt_size = 0;/* Not intending to support multiple GPUs */
    struct drm_i915_gem_context_param p = {
        .param = I915_CONTEXT_PARAM_GTT_SIZE
	};
    int val = 0;
    struct drm_i915_getparam gp = {
        .param = I915_PARAM_HAS_ALIASING_PPGTT,
        .value = &val,
    };

    if (gtt_size)
        return gtt_size;

    printf("Loading GTT size\n");

    gem_context_get_param(drm_fd, &p);
    gtt_size = p.value;
    printf("\tI915_CONTEXT_PARAM_GTT_SIZE=0x%" PRIx64 "\n", gtt_size);

    gem_get_param(drm_fd, &gp);
    printf("\tI915_PARAM_HAS_ALIASING_PPGTT=%i\n", val);
    if (val <= 1)
        gtt_size /= 2;

    if ((gtt_size - 1) >> 32) {
        if (gtt_size & (3ULL << 47))
            gtt_size = (1ULL << 46);
    }

    printf("\tGTT size: 0x%" PRIx64 "\n", gtt_size);
    return gtt_size;
}

static uint64_t canonical_addr(uint64_t addr)
{
    int shift = 47;

    return (int64_t)(addr << shift) >> shift;
}

uint64_t gem_buffer_get_offset(int drm_fd, struct gem_buffer UNUSED *buffer)
{
    uint64_t offset = rand() & UINT32_MAX;
    uint64_t gtt_size = gem_gtt_size_get(drm_fd);

    offset <<= 32;
    offset |= rand() & UINT32_MAX;
    if (offset < 10)
        offset = 0x1000;

    offset += 256 << 10; /* Keep the low 256k clear, for negative deltas */
    offset &= gtt_size - 1;
    offset &= ~(GEM_PAGE_SIZE - 1);
    offset = canonical_addr(offset);

    return offset;
}

int batch_buffer_push_reloc(struct gem_buffer *batch_buffer,
                            struct drm_i915_gem_exec_object2 *cmd_obj,
                            struct gem_buffer *image_buffer,
                            uint64_t image_offset)
{
    struct drm_i915_gem_relocation_entry *reloc_array;
    uint32_t val;

    reloc_array = calloc(1, sizeof(struct drm_i915_gem_relocation_entry));
    if (!reloc_array)
        return -ENOMEM;
    cmd_obj->relocs_ptr = (uint64_t)reloc_array;
    cmd_obj->relocation_count = 1;

    reloc_array[0].target_handle = image_buffer->handle;
    reloc_array[0].read_domains = 0;
    reloc_array[0].write_domain = I915_GEM_DOMAIN_RENDER;
    reloc_array[0].offset = batch_buffer->batch.cmds_index;
    reloc_array[0].presumed_offset = image_offset;

    val = image_offset;
    batch_buffer_cmd_push(batch_buffer, val);

    val = (image_offset >> 32);
    batch_buffer_cmd_push(batch_buffer, val);

    return 0;
}

static int
blt_draw_rect(int drm_fd, struct gem_buffer *image_buffer, struct drm_clip_rect *rect, uint32_t color)
{
    struct drm_i915_gem_exec_object2 *obj_array;
    struct drm_i915_gem_execbuffer2 execbuf = {};
    struct gem_buffer batch_buffer = {};
    uint32_t val;
    int ret, fence;

    printf("blt_draw_rect()\n");
    printf("\tx1=%u\n\ty1=%u\n\tx2=%u\n\ty2=%u\n\tcolor=0x%x\n",
           rect->x1, rect->y1, rect->x2, rect->y2, color);

    obj_array = calloc(2, sizeof(*obj_array));
    if (!obj_array)
        return -ENOMEM;

    obj_array[0].offset = gem_buffer_get_offset(drm_fd, &batch_buffer);
    printf("\tbatch_buffer offset=0x%llx\n", obj_array[0].offset);

    obj_array[1].handle = image_buffer->handle;
    obj_array[1].offset = gem_buffer_get_offset(drm_fd, image_buffer);
    printf("\timage_buffer offset=0x%llx\n", obj_array[1].offset);
    obj_array[1].flags = EXEC_OBJECT_WRITE | EXEC_OBJECT_NEEDS_FENCE;

    blt_switch_tiling(&batch_buffer, true);

    val = 0x2 << 29;// client
    val |= 0x50 << 22;// opcode
    val |= 0x1 << 21; // write alpha
    val |= 0x1 << 20; // write RGB
    val |= 0x0 << 11; // tiling, linear
    val |= 0x5 << 0; // lenght
    batch_buffer_cmd_push(&batch_buffer, val);

    val = 0x3 << 24;// color depth, 32bpp
    val |= 0xf0 << 16;// raster operation??
    val |= image_buffer->image.stride;
    batch_buffer_cmd_push(&batch_buffer, val);

    val = rect->y1 << 16 | rect->x1 << 0;
    batch_buffer_cmd_push(&batch_buffer, val);

    val = rect->y2 << 16 | rect->x2 << 0;
    batch_buffer_cmd_push(&batch_buffer, val);

    batch_buffer_push_reloc(&batch_buffer, &obj_array[0], image_buffer, obj_array[1].offset);

    batch_buffer_cmd_push(&batch_buffer, color);

    blt_switch_tiling(&batch_buffer, false);

    /* Round batchbuffer usage to 2 DWORDs. */
    if ((batch_buffer.batch.cmds_index * 4) % 8)
        batch_buffer_cmd_push(&batch_buffer, 0);

    batch_buffer_cmd_push(&batch_buffer, MI_BATCH_BUFFER_END);

    /* Round batchbuffer usage to 2 DWORDs. */
    if ((batch_buffer.batch.cmds_index * 4) % 8)
        batch_buffer_cmd_push(&batch_buffer, 0);

    batch_buffer_gem_create_and_write(drm_fd, &batch_buffer);
    obj_array[0].handle = batch_buffer.handle;

    execbuf.buffers_ptr = (uint64_t)obj_array;
	execbuf.buffer_count = 2;
    execbuf.batch_len = batch_buffer.batch.cmds_index * 4;
    printf("\tbatch_buffer.batch.cmds_index=%i\n", batch_buffer.batch.cmds_index);
    printf("\tbatch_len=%i\n", batch_buffer.batch.cmds_index * 4);
    execbuf.flags = I915_EXEC_BLT;
    execbuf.flags |= I915_EXEC_NO_RELOC;
    execbuf.flags |= I915_EXEC_BATCH_FIRST;
    execbuf.flags |= I915_EXEC_FENCE_OUT;

    ret = drmIoctl(drm_fd, DRM_IOCTL_I915_GEM_EXECBUFFER2_WR, &execbuf);
    printf("\tDRM_IOCTL_I915_GEM_EXECBUFFER2_WR ret=%i errno=%i\n", ret, errno);
    if (ret)
        goto exec_fail;

    //update_offsets()

    fence = execbuf.rsvd2 >> 32;
    printf("\tfence=%i\n", fence);
    //TODO: poll fence and close it... for now sleeping. IGT sync_fence_wait()
    sleep(2);

exec_fail:
    free(obj_array);
    free(batch_buffer.batch.cmds);
    gem_buffer_destroy(drm_fd, batch_buffer.handle);

    return ret;
}

static void rand_init()
{
    time_t t;

    srand((unsigned) time(&t));
}

uint32_t gem_buffer_pixel_color_get(struct gem_buffer *image_buffer, unsigned y, unsigned x)
{
    unsigned index = y * image_buffer->image.w + x;
    return image_buffer->mmap_ptr[index];
}

void gem_buffer_pixel_color_set(struct gem_buffer *image_buffer, unsigned y, unsigned x, uint32_t val)
{
    unsigned index = y * image_buffer->image.w + x;
    image_buffer->mmap_ptr[index] = val;
}

static int pixel_check(struct gem_buffer *image_buffer, unsigned y, unsigned x, uint32_t expected)
{
    uint32_t val = gem_buffer_pixel_color_get(image_buffer, y, x);
    bool match = val == expected;

    printf("Pixel %ux%u %smatch(expected=0x%x read=0x%x)\n",
           x, y, match == 0 ? "do not ": "", expected, val);
    return match ? 0 : -1;
}

int main()
{
    struct gem_buffer image_buffer = {};
    struct color_32_bits color;
    struct drm_clip_rect rect;
    int drm_fd, ret;
    unsigned x, y;
    uint32_t val;

    rand_init();

    drm_fd = drmOpen("i915", NULL);
    if (drm_fd < 0) {
        printf("Unable to open drm device. Errno %i\n", errno);
        return 0;
    }

    image_buffer.image.w = 800;
    image_buffer.image.h = 600;
    image_buffer.image.bpp = 4;
    /* TODO: needs stride aligment? */
    image_buffer.image.stride = image_buffer.image.w * image_buffer.image.bpp;
    image_buffer.size = image_buffer.image.stride * image_buffer.image.h;
    if (gem_buffer_create(drm_fd, image_buffer.size, &image_buffer.handle)) {
        printf("Unable to create image_buffer. Errno %i\n", errno);
        goto close;
    }

    image_buffer.mmap_ptr = gem_buffer_mmap(drm_fd, image_buffer.handle, image_buffer.size);
    if (!image_buffer.mmap_ptr) {
        printf("Unable to mmap image_buffer. Errno %i\n", errno);
        goto destroy_image_buffer;
    }

    val = I915_CACHING_NONE;
    gem_get_caching(drm_fd, image_buffer.handle, &val);
    printf("Buffer can have cache coherence=%i\n", val != I915_CACHING_NONE);

    gem_gtt_size_get(drm_fd);

    gem_set_domain(drm_fd, image_buffer.handle, I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);

    color.rgb.a = 255;
    color.rgb.r = 255;
    color.rgb.g = color.rgb.b = 0;
    x = y = 0;
    gem_buffer_pixel_color_set(&image_buffer, y, x, color.value);
    ret = pixel_check(&image_buffer, y, x, color.value);
    if (ret)
        goto exit;
    ret = pixel_check(&image_buffer, y, x + 1, 0);
    if (ret)
        goto exit;

    y = image_buffer.image.h / 2 + 1;
    gem_buffer_pixel_color_set(&image_buffer, y, x, color.value);
    ret = pixel_check(&image_buffer, y, x, color.value);
    if (ret)
        goto exit;
    ret = pixel_check(&image_buffer, y, x + 1, 0);
    if (ret)
        goto exit;

    color.rgb.r = color.rgb.g = color.rgb.b = 125;
    rect.x1 = rect.y1 = 0;
    rect.x2 = rect.x1 + image_buffer.image.w;
    rect.y2 = rect.y1 + image_buffer.image.h / 2;
    ret = blt_draw_rect(drm_fd, &image_buffer, &rect, color.value);
    if (ret)
        goto exit;

    gem_set_domain(drm_fd, image_buffer.handle, I915_GEM_DOMAIN_GTT, I915_GEM_DOMAIN_GTT);

    y = 0;
    pixel_check(&image_buffer, y, x, color.value);
    if (ret)
        goto exit;

    y = image_buffer.image.h / 2 + 1;
    pixel_check(&image_buffer, y, x, color.value);
    if (ret)
        goto exit;

exit:
    printf("Hello world of graphics \\o/\nExecution %s\n", ret == 0 ? "success" : "fail");
    gem_buffer_unmap(drm_fd, image_buffer.mmap_ptr, image_buffer.size);
destroy_image_buffer:
    gem_buffer_destroy(drm_fd, image_buffer.handle);
close:
    drmClose(drm_fd);

    return 0;    
}