/*
 * Xilinx X11 ARMSOC driver
 *
 * Author: Ichiro Kawazome<ichiro_k@ca2.so-net.ne.jp>
 *
 * Copyright (C) 2022 Ichiro Kawazome
 *
 * Based on drmmode_xilinx.c
 *
 * Copyright © 2013 ARM Limited.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <stdlib.h>

#include <drm.h>
#include <xf86drm.h>

#include "../drmmode_driver.h"

#define DIV_ROUND_UP(val,d)	(((val) + (d    ) - 1) / (d))
#define ALIGN(val, align)	(((val) + (align) - 1) & ~((align) - 1))

#include "xlnx_drm.h"

static int  xlnx_drmmode_initialized    = 0;
static int  xlnx_drm_scanout_align_size = 256;
static int  xlnx_drm_dumb_align_size    = 8;
static int  xlnx_drm_dumb_cache_avalable= 0;
static int  xlnx_drm_dumb_cache_default = 1;

extern _X_EXPORT Bool armsocDebug;

#define XLNX_DRM_WARNING(fmt, ...) \
	do {	xf86Msg(X_WARNING, "WARNING: drmmode_xilinx " fmt "\n",\
			##__VA_ARGS__); \
	} while (0)

#define XLNX_DRM_INFO(fmt, ...) \
	do {	xf86Msg(X_INFO, "drmmode_xilinx " fmt "\n",\
			##__VA_ARGS__);\
	} while (0)

#define XLNX_DRM_DEBUG(fmt, ...) \
	do { if (armsocDebug) \
		xf86Msg(X_INFO, "drmmode_xilinx " fmt "\n",\
			##__VA_ARGS__);\
	} while (0)

static void xlnx_drmmode_init(int fd)
{
	int ret;
	struct drm_xlnx_get_param get_param_arg;
	struct drm_xlnx_set_param set_param_arg;
		
	memset(&get_param_arg, 0, sizeof(get_param_arg));
	get_param_arg.param = DRM_XLNX_PARAM_DRIVER_IDENTIFIER;
	ret = drmIoctl(fd, DRM_IOCTL_XLNX_GET_PARAM, &get_param_arg);
	if (ret) {
		XLNX_DRM_WARNING("drmIoctl("
				 "DRM_IOCTL_XLNX_GET_PARAM,"
				 "DRM_XLNX_PARAM_DRIVER_IDENTIFIER"
				 ") failed(%d)", ret);
		goto init_done;
	}
	if (get_param_arg.value != DRM_XLNX_DRIVER_IDENTIFIER) {
		XLNX_DRM_WARNING("invalid identifier(0x%08X)\n", (int)get_param_arg.value);
		goto init_done;
	}

	memset(&get_param_arg, 0, sizeof(get_param_arg));
	get_param_arg.param = DRM_XLNX_PARAM_SCANOUT_ALIGNMENT_SIZE;
	ret = drmIoctl(fd, DRM_IOCTL_XLNX_GET_PARAM, &get_param_arg);
	if (ret) {
		XLNX_DRM_WARNING("drmIoctl("
				 "DRM_IOCTL_XLNX_GET_PARAM,"
				 "DRM_XLNX_PARAM_SCANOUT_ALIGNMENT_SIZE"
				 ") failed(%d)", ret);
	} else {
        	xlnx_drm_scanout_align_size = get_param_arg.value;
        }

	memset(&set_param_arg, 0, sizeof(set_param_arg));
	set_param_arg.param = DRM_XLNX_PARAM_DUMB_ALIGNMENT_SIZE;
	set_param_arg.value = xlnx_drm_dumb_align_size;
	ret = drmIoctl(fd, DRM_IOCTL_XLNX_SET_PARAM, &set_param_arg);
	if (ret) {
		XLNX_DRM_WARNING("drmIoctl("
				 "DRM_IOCTL_XLNX_SET_PARAM,"
				 "DRM_XLNX_PARAM_DUMB_ALIGNMENT_SIZE,"
				 "%d"
				 ") failed(%d)", 
				 (int)set_param_arg.value, ret);
	}

	memset(&get_param_arg, 0, sizeof(get_param_arg));
	get_param_arg.param = DRM_XLNX_PARAM_DUMB_CACHE_AVALABLE;
	ret = drmIoctl(fd, DRM_IOCTL_XLNX_GET_PARAM, &get_param_arg);
	if (ret) {
		XLNX_DRM_WARNING("drmIoctl("
				 "DRM_IOCTL_XLNX_GET_PARAM,"
				 "DRM_XLNX_PARAM_DUMB_CACHE_AVALABLE"
				 ") failed(%d)", ret);
	} else {
        	xlnx_drm_dumb_cache_avalable = get_param_arg.value;
        }
	memset(&get_param_arg, 0, sizeof(set_param_arg));
	set_param_arg.param = DRM_XLNX_PARAM_DUMB_CACHE_DEFAULT_MODE;
	set_param_arg.value = xlnx_drm_dumb_cache_default;
	ret = drmIoctl(fd, DRM_IOCTL_XLNX_SET_PARAM, &set_param_arg);
	if (ret) {
		XLNX_DRM_WARNING("drmIoctl("
				 "DRM_IOCTL_XLNX_SET_PARAM,"
				 "DRM_XLNX_PARAM_DUMB_CACHE_DEFAULT_MODE,"
				 "%d"
				 ") failed(%d)", 
				 (int)set_param_arg.value, ret);
	}
    init_done:
	XLNX_DRM_DEBUG("scanout  alignment size = %d", xlnx_drm_scanout_align_size );
        XLNX_DRM_DEBUG("dumb buf alignment size = %d", xlnx_drm_dumb_align_size    );
        XLNX_DRM_DEBUG("dumb buf cache avalable = %d", xlnx_drm_dumb_cache_avalable);
	XLNX_DRM_INFO("initialized");
}

static int create_custom_gem(int fd, struct armsoc_create_gem *create_gem)
{
	struct drm_mode_create_dumb arg;
	int ret;

	if (xlnx_drmmode_initialized != 1) {
		xlnx_drmmode_init(fd);
		xlnx_drmmode_initialized = 1;
	}

	memset(&arg, 0, sizeof(arg));

	if (create_gem->buf_type == ARMSOC_BO_SCANOUT) {
		arg.height = create_gem->height;
		arg.width  = create_gem->width;
		arg.bpp    = create_gem->bpp;
		/* For Xilinx DPDMA needs pitch scanout alignment */
		arg.pitch  = ALIGN(create_gem->width * DIV_ROUND_UP(create_gem->bpp,8), xlnx_drm_scanout_align_size);
		arg.flags  = DRM_XLNX_GEM_DUMB_SCANOUT;
                if (xlnx_drm_dumb_cache_avalable != 0)
			arg.flags |= DRM_XLNX_GEM_DUMB_CACHE_OFF;
	} else {
		/* For Lima need height and width 16 pixel alignment */
		arg.height = ALIGN(create_gem->height, 16);
		arg.width  = ALIGN(create_gem->width , 16);
		arg.bpp    = create_gem->bpp;
		arg.pitch  = arg.width * DIV_ROUND_UP(create_gem->bpp, 8);
		arg.flags  = DRM_XLNX_GEM_DUMB_NON_SCANOUT;
                if (xlnx_drm_dumb_cache_avalable != 0)
			arg.flags |= DRM_XLNX_GEM_DUMB_CACHE_OFF;
	}

	ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &arg);
	if (ret)
		return ret;

	create_gem->handle = arg.handle;
	create_gem->pitch  = arg.pitch;
	create_gem->size   = arg.size;

	return 0;
}

struct drmmode_interface xilinx_interface = {
	"xlnx"                /* name of drm driver */,
	1                     /* use_page_flip_events */,
	1                     /* use_early_display */,
	0                     /* cursor width */,
	0                     /* cursor_height */,
	0                     /* cursor padding */,
	HWCURSOR_API_NONE     /* cursor_api */,
	NULL                  /* init_plane_for_cursor */,
	0                     /* vblank_query_supported */,
	create_custom_gem     /* create_custom_gem */,
};

