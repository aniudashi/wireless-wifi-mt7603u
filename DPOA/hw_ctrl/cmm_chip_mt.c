/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology	5th	Rd.
 * Science-based Industrial	Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2004, Ralink Technology, Inc.
 *
 * All rights reserved.	Ralink's source	code is	an unpublished work	and	the
 * use of a	copyright notice does not imply	otherwise. This	source code
 * contains	confidential trade secret material of Ralink Tech. Any attemp
 * or participation	in deciphering,	decoding, reverse engineering or in	any
 * way altering	the	source code	is stricitly prohibited, unless	the	prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************

	Module Name:
	cmm_asic.c

	Abstract:
	Functions used to communicate with ASIC
	
	Revision History:
	Who			When			What
	--------	----------		----------------------------------------------
*/


#include "rt_config.h"


INT mt_asic_top_init(RTMP_ADAPTER *pAd)
{
#if defined(MT7603_FPGA) || defined(MT7628_FPGA)
	UINT32 mac_val;
		
	// TODO: shiang-7603
	// PSE/MAC Clock setting, Register work-around for FPGA, now it's take cared by ROM code!
	//RTMP_IO_READ32(pAd, 0x1100, &mac_val);
	//mac_val = 0x26110080;
	//RTMP_IO_WRITE32(pAd, 0x1100, mac_val);

	// For FPGA, adjust Clock setting, now 100ms setting will mapping to 400ms in real time
	/* Len's setting,
		MCU run in 10MHz, LMAC run in 1.25MHz
		so GPT timer will have 8 times differentiation with free run timer
		=>LMAC time will 8 times slower than MCU time
		=> host driver set beacon interval 100ms, then real beacon interval will be 800ms
	*/
#ifdef MT7628_FPGA
	// enable MAC circuit
	RTMP_IO_READ32(pAd, 0x2108, &mac_val);
	mac_val &= (~0x7ff0);
	RTMP_IO_WRITE32(pAd, 0x2108, mac_val);

	mac_val = 0x3e013;
	RTMP_IO_WRITE32(pAd, 0x2d004, mac_val);
#endif /* MT7628_FPGA */

	RTMP_IO_WRITE32(pAd, 0x24088, 0x900); // Set 40MHz Clock
	RTMP_IO_WRITE32(pAd, 0x2d034, 0x64180003);	// Set 32k clock, this clock is used for lower power.
#endif /* MT7603_FPGA */

	return TRUE;
}


INT mt_hif_sys_init(RTMP_ADAPTER *pAd)
{


#ifdef RTMP_MAC_USB
	{
       	USB_DMA_CFG_STRUC	UsbCfg;
            	
       	/* USB1.1 do not use bulk in aggregation */
		if (pAd->BulkInMaxPacketSize >= 512)
			UsbCfg.field.RxBulkAggEn = 1;
		else 
			UsbCfg.field.RxBulkAggEn = 0;

		/* for last packet, PBF might use more than limited, so minus 2 to prevent from error */
		UsbCfg.field.RxBulkAggLmt = (MAX_RXBULK_SIZE /1024) - 3;
		UsbCfg.field.RxBulkAggTOut = 0x80; 

		UsbCfg.field.RxBulkEn = 1;
		UsbCfg.field.TxBulkEn = 1;

		UsbCfg.word &= ~UDMA_WLCFG_0_RX_MPSZ_PAD0_MASK; //bit18
		UsbCfg.word |= UDMA_WLCFG_0_RX_MPSZ_PAD0(1);
				
		RTMP_IO_WRITE32(pAd, UDMA_WLCFG_0, UsbCfg.word);
	}
#endif /* RTMP_MAC_USB */


	return TRUE;
}

