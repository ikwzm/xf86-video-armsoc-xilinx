/*
 * Copyright © 2013 ARM Limited.
 *
 * Permission is hereby granted, free of charge, to any person
obtaining a
 * copy of this software and associated documentation files (the
"Software"),
 * to deal in the Software without restriction, including without
limitation
 * the rights to use, copy, modify, merge, publish, distribute,
sublicense,
 * and/or sell copies of the Software, and to permit persons to whom
the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
next
 * paragraph) shall be included in all copies or substantial portions
of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT
SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "../drmmode_driver.h"
#include <stddef.h>
#include <xf86drmMode.h>
#include <xf86drm.h>
#include <sys/ioctl.h>

/* Cursor dimensions
 * Technically we probably don't have any size limit.. since we
 * are just using an overlay... but xserver will always create
 * cursor images in the max size, so don't use width/height values
 * that are too big
 */
#define CURSORW  (64)
#define CURSORH  (64)

/*
 * Padding added down each side of cursor image. This is a workaround
for a bug
 * causing corruption when the cursor reaches the screen edges.
 */
#define CURSORPAD (16)

/* Optional function */
static int init_plane_for_cursor(int drm_fd, uint32_t plane_id) {
	return 0;
}

static int create_custom_gem(int fd, struct armsoc_create_gem
*create_gem) {
	struct drm_mode_create_dumb create_arg;
	int ret;

	memset (&create_arg, 0, sizeof (create_arg));
	create_arg.bpp = create_gem->bpp;
	create_arg.width = create_gem->width;
	create_arg.height = create_gem->height;

	ret = ioctl (fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_arg);
	if (ret)
		return ret;

	/* Convert custom create_exynos to generic create_gem */
	create_gem->handle = create_arg.handle;
	create_gem->pitch = create_arg.pitch;
	create_gem->size = create_gem->height * create_arg.pitch;

	return 0;
}

struct drmmode_interface sti_interface = {
	"sti"                 /* name of drm driver */,
	1                     /* use_page_flip_events */,
	1                     /* use_early_display */,
	CURSORW               /* cursor width */,
	CURSORH               /* cursor_height */,
	CURSORPAD             /* cursor padding */,
	HWCURSOR_API_STANDARD /* cursor_api */,
	init_plane_for_cursor /* init_plane_for_cursor */,
	0                     /* vblank_query_supported */,
	create_custom_gem     /* create_custom_gem */,
};

