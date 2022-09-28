/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/* xrt_drm.h 
 *
 * Xilinx DRM KMS Driver Public Header
 *
 * Copyright (c) 2022 ikwzm <ichiro_k@ca2.so-net.ne.jp> 
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __XLNX_DRM_H__
#define __XLNX_DRM_H__

#include "drm.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * Parameter identifier of various information
 */	
enum drm_xlnx_param {
	DRM_XLNX_PARAM_DRIVER_IDENTIFIER       = 0,
	DRM_XLNX_PARAM_SCANOUT_ALIGNMENT_SIZE  = 1,
	DRM_XLNX_PARAM_DUMB_ALIGNMENT_SIZE     = 2,
	DRM_XLNX_PARAM_DUMB_CACHE_AVALABLE     = 3,
	DRM_XLNX_PARAM_DUMB_CACHE_DEFAULT_MODE = 4,
};

/**
 * Get various information of the Xilinx DRM KMS Driver
 */
struct drm_xlnx_get_param {
	__u32 param; /* in , value in enum drm_xlnx_param */
	__u32 pad;   /* pad, must be zero */
	__u64 value; /* out, parameter value */
};
	
/**
 * Set various information of the Xilinx DRM KMS Driver
 */
struct drm_xlnx_set_param {
	__u32 param; /* in , value in enum drm_xlnx_param */
	__u32 pad;   /* pad, must be zero */
	__u64 value; /* in , parameter value */
};

/**
 * Xilinx DRM KMS Driver specific ioctls.
 */
#define DRM_XLNX_GET_PARAM   0x00
#define DRM_XLNX_SET_PARAM   0x01

#define DRM_IOCTL_XLNX_GET_PARAM DRM_IOWR(DRM_COMMAND_BASE+DRM_XLNX_GET_PARAM, struct drm_xlnx_get_param)
#define DRM_IOCTL_XLNX_SET_PARAM DRM_IOWR(DRM_COMMAND_BASE+DRM_XLNX_SET_PARAM, struct drm_xlnx_set_param)

/**
 * Xilinx DRM KMS Driver Identifier
 */
#define DRM_XLNX_DRIVER_IDENTIFIER      (0x53620C75)

/**
 * Xilinx DRM KMS Driver Create Dumb Flags
 */
#define DRM_XLNX_GEM_DUMB_SCANOUT_BIT   (0)
#define DRM_XLNX_GEM_DUMB_SCANOUT_MASK  (1 << DRM_XLNX_GEM_DUMB_SCANOUT_BIT)
#define DRM_XLNX_GEM_DUMB_SCANOUT       (1 << DRM_XLNX_GEM_DUMB_SCANOUT_BIT)
#define DRM_XLNX_GEM_DUMB_NON_SCANOUT   (0 << DRM_XLNX_GEM_DUMB_SCANOUT_BIT)
	
#define DRM_XLNX_GEM_DUMB_CACHE_BIT     (1)
#define DRM_XLNX_GEM_DUMB_CACHE_MASK    (3 << DRM_XLNX_GEM_DUMB_CACHE_BIT)
#define DRM_XLNX_GEM_DUMB_CACHE_DEFAULT (0 << DRM_XLNX_GEM_DUMB_CACHE_BIT)
#define DRM_XLNX_GEM_DUMB_CACHE_OFF     (2 << DRM_XLNX_GEM_DUMB_CACHE_BIT)
#define DRM_XLNX_GEM_DUMB_CACHE_ON      (3 << DRM_XLNX_GEM_DUMB_CACHE_BIT)
	
#if defined(__cplusplus)
}
#endif

#endif /* __XLNX_DRM_H__ */
