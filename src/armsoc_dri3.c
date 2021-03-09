/* -*- mode: C; c-file-style: "k&r"; tab-width 4; indent-tabs-mode: t; -*- */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "armsoc_driver.h"
#include "armsoc_exa.h"

#include "dri3.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

static int
armsoc_dri3_open(ScreenPtr pScreen, RRProviderPtr provider, int* out)
{
	ScrnInfoPtr       pScrn   = xf86ScreenToScrn(pScreen);
	struct ARMSOCRec* pARMSOC = ARMSOCPTR(pScrn);
	int               fd;
	drm_magic_t       magic;

	if (!pARMSOC->deviceName)
		return BadAlloc;

	fd = open(pARMSOC->deviceName, O_RDWR | O_CLOEXEC);

	if (fd < 0)
		return BadAlloc;

	if (drmGetMagic(fd, &magic) < 0) {
		if (errno == EACCES) {
			*out = fd;
			return Success;
		} else {
			close(fd);
			return BadMatch;
		}
	}

	if (drmAuthMagic(pARMSOC->drmFD, magic) < 0) {
		close(fd);
		return BadMatch;
	}
		
	*out = fd;
	return Success;
}	

static PixmapPtr
armsoc_dri3_pixmap_from_fd(ScreenPtr pScreen,
						   int       fd     ,
						   CARD16    width  ,
						   CARD16    height ,
						   CARD16    stride ,
						   CARD8     depth  ,
						   CARD8     bpp    )
{
	ScrnInfoPtr       pScrn   = xf86ScreenToScrn(pScreen);
	struct ARMSOCRec* pARMSOC = ARMSOCPTR(pScrn);
	PixmapPtr         pixmap  = NULL;
    struct ARMSOCPixmapPrivRec* priv;

	if (depth < 8) {
		DEBUG_MSG("%s(fd=%d,width=%d,height=%d,stride=%d,depth=%d,bpp=%d) depth < 8 failed",
				  __func__, fd, width, height, stride, depth, bpp);
		goto failed;
	}

	switch (bpp) {
	case 8:
	case 16:
	case 32:
		break;
	default:
		DEBUG_MSG("%s(fd=%d,width=%d,height=%d,stride=%d,depth=%d,bpp=%d) bpp failed",
				  __func__, fd, width, height, stride, depth, bpp);
		goto failed;
	}

	pixmap = pScreen->CreatePixmap(pScreen, width, height, depth, ARMSOC_CREATE_PIXMAP_IMPORT);
	if (!pixmap) {
		DEBUG_MSG("%s(fd=%d,width=%d,height=%d,stride=%d,depth=%d,bpp=%d) CreatePixmap() failed",
				  __func__, fd, width, height, stride, depth, bpp);
		goto failed;
	}

	priv = exaGetPixmapDriverPrivate(pixmap);
	if (!priv) {
		goto failed;
	}

	priv->bo = armsoc_bo_import_with_dim(pARMSOC->dev, fd, width, height, stride, depth, bpp);
	if (!priv->bo) {
		DEBUG_MSG("%s(fd=%d,width=%d,height=%d,stride=%d,depth=%d,bpp=%d) armsoc_bo_import_with_dim() failed",
				  __func__, fd, width, height, stride, depth, bpp);
		goto failed;
	}

	if (pScreen->ModifyPixmapHeader(pixmap, 0, 0, 0, 0, stride, NULL)) {
		DEBUG_MSG("%s(fd=%d,width=%d,height=%d,stride=%d,depth=%d,bpp=%d) ModifyPixmapHeader(pixmap=%p,stride=%d) failed",
				  __func__, fd, width, height, stride, depth, bpp, pixmap, stride);
		goto failed;
	}

	return pixmap;
	
 failed:
	if (pixmap)
		fbDestroyPixmap(pixmap);

	ERROR_MSG("DRI3 Pixmap from FD(%d) failed", fd);
	return NULL;
}

static int
armsoc_dri3_fd_from_pixmap(ScreenPtr pScreen,
						   PixmapPtr pixmap ,
						   CARD16*   stride ,
						   CARD32*   size   )
{
	ScrnInfoPtr       pScrn = xf86ScreenToScrn(pScreen);
	struct armsoc_bo* bo;
	int               fd;

	bo = ARMSOCPixmapBo(pixmap);

	if (!bo) {
		exaMoveInPixmap(pixmap);
		bo = ARMSOCPixmapBo(pixmap);
		if (!bo) {
			DEBUG_MSG("%s(pixmap=%p) bo==NULL failed", __func__, pixmap);
			goto failed;
		}
	}

	fd = armsoc_bo_export(bo);
	if (fd < 0) {
		DEBUG_MSG("%s(pixmap=%p) armsoc_bo_export(%p) failed", __func__, pixmap, bo);
		goto failed;
	}

	*stride = armsoc_bo_pitch(bo);
	*size   = armsoc_bo_size(bo);
	DEBUG_MSG("%s(pixmap=%p,*stride=%d,*size=%d) success", __func__, pixmap, *stride, *size);
	return fd;

  failed:
	ERROR_MSG("DRI3 Fd From Pixmap(%p) failed", pixmap);
	return -1;
}

static dri3_screen_info_rec armsoc_dri3_screen_info = {
	.version        = DRI3_SCREEN_INFO_VERSION  ,
	.open           = armsoc_dri3_open          ,
	.pixmap_from_fd = armsoc_dri3_pixmap_from_fd,
	.fd_from_pixmap = armsoc_dri3_fd_from_pixmap,
};

Bool
ARMSOCDRI3ScreenInit(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);

	if (!dri3_screen_init(pScreen, &armsoc_dri3_screen_info)) {
		WARNING_MSG("dri3_screen_init failed\n");
		return FALSE;
	}
	return TRUE;
}
