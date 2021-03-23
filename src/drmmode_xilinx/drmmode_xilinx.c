/*
 * Xilinx X11 ARMSOC driver
 *
 * Author: Ichiro Kawazome<ichiro_k@ca2.so-net.ne.jp>
 *
 * Copyright (C) 2021 Ichiro Kawazome
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

static int create_custom_gem(int fd, struct armsoc_create_gem *create_gem)
{
	struct drm_mode_create_dumb arg;
	int ret;

	memset(&arg, 0, sizeof(arg));

	if (create_gem->buf_type == ARMSOC_BO_SCANOUT) {
		arg.height = create_gem->height;
		arg.width  = create_gem->width;
		arg.bpp    = create_gem->bpp;
		/* For Xilinx DPDMA needs pitch 256 bytes alignment */
		arg.pitch  = ALIGN(create_gem->width * DIV_ROUND_UP(create_gem->bpp,8), 256);
	} else {
		/* For Lima need height and width 16 bytes alignment */
		arg.height = ALIGN(create_gem->height, 16);
		arg.width  = ALIGN(create_gem->width , 16);
		arg.bpp    = create_gem->bpp;
		arg.pitch  = arg.width * DIV_ROUND_UP(create_gem->bpp,8);
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

