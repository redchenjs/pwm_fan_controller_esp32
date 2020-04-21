/*
 * gdisp_lld_config.h
 *
 *  Created on: 2019-04-29 22:04
 *      Author: Jack Chen <redchenjs@live.com>
 */

#ifndef _GDISP_LLD_CONFIG_H
#define _GDISP_LLD_CONFIG_H

#if GFX_USE_GDISP

/*===========================================================================*/
/* Driver hardware support.                                                  */
/*===========================================================================*/

#define GDISP_HARDWARE_FLUSH            TRUE
#define GDISP_HARDWARE_STREAM_WRITE     TRUE
#define GDISP_HARDWARE_STREAM_READ      TRUE
#define GDISP_HARDWARE_CONTROL          TRUE

#define GDISP_LLD_PIXELFORMAT           GDISP_PIXELFORMAT_RGB565

#endif	/* GFX_USE_GDISP */

#endif	/* _GDISP_LLD_CONFIG_H */
