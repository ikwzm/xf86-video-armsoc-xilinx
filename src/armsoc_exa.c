
/*
 * Copyright © 2011 Texas Instruments, Inc
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
 * Authors:
 *    Rob Clark <rob@ti.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "armsoc_exa.h"
#include "armsoc_driver.h"
#include "umplock/umplock_ioctl.h"
#include <sys/ioctl.h>
#include <unistd.h>

/* keep this here, instead of static-inline so submodule doesn't
 * need to know layout of ARMSOCRec.
 */
_X_EXPORT struct ARMSOCEXARec *
ARMSOCEXAPTR(ScrnInfoPtr pScrn)
{
	struct ARMSOCRec *pARMSOC = ARMSOCPTR(pScrn);
	return pARMSOC->pARMSOCEXA;
}

/* Common ARMSOC EXA functions, mostly related to pixmap/buffer allocation.
 * Individual driver submodules can use these directly, or wrap them with
 * there own functions if anything additional is required.  Submodules
 * can use ARMSOCPrixmapPrivPtr#priv for their own private data.
 */

/* used by DRI2 code to play buffer switcharoo */
void
ARMSOCPixmapExchange(PixmapPtr a, PixmapPtr b)
{
	struct ARMSOCPixmapPrivRec *apriv = exaGetPixmapDriverPrivate(a);
	struct ARMSOCPixmapPrivRec *bpriv = exaGetPixmapDriverPrivate(b);
	exchange(apriv->priv, bpriv->priv);
	exchange(apriv->bo, bpriv->bo);

	/* Ensure neither pixmap has a dmabuf fd attached to the bo if the
	 * ext_access_cnt refcount is 0, as it will never be cleared. */
	if (armsoc_bo_has_dmabuf(apriv->bo) && !apriv->ext_access_cnt) {
		armsoc_bo_clear_dmabuf(apriv->bo);

		/* Should only have to clear one dmabuf fd, otherwise the
		 * refcount is wrong */
		assert(!armsoc_bo_has_dmabuf(bpriv->bo));
	} else if (armsoc_bo_has_dmabuf(bpriv->bo) && !bpriv->ext_access_cnt) {
		armsoc_bo_clear_dmabuf(bpriv->bo);

		assert(!armsoc_bo_has_dmabuf(apriv->bo));
	}
}

_X_EXPORT void *
ARMSOCCreatePixmap2(ScreenPtr pScreen, int width, int height,
		int depth, int usage_hint, int bitsPerPixel,
		int *new_fb_pitch)
{
	struct ARMSOCPixmapPrivRec *priv = calloc(1, sizeof(*priv));
	ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
	struct ARMSOCRec *pARMSOC = ARMSOCPTR(pScrn);
	enum armsoc_buf_type buf_type = ARMSOC_BO_NON_SCANOUT;

	if (!priv)
		return NULL;

	if (!(usage_hint & ARMSOC_CREATE_PIXMAP_IMPORT) && 
		(width > 0 && height > 0 && depth > 0 && bitsPerPixel > 0)) {

		if (usage_hint & ARMSOC_CREATE_PIXMAP_SCANOUT)
			buf_type = ARMSOC_BO_SCANOUT;

		/* Pixmap creates and takes a ref on its bo */
		priv->bo = armsoc_bo_new_with_dim(pARMSOC->dev,
				width,
				height,
				depth,
				bitsPerPixel, buf_type);

		if ((!priv->bo) && ARMSOC_BO_SCANOUT == buf_type) {
			/* Tried to create a scanout but failed. Attempt to
			 * fall back to non-scanout instead.
			 */
			WARNING_MSG(
					"Scanout buffer allocation failed, falling back to non-scanout");
			buf_type = ARMSOC_BO_NON_SCANOUT;
			/* Pixmap creates and takes a ref on its bo */
			priv->bo = armsoc_bo_new_with_dim(pARMSOC->dev,
					width,
					height,
					depth,
					bitsPerPixel, buf_type);
		}

		if (!priv->bo) {
			ERROR_MSG("failed to allocate %dx%d bo, buf_type = %d",
					width, height, buf_type);
			free(priv);
			return NULL;
		}
		*new_fb_pitch = armsoc_bo_pitch(priv->bo);
	}

	/* The usage_hint field of the Pixmap passed to ModifyPixmapHeader is
	 * not set to the usage_hint parameter passed to CreatePixmap.
	 * It does appear to be set here so we stash it in the private
	 * structure. However as we do not fully understand the uses of this
	 * parameter, beware of any unexpected values!
	 */
	priv->usage_hint = usage_hint;

	return priv;
}

_X_EXPORT void
ARMSOCDestroyPixmap(ScreenPtr pScreen, void *driverPriv)
{
	struct ARMSOCPixmapPrivRec *priv = driverPriv;

	assert(!priv->ext_access_cnt);

	/* If ModifyPixmapHeader failed, it's possible we don't have a bo
	 * backing this pixmap. */
	if (priv->bo) {
		assert(!armsoc_bo_has_dmabuf(priv->bo));
		/* pixmap drops ref on its bo */
		armsoc_bo_unreference(priv->bo);
	}

	free(priv);
}

_X_EXPORT Bool
ARMSOCModifyPixmapHeader(PixmapPtr pPixmap, int width, int height,
		int depth, int bitsPerPixel, int devKind,
		pointer pPixData)
{
	struct ARMSOCPixmapPrivRec *priv = exaGetPixmapDriverPrivate(pPixmap);
	ScrnInfoPtr pScrn = pix2scrn(pPixmap);
	struct ARMSOCRec *pARMSOC = ARMSOCPTR(pScrn);
	enum armsoc_buf_type buf_type = ARMSOC_BO_NON_SCANOUT;

    /* Only modify specified fields, keeping all others intact. */
	if (pPixData)
		pPixmap->devPrivate.ptr = pPixData;

	if (devKind > 0)
		pPixmap->devKind = devKind;

	/*
	 * We can't accelerate this pixmap, and don't ever want to
	 * see it again..
	 */
	if (pPixData && pPixData != armsoc_bo_map(pARMSOC->scanout)) {
		/* scratch-pixmap (see GetScratchPixmapHeader()) gets recycled,
		 * so could have a previous bo!
		 * Pixmap drops ref on its old bo */
		armsoc_bo_unreference(priv->bo);
		priv->bo = NULL;

		/* Returning FALSE calls miModifyPixmapHeader */
		return FALSE;
	}

	/* Replacing the pixmap's current bo with the scanout bo */
	if (pPixData == armsoc_bo_map(pARMSOC->scanout) && priv->bo != pARMSOC->scanout) {
		struct armsoc_bo *old_bo = priv->bo;

		priv->bo = pARMSOC->scanout;
		/* pixmap takes a ref on its new bo */
		armsoc_bo_reference(priv->bo);

		if (old_bo) {
			/* We are detaching the old_bo so clear it now. */
			if (armsoc_bo_has_dmabuf(old_bo))
				armsoc_bo_clear_dmabuf(old_bo);
			/* pixmap drops ref on previous bo */
			armsoc_bo_unreference(old_bo);
		}
	}

	if (priv->usage_hint & ARMSOC_CREATE_PIXMAP_SCANOUT)
		buf_type = ARMSOC_BO_SCANOUT;

	if (depth > 0)
		pPixmap->drawable.depth = depth;

	if (bitsPerPixel > 0)
		pPixmap->drawable.bitsPerPixel = bitsPerPixel;

	if (width > 0)
		pPixmap->drawable.width = width;

	if (height > 0)
		pPixmap->drawable.height = height;

	/*
	 * X will sometimes create an empty pixmap (width/height == 0) and then
	 * use ModifyPixmapHeader to point it at PixData. We'll hit this path
	 * during the CreatePixmap call. Just return true and skip the allocate
	 * in this case.
	 */
	if (!pPixmap->drawable.width || !pPixmap->drawable.height)
		return TRUE;

	if ((priv->usage_hint & ARMSOC_CREATE_PIXMAP_IMPORT) && !priv->bo)
		return TRUE;

	assert(priv->bo);
	if (armsoc_bo_width(priv->bo) != pPixmap->drawable.width ||
	    armsoc_bo_height(priv->bo) != pPixmap->drawable.height ||
	    armsoc_bo_bpp(priv->bo) != pPixmap->drawable.bitsPerPixel) {
		if (armsoc_bo_imported(priv->bo)) {
			ERROR_MSG("failed to resize %dx%d%d imported bo",
					pPixmap->drawable.width,
					pPixmap->drawable.height,
					pPixmap->drawable.bitsPerPixel);
			return FALSE;
		}
		/* pixmap drops ref on its old bo */
		armsoc_bo_unreference(priv->bo);
		/* pixmap creates new bo and takes ref on it */
		priv->bo = armsoc_bo_new_with_dim(pARMSOC->dev,
				pPixmap->drawable.width,
				pPixmap->drawable.height,
				pPixmap->drawable.depth,
				pPixmap->drawable.bitsPerPixel, buf_type);

		if ((!priv->bo) && ARMSOC_BO_SCANOUT == buf_type) {
			/* Tried to create a scanout but failed. Attempt to
			 * fall back to non-scanout instead.
			 */
			WARNING_MSG(
					"Scanout buffer allocation failed, falling back to non-scanout");
			buf_type = ARMSOC_BO_NON_SCANOUT;
			/* pixmap creates new bo and takes ref on it */
			priv->bo = armsoc_bo_new_with_dim(pARMSOC->dev,
					pPixmap->drawable.width,
					pPixmap->drawable.height,
					pPixmap->drawable.depth,
					pPixmap->drawable.bitsPerPixel,
					buf_type);
		}
		if (!priv->bo) {
			ERROR_MSG("failed to allocate %dx%d bo, buf_type = %d",
					pPixmap->drawable.width,
					pPixmap->drawable.height, buf_type);
			return FALSE;
		}
		pPixmap->devKind = armsoc_bo_pitch(priv->bo);
	}
	return TRUE;
}

/**
 * WaitMarker is a required EXA callback but synchronization is
 * performed during ARMSOCPrepareAccess so this function does not
 * have anything to do at present
 */
_X_EXPORT void
ARMSOCWaitMarker(ScreenPtr pScreen, int marker)
{
	/* no-op */
}

static inline enum armsoc_gem_op idx2op(int index)
{
	switch (index) {
	case EXA_PREPARE_SRC:
	case EXA_PREPARE_MASK:
	case EXA_PREPARE_AUX_SRC:
	case EXA_PREPARE_AUX_MASK:
		return ARMSOC_GEM_READ;
	case EXA_PREPARE_AUX_DEST:
	case EXA_PREPARE_DEST:
	default:
		return ARMSOC_GEM_READ_WRITE;
	}
}

/**
 * PrepareAccess() is called before CPU access to an offscreen pixmap.
 *
 * @param pPix the pixmap being accessed
 * @param index the index of the pixmap being accessed.
 *
 * PrepareAccess() will be called before CPU access to an offscreen pixmap.
 * This can be used to set up hardware surfaces for byteswapping or
 * untiling, or to adjust the pixmap's devPrivate.ptr for the purpose of
 * making CPU access use a different aperture.
 *
 * The index is one of #EXA_PREPARE_DEST, #EXA_PREPARE_SRC,
 * #EXA_PREPARE_MASK, #EXA_PREPARE_AUX_DEST, #EXA_PREPARE_AUX_SRC, or
 * #EXA_PREPARE_AUX_MASK. Since only up to #EXA_NUM_PREPARE_INDICES pixmaps
 * will have PrepareAccess() called on them per operation, drivers can have
 * a small, statically-allocated space to maintain state for PrepareAccess()
 * and FinishAccess() in.  Note that PrepareAccess() is only called once per
 * pixmap and operation, regardless of whether the pixmap is used as a
 * destination and/or source, and the index may not reflect the usage.
 *
 * PrepareAccess() may fail.  An example might be the case of hardware that
 * can set up 1 or 2 surfaces for CPU access, but not 3.  If PrepareAccess()
 * fails, EXA will migrate the pixmap to system memory.
 * DownloadFromScreen() must be implemented and must not fail if a driver
 * wishes to fail in PrepareAccess().  PrepareAccess() must not fail when
 * pPix is the visible screen, because the visible screen can not be
 * migrated.
 *
 * @return TRUE if PrepareAccess() successfully prepared the pixmap for CPU
 * drawing.
 * @return FALSE if PrepareAccess() is unsuccessful and EXA should use
 * DownloadFromScreen() to migate the pixmap out.
 */
_X_EXPORT Bool
ARMSOCPrepareAccess(PixmapPtr pPixmap, int index)
{
	ScreenPtr pScreen = pPixmap->drawable.pScreen;
	ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
	struct ARMSOCRec *pARMSOC = ARMSOCPTR(pScrn);
	uint32_t dmabuf_name = 0;
	_lock_item_s item;
	int ret;
	struct ARMSOCPixmapPrivRec *priv = exaGetPixmapDriverPrivate(pPixmap);

	pPixmap->devPrivate.ptr = armsoc_bo_map(priv->bo);
	if (!pPixmap->devPrivate.ptr) {
		xf86DrvMsg(-1, X_ERROR, "%s: Failed to map buffer\n", __func__);
		return FALSE;
	}

	/* Attach dmabuf fd to bo to synchronise access if
	 * pixmap wrapped by DRI2
	 */
	if (priv->ext_access_cnt && !armsoc_bo_has_dmabuf(priv->bo)) {
		if (armsoc_bo_set_dmabuf(priv->bo)) {
			xf86DrvMsg(-1, X_ERROR,
				"%s: Unable to get dma_buf fd for bo, to enable synchronised CPU access.\n",
				__func__);
			return FALSE;
		}
	}

	if (-1 != pARMSOC->lockFD) {
		ret = armsoc_bo_get_name(priv->bo, &dmabuf_name);

		if (ret) {
			ERROR_MSG("could not get buffer name");
			return FALSE;
		}

		item.secure_id = dmabuf_name;
		item.usage = _LOCK_ACCESS_CPU_WRITE;

		if (ioctl(pARMSOC->lockFD,  LOCK_IOCTL_CREATE, &item) < 0) {
			ERROR_MSG("Unable to create lock item\n");
			return FALSE;
		}
		if (ioctl(pARMSOC->lockFD, LOCK_IOCTL_PROCESS, &item) < 0) {
			int max_retries = 5;
			ERROR_MSG("Unable to process lock item with ID 0x%x - throttling\n", item.secure_id);
			while ((ioctl(pARMSOC->lockFD, LOCK_IOCTL_PROCESS, &item) < 0) && max_retries) {
				usleep(2000);
				max_retries--;
			}
			if (max_retries == 0)
				ERROR_MSG("Warning: Max retries == 0\n");
		}
	} else {
		if (armsoc_bo_cpu_prep(priv->bo, idx2op(index))) {
			xf86DrvMsg(-1, X_ERROR,
				"%s: armsoc_bo_cpu_prep failed - unable to synchronise access.\n",
				__func__);
			return FALSE;
		}
	}
	return TRUE;
}

/**
 * FinishAccess() is called after CPU access to an offscreen pixmap.
 *
 * @param pPix the pixmap being accessed
 * @param index the index of the pixmap being accessed.
 *
 * FinishAccess() will be called after finishing CPU access of an offscreen
 * pixmap set up by PrepareAccess().  Note that the FinishAccess() will not be
 * called if PrepareAccess() failed and the pixmap was migrated out.
 */
_X_EXPORT void
ARMSOCFinishAccess(PixmapPtr pPixmap, int index)
{
	ScreenPtr pScreen = pPixmap->drawable.pScreen;
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	struct ARMSOCPixmapPrivRec *priv = exaGetPixmapDriverPrivate(pPixmap);
	struct ARMSOCRec *pARMSOC = ARMSOCPTR(pScrn);
	if (-1 != pARMSOC->lockFD){
		uint32_t dmabuf_name = 0;
		_lock_item_s item;
		int ret;

		pPixmap->devPrivate.ptr = NULL;
		ret = armsoc_bo_get_name(priv->bo, &dmabuf_name);
		if (ret) {
			ERROR_MSG("could not get buffer name");
			return ;
		}
		item.secure_id = dmabuf_name;
		item.usage = _LOCK_ACCESS_CPU_WRITE;
		ioctl(pARMSOC->lockFD, LOCK_IOCTL_RELEASE, &item);
	}else{
		/* NOTE: can we use EXA migration module to track which parts of the
		 * buffer was accessed by sw, and pass that info down to kernel to
		 * do a more precise cache flush..
		 */
		pPixmap->devPrivate.ptr = NULL;
		armsoc_bo_cpu_fini(priv->bo, idx2op(index));
	}
}

/**
 * PixmapIsOffscreen() is an optional driver replacement to
 * exaPixmapHasGpuCopy(). Set to NULL if you want the standard behaviour
 * of exaPixmapHasGpuCopy().
 *
 * @param pPix the pixmap
 * @return TRUE if the given drawable is in framebuffer memory.
 *
 * exaPixmapHasGpuCopy() is used to determine if a pixmap is in
 * offscreen memory, meaning that acceleration could probably be done
 * to it, and that it will need to be wrapped by PrepareAccess()/
 * FinishAccess() when accessing it from the CPU.
 */
_X_EXPORT Bool
ARMSOCPixmapIsOffscreen(PixmapPtr pPixmap)
{
	/* offscreen means in 'gpu accessible memory', not that it's off
	 * the visible screen.  We currently have no special constraints,
	 * since compatible ARM CPUS have a flat memory model (no separate
	 * GPU memory). If an individual EXA implementation has additional
	 * constraints, like buffer size or mapping in GPU MMU, it should
	 * wrap this function.
	 */
	struct ARMSOCPixmapPrivRec *priv = exaGetPixmapDriverPrivate(pPixmap);
	return priv && priv->bo;
}

void ARMSOCRegisterExternalAccess(PixmapPtr pPixmap)
{
	struct ARMSOCPixmapPrivRec *priv = exaGetPixmapDriverPrivate(pPixmap);

	priv->ext_access_cnt++;
}

void ARMSOCDeregisterExternalAccess(PixmapPtr pPixmap)
{
	struct ARMSOCPixmapPrivRec *priv = exaGetPixmapDriverPrivate(pPixmap);

	assert(priv->ext_access_cnt > 0);
	priv->ext_access_cnt--;

	if (priv->ext_access_cnt == 0) {
		/* No DRI2 buffers wrapping the pixmap, so no
		 * need for synchronisation with dma_buf
		 */
		if (armsoc_bo_has_dmabuf(priv->bo))
			armsoc_bo_clear_dmabuf(priv->bo);
	}
}
