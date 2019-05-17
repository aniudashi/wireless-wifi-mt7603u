/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology 5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2004, Ralink Technology, Inc.
 *
 * All rights reserved. Ralink's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************

	Module Name:

	Abstract:

	Revision History:
	Who 		When			What
	--------	----------		----------------------------------------------
*/

#include "rt_config.h"



static UCHAR *txwi_txop_str[]={"HT_TXOP", "PIFS", "SIFS", "BACKOFF", "Invalid"};
#define TXWI_TXOP_STR(_x)	((_x) <= 3 ? txwi_txop_str[(_x)]: txwi_txop_str[4])
VOID dump_rtmp_txwi(RTMP_ADAPTER *pAd, TXWI_STRUC *pTxWI)
{
	struct _TXWI_OMAC *txwi_o = (struct _TXWI_OMAC *)pTxWI;

	ASSERT((sizeof(struct _TXWI_OMAC) == pAd->chipCap.TXWISize));

	if (pAd->chipCap.TXWISize != (sizeof(struct _TXWI_OMAC)))
		DBGPRINT(RT_DEBUG_TRACE, ("%s():sizeof(struct _TXWI_OMAC)=%d, pAd->chipCap.TXWISize=%d\n",
					__FUNCTION__, sizeof(struct _TXWI_OMAC), pAd->chipCap.TXWISize));

	DBGPRINT(RT_DEBUG_OFF, ("\tPHYMODE=%d(%s)\n", txwi_o->PHYMODE, get_phymode_str(txwi_o->PHYMODE)));
	DBGPRINT(RT_DEBUG_OFF, ("\tiTxBF=%d\n", txwi_o->iTxBF));
	DBGPRINT(RT_DEBUG_OFF, ("\tSounding=%d\n", txwi_o->Sounding));
	DBGPRINT(RT_DEBUG_OFF, ("\teTxBF=%d\n", txwi_o->eTxBF));
	DBGPRINT(RT_DEBUG_OFF, ("\tSTBC=%d\n", txwi_o->STBC));
	DBGPRINT(RT_DEBUG_OFF, ("\tShortGI=%d\n", txwi_o->ShortGI));
	DBGPRINT(RT_DEBUG_OFF, ("\tBW=%d(%sMHz)\n", txwi_o->BW, get_bw_str(txwi_o->BW)));
	DBGPRINT(RT_DEBUG_OFF, ("\tMCS=%d\n", txwi_o->MCS));
	DBGPRINT(RT_DEBUG_OFF, ("\tTxOP=%d(%s)\n", txwi_o->txop, TXWI_TXOP_STR(txwi_o->txop)));
	DBGPRINT(RT_DEBUG_OFF, ("\tMpduDensity=%d\n", txwi_o->MpduDensity));
	DBGPRINT(RT_DEBUG_OFF, ("\tAMPDU=%d\n", txwi_o->AMPDU));
	DBGPRINT(RT_DEBUG_OFF, ("\tTS=%d\n", txwi_o->TS));
	DBGPRINT(RT_DEBUG_OFF, ("\tCF-ACK=%d\n", txwi_o->CFACK));
	DBGPRINT(RT_DEBUG_OFF, ("\tMIMO-PS=%d\n", txwi_o->MIMOps));
	DBGPRINT(RT_DEBUG_OFF, ("\tFRAG=%d\n", txwi_o->FRAG));
	DBGPRINT(RT_DEBUG_OFF, ("\tPID=%d\n", txwi_o->PacketId));
	DBGPRINT(RT_DEBUG_OFF, ("\tMPDUtotalByteCnt=%d\n", txwi_o->MPDUtotalByteCnt));
	DBGPRINT(RT_DEBUG_OFF, ("\tWCID=%d\n", txwi_o->wcid));
	DBGPRINT(RT_DEBUG_OFF, ("\tBAWinSize=%d\n", txwi_o->BAWinSize));
	DBGPRINT(RT_DEBUG_OFF, ("\tNSEQ=%d\n", txwi_o->NSEQ));
	DBGPRINT(RT_DEBUG_OFF, ("\tACK=%d\n", txwi_o->ACK));
}


VOID dump_rtmp_rxwi(RTMP_ADAPTER *pAd, RXWI_STRUC *pRxWI)
{
	struct _RXWI_OMAC *rxwi_o = (struct _RXWI_OMAC *)pRxWI;

	ASSERT((sizeof(struct _RXWI_OMAC) == pAd->chipCap.RXWISize));

	if (pAd->chipCap.RXWISize != (sizeof(struct _RXWI_OMAC)))
		DBGPRINT(RT_DEBUG_TRACE, ("%s():sizeof(struct _RXWI_OMAC)=%d, pAd->chipCap.RXWISize=%d\n",
					__FUNCTION__, sizeof(struct _RXWI_OMAC), pAd->chipCap.RXWISize));

	DBGPRINT(RT_DEBUG_OFF, ("\tWCID=%d\n", rxwi_o->wcid));
	DBGPRINT(RT_DEBUG_OFF, ("\tMPDUtotalByteCnt=%d\n", rxwi_o->MPDUtotalByteCnt));
	DBGPRINT(RT_DEBUG_OFF, ("\tPhyMode=%d(%s)\n", rxwi_o->phy_mode, get_phymode_str(rxwi_o->phy_mode)));
	DBGPRINT(RT_DEBUG_OFF, ("\tMCS=%d\n", rxwi_o->mcs));
	DBGPRINT(RT_DEBUG_OFF, ("\tBW=%d\n", rxwi_o->bw));
	DBGPRINT(RT_DEBUG_OFF, ("\tSGI=%d\n", rxwi_o->sgi));
	DBGPRINT(RT_DEBUG_OFF, ("\tSTBC=%d\n", rxwi_o->stbc));


	DBGPRINT(RT_DEBUG_OFF, ("\tSequence=%d\n", rxwi_o->SEQUENCE));
	DBGPRINT(RT_DEBUG_OFF, ("\tFRAG=%d\n", rxwi_o->FRAG));
	DBGPRINT(RT_DEBUG_OFF, ("\tTID=%d\n", rxwi_o->tid));

	DBGPRINT(RT_DEBUG_OFF, ("\tkey_idx=%d\n", rxwi_o->key_idx));
	DBGPRINT(RT_DEBUG_OFF, ("\tBSS_IDX=%d\n", rxwi_o->bss_idx));

	DBGPRINT(RT_DEBUG_OFF, ("\tRSSI=%d:%d:%d\n", rxwi_o->RSSI0, rxwi_o->RSSI1, rxwi_o->RSSI2));
	DBGPRINT(RT_DEBUG_OFF, ("\tSNR=%d:%d:%d\n", rxwi_o->SNR0, rxwi_o->SNR1, rxwi_o->SNR2));
	DBGPRINT(RT_DEBUG_OFF, ("\tFreqOffset=%d\n", rxwi_o->FOFFSET));
}



