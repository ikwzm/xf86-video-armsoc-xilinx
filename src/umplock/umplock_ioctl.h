/*
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

#ifndef __UMPLOCK_IOCTL_H__
#define __UMPLOCK_IOCTL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/types.h>
#include <linux/ioctl.h>

#ifndef __user
#define __user
#endif


/**
 * @file umplock_ioctl.h
 * This file describes the interface needed to use the Linux device driver.
 * The interface is used by the userpace Mali DDK.
 */

typedef enum
{
	_LOCK_ACCESS_RENDERABLE = 1,
	_LOCK_ACCESS_TEXTURE,
	_LOCK_ACCESS_CPU_WRITE,
	_LOCK_ACCESS_CPU_READ,
} _lock_access_usage;

typedef struct _lock_item_s
{
	unsigned int secure_id;
	_lock_access_usage usage;
} _lock_item_s;


#define LOCK_IOCTL_GROUP 0x91

#define _LOCK_IOCTL_CREATE_CMD  0   /* create kernel lock item        */
#define _LOCK_IOCTL_PROCESS_CMD 1   /* process kernel lock item       */
#define _LOCK_IOCTL_RELEASE_CMD 2   /* release kernel lock item       */
#define _LOCK_IOCTL_ZAP_CMD     3   /* clean up all kernel lock items */
#define _LOCK_IOCTL_DUMP_CMD    4   /* dump all the items */

#define LOCK_IOCTL_MAX_CMDS     5

#define LOCK_IOCTL_CREATE  _IOW( LOCK_IOCTL_GROUP, _LOCK_IOCTL_CREATE_CMD,  _lock_item_s )
#define LOCK_IOCTL_PROCESS _IOW( LOCK_IOCTL_GROUP, _LOCK_IOCTL_PROCESS_CMD, _lock_item_s )
#define LOCK_IOCTL_RELEASE _IOW( LOCK_IOCTL_GROUP, _LOCK_IOCTL_RELEASE_CMD, _lock_item_s )
#define LOCK_IOCTL_ZAP     _IO ( LOCK_IOCTL_GROUP, _LOCK_IOCTL_ZAP_CMD )
#define LOCK_IOCTL_DUMP    _IO ( LOCK_IOCTL_GROUP, _LOCK_IOCTL_DUMP_CMD )

#ifdef __cplusplus
}
#endif

#endif /* __UMPLOCK_IOCTL_H__ */

