/****************************************************************************
 * Ralink Tech Inc.
 * Taiwan, R.O.C.
 *
 * (c) Copyright 2002, Ralink Technology, Inc.
 *
 * All rights reserved. Ralink's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************/


#ifdef RTMP_MAC_USB


#include	"rt_config.h"

#ifdef CONFIG_PRE_ALLOC	
void *RTMPQMemAddr(int size, ra_dma_addr_t *pDmaAddr, int type);
enum BLK_TYPE {
	BLK_TX0,
	BLK_TX1,
	BLK_TX2,
	BLK_TX3,
	BLK_TX4,
	BLK_NULL,
	BLK_PSPOLL,
	BLK_RX0,
	BLK_RX1,
	BLK_RX2,
	BLK_RX3,
	BLK_RX4,
	BLK_RX5,
	BLK_RX6,
	BLK_RX7,
	BLK_CMD,
};
#endif

static NDIS_STATUS RTMPAllocUsbBulkBufStruct(
	IN RTMP_ADAPTER *pAd,
	IN PURB *ppUrb,
	IN PVOID *ppXBuffer,
	IN INT	bufLen,
	IN ra_dma_addr_t *pDmaAddr,
	IN RTMP_STRING *pBufName)
{
	POS_COOKIE pObj = (POS_COOKIE) pAd->OS_Cookie;

	
	*ppUrb = RTUSB_ALLOC_URB(0);
	if (*ppUrb == NULL)
	{
		DBGPRINT(RT_DEBUG_ERROR, ("<-- ERROR in Alloc urb struct for %s !\n", pBufName));
		return NDIS_STATUS_RESOURCES;
	}
	
#ifndef CONFIG_PRE_ALLOC	
	*ppXBuffer = RTUSB_URB_ALLOC_BUFFER(pObj->pUsb_Dev, bufLen, pDmaAddr);
	if (*ppXBuffer == NULL) {
		DBGPRINT(RT_DEBUG_ERROR, ("<-- ERROR in Alloc Bulk buffer for %s!\n", pBufName));
		return NDIS_STATUS_RESOURCES;
	}
#else	
	if ((strcmp(pBufName, "TxNullContext") == 0) || (strcmp(pBufName, "TxPsPollContext") == 0))
	{
		printk("Alloc TX null/PsPoll\n");
		*ppXBuffer = RTUSB_URB_ALLOC_BUFFER(pObj->pUsb_Dev, bufLen, pDmaAddr);
        	if (*ppXBuffer == NULL) {
                	DBGPRINT(RT_DEBUG_ERROR, ("<-- ERROR in Alloc Bulk buffer for %s!\n", pBufName));
                	return NDIS_STATUS_RESOURCES;
		}
	}
#endif
	return NDIS_STATUS_SUCCESS;
}


static NDIS_STATUS RTMPFreeUsbBulkBufStruct(
	IN RTMP_ADAPTER *pAd,
	IN PURB *ppUrb,
	IN PUCHAR *ppXBuffer,
	IN INT bufLen,
	IN ra_dma_addr_t data_dma)
{
	POS_COOKIE pObj = (POS_COOKIE) pAd->OS_Cookie;
	
	if (NULL != *ppUrb) {
		RTUSB_UNLINK_URB(*ppUrb);
		RTUSB_FREE_URB(*ppUrb);
		*ppUrb = NULL;
	}
	
#ifndef CONFIG_PRE_ALLOC	
	if (NULL != *ppXBuffer) {
		RTUSB_URB_FREE_BUFFER(pObj->pUsb_Dev, bufLen,	*ppXBuffer, data_dma);
		*ppXBuffer = NULL;
	}
#endif

	return NDIS_STATUS_SUCCESS;
}


#ifdef RESOURCE_PRE_ALLOC
VOID RTMPResetTxRxRingMemory(
	IN RTMP_ADAPTER * pAd)
{
	UINT index, i, acidx;
	PTX_CONTEXT pNullContext   = &pAd->NullContext;
	PTX_CONTEXT pPsPollContext = &pAd->PsPollContext;
	PCMD_RSP_CONTEXT pCmdRspEventContext = &pAd->CmdRspEventContext;
	unsigned int IrqFlags;

	rtmp_tx_swq_exit(pAd, WCID_ALL);
	
	for(i=0; i<(RX_RING_SIZE); i++)
	{
		PRX_CONTEXT  pRxContext = &(pAd->RxContext[i]);
		if (pRxContext->pUrb)
			RTUSB_UNLINK_URB(pRxContext->pUrb);
	}

	if (pCmdRspEventContext->pUrb)
		RTUSB_UNLINK_URB(pCmdRspEventContext->pUrb);

	/* unlink PsPoll urb resource*/
	if (pPsPollContext && pPsPollContext->pUrb)
		RTUSB_UNLINK_URB(pPsPollContext->pUrb);

	/* Free NULL frame urb resource*/
	if (pNullContext && pNullContext->pUrb)
		RTUSB_UNLINK_URB(pNullContext->pUrb);


	/* Free mgmt frame resource*/
	for(i = 0; i < MGMT_RING_SIZE; i++)
	{
		PTX_CONTEXT pMLMEContext = (PTX_CONTEXT)pAd->MgmtRing.Cell[i].AllocVa;
		if (pMLMEContext)
		{
			if (NULL != pMLMEContext->pUrb)
			{
				RTUSB_UNLINK_URB(pMLMEContext->pUrb);
				RTUSB_FREE_URB(pMLMEContext->pUrb);
				pMLMEContext->pUrb = NULL;
			}
		}
		
		if (NULL != pAd->MgmtRing.Cell[i].pNdisPacket) 
		{
			RELEASE_NDIS_PACKET(pAd, pAd->MgmtRing.Cell[i].pNdisPacket, NDIS_STATUS_FAILURE);
			pAd->MgmtRing.Cell[i].pNdisPacket = NULL;
			if (pMLMEContext)
				pMLMEContext->TransferBuffer = NULL; 
		}
		
	}
	
	
	/* Free Tx frame resource*/
	for (acidx = 0; acidx < NUM_OF_TX_RING; acidx++)
	{
		PHT_TX_CONTEXT pHTTXContext = &(pAd->TxContext[acidx]);
#ifdef USB_BULK_BUF_ALIGMENT
		INT ringidx;
		for(ringidx=0;ringidx < BUF_ALIGMENT_RINGSIZE ;ringidx++)
		{
			if (pHTTXContext && pHTTXContext->pUrb[ringidx])
				RTUSB_UNLINK_URB(pHTTXContext->pUrb[ringidx]);
		}
#else
		if (pHTTXContext && pHTTXContext->pUrb)
			RTUSB_UNLINK_URB(pHTTXContext->pUrb);
#endif /* USB_BULK_BUF_ALIGMENT */

	}
	
	for(i=0; i<6; i++)
	{
		NdisFreeSpinLock(&pAd->BulkOutLock[i]);
	}

#ifdef MULTI_WMM_SUPPORT
	/* Free Tx frame resource*/
	for (acidx = 0; acidx < NUM_OF_WMM1_TX_RING; acidx++)
	{
		PHT_TX_CONTEXT pHTTXContext = &(pAd->TxContextWmm1[acidx]);
		if (pHTTXContext && pHTTXContext->pUrb)
			RTUSB_UNLINK_URB(pHTTXContext->pUrb);
	}
	
	for(i=0; i<NUM_OF_WMM1_TX_RING; i++)
	{
		NdisFreeSpinLock(&pAd->BulkOutWmm1Lock[i]);
	}
#endif /* MULTI_WMM_SUPPORT */
	
	NdisFreeSpinLock(&pAd->BulkInLock);
	NdisFreeSpinLock(&pAd->CmdRspLock);
	NdisFreeSpinLock(&pAd->MLMEBulkOutLock);

	NdisFreeSpinLock(&pAd->CmdQLock);
#ifdef CONFIG_ATE
	NdisFreeSpinLock(&pAd->GenericLock);
#endif /* CONFIG_ATE */
	/* Clear all pending bulk-out request flags.*/
	RTUSB_CLEAR_BULK_FLAG(pAd, 0xffffffff);
	
	for (i = 0; i < NUM_OF_TX_RING; i++)
	{
		NdisFreeSpinLock(&pAd->TxContextQueueLock[i]);
	}
	
#ifdef MULTI_WMM_SUPPORT
	for (i = 0; i < NUM_OF_WMM1_TX_RING; i++)
	{
		NdisFreeSpinLock(&pAd->TxContextQueueWmm1Lock[i]);
	}
#endif /* MULTI_WMM_SUPPORT */
	
/*
	NdisFreeSpinLock(&pAd->MacTabLock);
	for(i=0; i<MAX_LEN_OF_BA_REC_TABLE; i++)
	{
		NdisFreeSpinLock(&pAd->BATable.BARecEntry[i].RxReRingLock);
	}
*/
}


/*
========================================================================
Routine Description:
	Calls USB_InterfaceStop and frees memory allocated for the URBs
    calls NdisMDeregisterDevice and frees the memory
    allocated in VNetInitialize for the Adapter Object

Arguments:
	*pAd				the raxx interface data pointer

Return Value:
	None

Note:
========================================================================
*/
VOID RTMPFreeTxRxRingMemory(PRTMP_ADAPTER pAd)
{
	UINT                i, acidx;
	PTX_CONTEXT			pNullContext   = &pAd->NullContext;
	PTX_CONTEXT			pPsPollContext = &pAd->PsPollContext;
	PCMD_RSP_CONTEXT pCmdRspEventContext = &pAd->CmdRspEventContext;
#ifdef USB_BULK_BUF_ALIGMENT
	INT ringidx;
#endif /* USB_BULK_BUF_ALIGMENT */

	DBGPRINT(RT_DEBUG_ERROR, ("---> RTMPFreeTxRxRingMemory\n"));

	NdisFreeSpinLock(&pAd->RxFreeQLock);
	NdisFreeSpinLock(&pAd->RxProcessingQLock);
	NdisFreeSpinLock(&pAd->RxBulkInQLock);

	/* Free all resources for the RECEIVE buffer queue.*/
	for(i=0; i<(RX_RING_SIZE); i++)
	{
		PRX_CONTEXT  pRxContext = &(pAd->RxContext[i]);
		if (pRxContext)
			RTMPFreeUsbBulkBufStruct(pAd, 
										&pRxContext->pUrb, 
										(PUCHAR *)&pRxContext->TransferBuffer, 
										MAX_RXBULK_SIZE, 
										pRxContext->data_dma);
	}

	/* Command Response */
	RTMPFreeUsbBulkBufStruct(pAd,
							 &pCmdRspEventContext->pUrb,
							 (PUCHAR *)&pCmdRspEventContext->CmdRspBuffer,
							 CMD_RSP_BULK_SIZE,
							 pCmdRspEventContext->data_dma); 
					


	/* Free PsPoll frame resource*/
	RTMPFreeUsbBulkBufStruct(pAd, 
								&pPsPollContext->pUrb, 
								(PUCHAR *)&pPsPollContext->TransferBuffer, 
								sizeof(TX_BUFFER), 
								pPsPollContext->data_dma);

#ifdef CONFIG_PRE_ALLOC        
   	if (NULL != pPsPollContext->TransferBuffer) {
		POS_COOKIE pObj = (POS_COOKIE) pAd->OS_Cookie;
       	RTUSB_URB_FREE_BUFFER(pObj->pUsb_Dev, sizeof(TX_BUFFER), pPsPollContext->TransferBuffer, pPsPollContext->data_dma);
        pPsPollContext->TransferBuffer = NULL;
    }
#endif
	/* Free NULL frame resource*/
	RTMPFreeUsbBulkBufStruct(pAd, 
								&pNullContext->pUrb, 
								(PUCHAR *)&pNullContext->TransferBuffer, 
								sizeof(TX_BUFFER), 
								pNullContext->data_dma);
#ifdef CONFIG_PRE_ALLOC        
    if (NULL != pNullContext->TransferBuffer) {
		POS_COOKIE pObj = (POS_COOKIE) pAd->OS_Cookie;
       	RTUSB_URB_FREE_BUFFER(pObj->pUsb_Dev, sizeof(TX_BUFFER), pNullContext->TransferBuffer, pNullContext->data_dma);
        pNullContext->TransferBuffer = NULL;
    }
#endif

	/* Free mgmt frame resource*/
	for(i = 0; i < MGMT_RING_SIZE; i++)
	{
		PTX_CONTEXT pMLMEContext = (PTX_CONTEXT)pAd->MgmtRing.Cell[i].AllocVa;
		if (pMLMEContext)
		{
			if (NULL != pMLMEContext->pUrb)
			{
				RTUSB_UNLINK_URB(pMLMEContext->pUrb);
				RTUSB_FREE_URB(pMLMEContext->pUrb);
				pMLMEContext->pUrb = NULL;
			}
		}
		
		if (NULL != pAd->MgmtRing.Cell[i].pNdisPacket) 
		{
			RELEASE_NDIS_PACKET(pAd, pAd->MgmtRing.Cell[i].pNdisPacket, NDIS_STATUS_FAILURE);
			pAd->MgmtRing.Cell[i].pNdisPacket = NULL;
			if (pMLMEContext)
				pMLMEContext->TransferBuffer = NULL; 
		}
	}
	
	if (pAd->MgmtDescRing.AllocVa)
		os_free_mem(pAd, pAd->MgmtDescRing.AllocVa);
	
#ifdef MT_MAC
    if (pAd->BcnDescRing.AllocVa)
		os_free_mem(pAd, pAd->BcnDescRing.AllocVa);
#endif /* MT_MAC */
	
	/* Free Tx frame resource*/
	for (acidx = 0; acidx < NUM_OF_TX_RING; acidx++)
	{
		PHT_TX_CONTEXT pHTTXContext = &(pAd->TxContext[acidx]);
#ifdef USB_BULK_BUF_ALIGMENT
		if (pHTTXContext)
		{
			for(ringidx=0;ringidx < BUF_ALIGMENT_RINGSIZE ;ringidx++)	
			{
				RTMPFreeUsbBulkBufStruct(pAd, 
											&pHTTXContext->pUrb[ringidx], 
											(PUCHAR *)&pHTTXContext->TransferBuffer[ringidx], 
											sizeof(HTTX_BUFFER), 
											pHTTXContext->data_dma[ringidx]);
			}
		}
#else

		if (pHTTXContext)
			RTMPFreeUsbBulkBufStruct(pAd, 
										&pHTTXContext->pUrb, 
										(PUCHAR *)&pHTTXContext->TransferBuffer, 
										sizeof(HTTX_BUFFER), 
										pHTTXContext->data_dma);

#ifdef USB_IOT_WORKAROUND
		pHTTXContext = &(pAd->TxContext2[acidx]);
		if (pHTTXContext)
			RTMPFreeUsbBulkBufStruct(pAd, 
										&pHTTXContext->pUrb, 
										(PUCHAR *)&pHTTXContext->TransferBuffer, 
										sizeof(HTTX_BUFFER), 
										pHTTXContext->data_dma);	
#endif		
#endif /* USB_BULK_BUF_ALIGMENT */

	}
	
#ifdef MULTI_WMM_SUPPORT
	/* Free Tx frame resource*/
	for (acidx = 0; acidx < NUM_OF_WMM1_TX_RING; acidx++)
	{
		PHT_TX_CONTEXT pHTTXContext = &(pAd->TxContextWmm1[acidx]);
		if (pHTTXContext)
			RTMPFreeUsbBulkBufStruct(pAd, 
										&pHTTXContext->pUrb, 
										(PUCHAR *)&pHTTXContext->TransferBuffer, 
										sizeof(HTTX_BUFFER), 
										pHTTXContext->data_dma);
	}
#endif /* MULTI_WMM_SUPPORT */
	
	if (pAd->FragFrame.pFragPacket)
		RELEASE_NDIS_PACKET(pAd, pAd->FragFrame.pFragPacket, NDIS_STATUS_SUCCESS);


	DBGPRINT(RT_DEBUG_ERROR, ("<--- RTMPFreeTxRxRingMemory\n"));
}


/*
========================================================================
Routine Description:
    Initialize receive data structures.

Arguments:
    pAd					Pointer to our adapter

Return Value:
	NDIS_STATUS_SUCCESS
	NDIS_STATUS_RESOURCES

Note:
	Initialize all receive releated private buffer, include those define
	in RTMP_ADAPTER structure and all private data structures. The major
	work is to allocate buffer for each packet and chain buffer to 
	NDIS packet descriptor.
========================================================================
*/
NDIS_STATUS	NICInitRecv(RTMP_ADAPTER *pAd)
{
	UCHAR i;
	PCMD_RSP_CONTEXT pCmdRspEventContext = &pAd->CmdRspEventContext;
	unsigned long Flags;

	DBGPRINT(RT_DEBUG_TRACE, ("--> NICInitRecv\n"));
	
	RTMP_SPIN_LOCK_IRQSAVE(&pAd->RxFreeQLock, &Flags);
	DlListInit(&pAd->RxFreeQ);
	RTMP_SPIN_UNLOCK_IRQRESTORE(&pAd->RxFreeQLock, &Flags);

	RTMP_SPIN_LOCK_IRQSAVE(&pAd->RxProcessingQLock, &Flags);
	DlListInit(&pAd->RxProcessingQ);
	RTMP_SPIN_UNLOCK_IRQRESTORE(&pAd->RxProcessingQLock, &Flags);

	RTMP_SPIN_LOCK_IRQSAVE(&pAd->RxBulkInQLock, &Flags);
	DlListInit(&pAd->RxBulkInQ);
	RTMP_SPIN_UNLOCK_IRQRESTORE(&pAd->RxBulkInQLock, &Flags);

	for (i = 0; i < (RX_RING_SIZE); i++)
	{
		PRX_CONTEXT pRxContext = &(pAd->RxContext[i]);

		ASSERT((pRxContext->TransferBuffer != NULL));
		ASSERT((pRxContext->pUrb != NULL));
		
		NdisZeroMemory(pRxContext->TransferBuffer, MAX_RXBULK_SIZE);

		pRxContext->Id = i;
		pRxContext->pAd	= pAd;
		pRxContext->BulkInOffset = 0;
		pRxContext->URBCancel = FALSE;

		RTMP_SPIN_LOCK_IRQSAVE(&pAd->RxFreeQLock, &Flags);
		DlListAddTail(&pAd->RxFreeQ, &pRxContext->list);
		RTMP_SPIN_UNLOCK_IRQRESTORE(&pAd->RxFreeQLock, &Flags);
	}

	pCmdRspEventContext->pAd = pAd;
	NdisZeroMemory(pCmdRspEventContext->CmdRspBuffer, CMD_RSP_BULK_SIZE);

	DBGPRINT(RT_DEBUG_TRACE, ("<-- NICInitRecv()\n"));
	
	return NDIS_STATUS_SUCCESS;
}


/*
========================================================================
Routine Description:
    Initialize transmit data structures.

Arguments:
    pAd					Pointer to our adapter

Return Value:
	NDIS_STATUS_SUCCESS
	NDIS_STATUS_RESOURCES

Note:
========================================================================
*/
NDIS_STATUS	NICInitTransmit(
	IN	PRTMP_ADAPTER	pAd)
{
	UCHAR			i, acidx;
	NDIS_STATUS     Status = NDIS_STATUS_SUCCESS;
	PTX_CONTEXT		pNullContext   = &(pAd->NullContext);
	PTX_CONTEXT		pPsPollContext = &(pAd->PsPollContext);
	PTX_CONTEXT		pMLMEContext = NULL;
    PTX_CONTEXT		pBcnContext = NULL;
	PVOID			RingBaseVa;
	RTMP_MGMT_RING  *pMgmtRing;
#ifdef MT_MAC
    RTMP_BCN_RING  *pBcnRing;
#endif
	PVOID pTransferBuffer;
	PURB	pUrb;
	ra_dma_addr_t data_dma;
#ifdef USB_BULK_BUF_ALIGMENT
	INT ringidx;
#endif /* USB_BULK_BUF_ALIGMENT */
	
	DBGPRINT(RT_DEBUG_TRACE, ("--> NICInitTransmit\n"));


	/* Init 4 set of Tx parameters*/
	rtmp_tx_swq_init(pAd);
	for(acidx = 0; acidx < NUM_OF_TX_RING; acidx++)
	{
		/* Next Local tx ring pointer waiting for buck out*/
		pAd->NextBulkOutIndex[acidx] = acidx;
		pAd->BulkOutPending[acidx] = FALSE; /* Buck Out control flag	*/
	}

#ifdef MULTI_WMM_SUPPORT
	/* Init 1 set of Wmm1 Tx parameters*/
	for(acidx = 0; acidx < NUM_OF_WMM1_TX_RING; acidx++)
	{
		/* Next Local tx ring pointer waiting for buck out*/
		pAd->NextBulkOutWmm1Index[acidx] = acidx;
		pAd->BulkOutWmm1Pending[acidx] = FALSE; /* Buck Out control flag	*/
	}
#endif /* MULTI_WMM_SUPPORT */

	do
	{
		
		/* TX_RING_SIZE, 4 ACs*/
		
		for(acidx=0; acidx < NUM_OF_TX_RING; acidx++)
		{
			PHT_TX_CONTEXT	pHTTXContext = &(pAd->TxContext[acidx]);
#ifdef USB_BULK_BUF_ALIGMENT
			for(ringidx=0;ringidx < BUF_ALIGMENT_RINGSIZE ;ringidx++)
			{

				pTransferBuffer = pHTTXContext->TransferBuffer[ringidx];
				pUrb = pHTTXContext->pUrb[ringidx];
				data_dma = pHTTXContext->data_dma[ringidx];
				
				ASSERT( (pTransferBuffer != NULL));
				ASSERT( (pUrb != NULL));

				pHTTXContext->TransferBuffer[ringidx] = pTransferBuffer;
				pHTTXContext->pUrb[ringidx] = pUrb;
				pHTTXContext->data_dma[ringidx] = data_dma;
				
				NdisZeroMemory(pHTTXContext->TransferBuffer[ringidx]->Aggregation, 4);			

			}
#else

			pTransferBuffer = pHTTXContext->TransferBuffer;
			pUrb = pHTTXContext->pUrb;
			data_dma = pHTTXContext->data_dma;
			
			ASSERT( (pTransferBuffer != NULL));
			ASSERT( (pUrb != NULL));
			
			NdisZeroMemory(pHTTXContext, sizeof(HT_TX_CONTEXT));
			pHTTXContext->TransferBuffer = pTransferBuffer;
			pHTTXContext->pUrb = pUrb;
			pHTTXContext->data_dma = data_dma;
			
			NdisZeroMemory(pHTTXContext->TransferBuffer->Aggregation, 4);			
#endif /* USB_BULK_BUF_ALIGMENT */	
			
			pHTTXContext->pAd = pAd;
			pHTTXContext->BulkOutPipeId = acidx;
			pHTTXContext->bRingEmpty = TRUE;
			pHTTXContext->bCopySavePad = FALSE;

			pAd->BulkOutPending[acidx] = FALSE;
		}

#ifdef MULTI_WMM_SUPPORT
		/* TX_RING_SIZE, 4 ACs*/		
		for(acidx=0; acidx<NUM_OF_WMM1_TX_RING; acidx++)
		{
			PHT_TX_CONTEXT	pHTTXContext = &(pAd->TxContextWmm1[acidx]);

			pTransferBuffer = pHTTXContext->TransferBuffer;
			pUrb = pHTTXContext->pUrb;
			data_dma = pHTTXContext->data_dma;
			
			ASSERT( (pTransferBuffer != NULL));
			ASSERT( (pUrb != NULL));
			
			NdisZeroMemory(pHTTXContext, sizeof(HT_TX_CONTEXT));
			pHTTXContext->TransferBuffer = pTransferBuffer;
			pHTTXContext->pUrb = pUrb;
			pHTTXContext->data_dma = data_dma;
			
			NdisZeroMemory(pHTTXContext->TransferBuffer->Aggregation, 4);			
		
			pHTTXContext->pAd = pAd;
			pHTTXContext->BulkOutPipeId = acidx;
			pHTTXContext->bRingEmpty = TRUE;
			pHTTXContext->bCopySavePad = FALSE;

			pAd->BulkOutWmm1Pending[acidx] = FALSE;
		}
#endif /* MULTI_WMM_SUPPORT */
		
		/* MGMT_RING_SIZE*/
		NdisZeroMemory(pAd->MgmtDescRing.AllocVa, pAd->MgmtDescRing.AllocSize);
		RingBaseVa = pAd->MgmtDescRing.AllocVa;

		/* Initialize MGMT Ring and associated buffer memory*/
		pMgmtRing = &pAd->MgmtRing;
		for (i = 0; i < MGMT_RING_SIZE; i++)
		{
			/* link the pre-allocated Mgmt buffer to MgmtRing.Cell*/
			pMgmtRing->Cell[i].AllocSize = sizeof(TX_CONTEXT);
			pMgmtRing->Cell[i].AllocVa = RingBaseVa;
			pMgmtRing->Cell[i].pNdisPacket = NULL;
			pMgmtRing->Cell[i].pNextNdisPacket = NULL;

			/*Allocate URB for MLMEContext*/
			pMLMEContext = (PTX_CONTEXT) pAd->MgmtRing.Cell[i].AllocVa;
			pMLMEContext->pUrb = RTUSB_ALLOC_URB(0);
			if (pMLMEContext->pUrb == NULL)
			{
				DBGPRINT(RT_DEBUG_ERROR, ("<-- ERROR in Alloc TX MLMEContext[%d] urb!! \n", i));
				Status = NDIS_STATUS_RESOURCES;
				goto err; 
			}
			pMLMEContext->pAd = pAd;
			pMLMEContext->SelfIdx = i;
			
			/* Offset to next ring descriptor address*/
			RingBaseVa = (PUCHAR) RingBaseVa + sizeof(TX_CONTEXT);
		}
		DBGPRINT(RT_DEBUG_TRACE, ("MGMT Ring: total %d entry allocated\n", i));
		
		/*pAd->MgmtRing.TxSwFreeIdx = (MGMT_RING_SIZE - 1);*/
		pAd->MgmtRing.TxSwFreeIdx = MGMT_RING_SIZE;
		pAd->MgmtRing.TxCpuIdx = 0;
		pAd->MgmtRing.TxDmaIdx = 0;

#ifdef MT_MAC
        /* BCN_RING_SIZE*/
		NdisZeroMemory(pAd->BcnDescRing.AllocVa, pAd->BcnDescRing.AllocSize);
		RingBaseVa = pAd->BcnDescRing.AllocVa;
        /* Initialize BCN Ring and associated buffer memory*/
		pBcnRing = &pAd->BcnRing;
		for (i = 0; i < BCN_RING_SIZE; i++)
		{
			/* link the pre-allocated Mgmt buffer to BcnRing.Cell*/
			pBcnRing->Cell[i].AllocSize = sizeof(TX_CONTEXT);
			pBcnRing->Cell[i].AllocVa = RingBaseVa;
			pBcnRing->Cell[i].pNdisPacket = NULL;
			pBcnRing->Cell[i].pNextNdisPacket = NULL;

			/*Allocate URB for BCNContext*/
			pBcnContext = (PTX_CONTEXT) pAd->BcnRing.Cell[i].AllocVa;
			pBcnContext->pUrb = RTUSB_ALLOC_URB(0);
			if (pBcnContext->pUrb == NULL)
			{
				DBGPRINT(RT_DEBUG_ERROR, ("<-- ERROR in Alloc TX pBcnContext[%d] urb!! \n", i));
				Status = NDIS_STATUS_RESOURCES;
				goto err;
			}
			pBcnContext->pAd = pAd;
			pBcnContext->SelfIdx = i;

			/* Offset to next ring descriptor address*/
			RingBaseVa = (PUCHAR) RingBaseVa + sizeof(TX_CONTEXT);
		}
		DBGPRINT(RT_DEBUG_TRACE, ("BCN Ring: total %d entry allocated\n", i));

		pAd->BcnRing.TxSwFreeIdx = BCN_RING_SIZE;
		pAd->BcnRing.TxCpuIdx = 0;
		pAd->BcnRing.TxDmaIdx = 0;
#endif /* MT_MAC */

		/* NullContext*/
		
		pTransferBuffer = pNullContext->TransferBuffer;
		pUrb = pNullContext->pUrb;
		data_dma = pNullContext->data_dma;
		
		NdisZeroMemory(pNullContext, sizeof(TX_CONTEXT));
		pNullContext->TransferBuffer = pTransferBuffer;
		pNullContext->pUrb = pUrb;
		pNullContext->data_dma = data_dma;
		pNullContext->pAd = pAd;


		
		/* PsPollContext*/
		
		pTransferBuffer = pPsPollContext->TransferBuffer;
		pUrb = pPsPollContext->pUrb;
		data_dma = pPsPollContext->data_dma;
		NdisZeroMemory(pPsPollContext, sizeof(TX_CONTEXT));
		pPsPollContext->TransferBuffer = pTransferBuffer;
		pPsPollContext->pUrb = pUrb;
		pPsPollContext->data_dma = data_dma;
		pPsPollContext->pAd = pAd;
		pPsPollContext->LastOne = TRUE;

	}   while (FALSE);


	DBGPRINT(RT_DEBUG_TRACE, ("<-- NICInitTransmit(Status=%d)\n", Status));

	return Status;

	/* --------------------------- ERROR HANDLE --------------------------- */
err:	
	if (pAd->MgmtDescRing.AllocVa)
	{
		pMgmtRing = &pAd->MgmtRing;
		for(i = 0; i < MGMT_RING_SIZE; i++)
		{
			pMLMEContext = (PTX_CONTEXT) pAd->MgmtRing.Cell[i].AllocVa;
			if (pMLMEContext)
			{
				RTMPFreeUsbBulkBufStruct(pAd, 
											&pMLMEContext->pUrb, 
											(PUCHAR *)&pMLMEContext->TransferBuffer, 
											sizeof(TX_BUFFER), 
											pMLMEContext->data_dma);
#ifdef CONFIG_PRE_ALLOC        
   				if (NULL != pMLMEContext->TransferBuffer) {
					POS_COOKIE pObj = (POS_COOKIE) pAd->OS_Cookie;
       				RTUSB_URB_FREE_BUFFER(pObj->pUsb_Dev, sizeof(TX_BUFFER), pMLMEContext->TransferBuffer, pMLMEContext->data_dma);
        			pMLMEContext->TransferBuffer = NULL;
    			}
#endif											
			}								
		}
		os_free_mem(pAd, pAd->MgmtDescRing.AllocVa);
		pAd->MgmtDescRing.AllocVa = NULL;
	}

#ifdef MT_MAC
	if (pAd->BcnDescRing.AllocVa)
	{
		pBcnRing = &pAd->BcnRing;
		for(i = 0; i < BCN_RING_SIZE; i++)
		{
			pBcnContext = (PTX_CONTEXT) pAd->BcnRing.Cell[i].AllocVa;
			if (pBcnContext)
				RTMPFreeUsbBulkBufStruct(pAd,
											&pBcnContext->pUrb,
											(PUCHAR *)&pBcnContext->TransferBuffer,
											sizeof(TX_BUFFER),
											pBcnContext->data_dma);
		}
		os_free_mem(pAd, pAd->BcnDescRing.AllocVa);
		pAd->BcnDescRing.AllocVa = NULL;
	}
#endif
	/* Here we didn't have any pre-allocated memory need to free.*/
	
	return Status;	
}


/*
========================================================================
Routine Description:
    Allocate DMA memory blocks for send, receive.

Arguments:
    pAd					Pointer to our adapter

Return Value:
	NDIS_STATUS_SUCCESS
	NDIS_STATUS_FAILURE
	NDIS_STATUS_RESOURCES

Note:
========================================================================
*/
NDIS_STATUS	RTMPAllocTxRxRingMemory(PRTMP_ADAPTER pAd)
{	
	NDIS_STATUS Status = NDIS_STATUS_FAILURE;
	PTX_CONTEXT pNullContext   = &(pAd->NullContext);
	PTX_CONTEXT pPsPollContext = &(pAd->PsPollContext);
	PCMD_RSP_CONTEXT pCmdRspEventContext = &(pAd->CmdRspEventContext);
	INT i, acidx;
	ULONG Flags;
#ifdef USB_BULK_BUF_ALIGMENT
	INT ringidx;
#endif /* USB_BULK_BUF_ALIGMENT */

	DBGPRINT(RT_DEBUG_TRACE, ("--> RTMPAllocTxRxRingMemory\n"));

	do
	{
		
		/* Init send data structures and related parameters*/
		
		
		/* TX_RING_SIZE, 4 ACs*/
		
		for(acidx=0; acidx < NUM_OF_TX_RING; acidx++)
		{
			PHT_TX_CONTEXT	pHTTXContext = &(pAd->TxContext[acidx]);

			NdisZeroMemory(pHTTXContext, sizeof(HT_TX_CONTEXT));
			/*Allocate URB and bulk buffer*/
#ifdef USB_BULK_BUF_ALIGMENT

			/*Allocate URB and bulk buffer*/
			for(ringidx=0;ringidx < BUF_ALIGMENT_RINGSIZE ;ringidx++)
			{
				printk("allocate tx ringidx %d \n",ringidx);
				Status = RTMPAllocUsbBulkBufStruct(pAd, 
													&pHTTXContext->pUrb[ringidx], 
													(PVOID *)&pHTTXContext->TransferBuffer[ringidx], 
													sizeof(HTTX_BUFFER),													 
													&pHTTXContext->data_dma[ringidx],
													"HTTxContext");


				if (Status != NDIS_STATUS_SUCCESS)
					goto err;

			}
#else

			Status = RTMPAllocUsbBulkBufStruct(pAd, 
												&pHTTXContext->pUrb, 
												(PVOID *)&pHTTXContext->TransferBuffer, 
												sizeof(HTTX_BUFFER), 
												&pHTTXContext->data_dma,
												"HTTxContext");
			if (Status != NDIS_STATUS_SUCCESS)
				goto err;

#ifdef USB_IOT_WORKAROUND
			pHTTXContext = &(pAd->TxContext2[acidx]);
			Status = RTMPAllocUsbBulkBufStruct(pAd, 
												&pHTTXContext->pUrb, 
												(PVOID *)&pHTTXContext->TransferBuffer, 
												sizeof(HTTX_BUFFER), 
												&pHTTXContext->data_dma,
												"HTTxContext2");
			if (Status != NDIS_STATUS_SUCCESS)
				goto err;
#endif

#ifdef CONFIG_PRE_ALLOC				
			pHTTXContext->TransferBuffer = RTMPQMemAddr(sizeof(HTTX_BUFFER), &pHTTXContext->data_dma, acidx+BLK_TX0);
			if (pHTTXContext->TransferBuffer == NULL)
				goto err;
			printk("[%d] pHTTXContext TransferBuffer=0x%x,DMA=0x%x\n", acidx, pHTTXContext->TransferBuffer, pHTTXContext->data_dma);		
#endif					
#endif /* USB_BULK_BUF_ALIGMENT */

		}

#ifdef MULTI_WMM_SUPPORT
		/* TX_RING_SIZE, 4 ACs*/
		for(acidx=0; acidx<NUM_OF_WMM1_TX_RING; acidx++)
		{
			PHT_TX_CONTEXT	pHTTXContext = &(pAd->TxContextWmm1[acidx]);

			NdisZeroMemory(pHTTXContext, sizeof(HT_TX_CONTEXT));
			/*Allocate URB and bulk buffer*/

#ifdef USB_BULK_BUF_ALIGMENT

			/*Allocate URB and bulk buffer*/
			for(ringidx=0;ringidx < BUF_ALIGMENT_RINGSIZE ;ringidx++)
			{
				printk("allocate tx ringidx %d \n",ringidx);
				Status = RTMPAllocUsbBulkBufStruct(pAd, 
													&pHTTXContext->pUrb[ringidx], 
													(PVOID *)&pHTTXContext->TransferBuffer[ringidx], 
													sizeof(HTTX_BUFFER),													 
													&pHTTXContext->data_dma[ringidx],
													"HTTxContext");


				if (Status != NDIS_STATUS_SUCCESS)
					goto err;

			}
#else

			Status = RTMPAllocUsbBulkBufStruct(pAd, 
												&pHTTXContext->pUrb, 
												(PVOID *)&pHTTXContext->TransferBuffer, 
												sizeof(HTTX_BUFFER), 
												&pHTTXContext->data_dma,
												"HTTxWmm1Context");
			if (Status != NDIS_STATUS_SUCCESS)
				goto err;

#endif /* USB_BULK_BUF_ALIGMENT */

		}
#endif /* MULTI_WMM_SUPPORT */		
		/* MGMT_RING_SIZE*/
		
		/* Allocate MGMT ring descriptor's memory*/
		pAd->MgmtDescRing.AllocSize = MGMT_RING_SIZE * sizeof(TX_CONTEXT);
		os_alloc_mem(pAd, (PUCHAR *)(&pAd->MgmtDescRing.AllocVa), pAd->MgmtDescRing.AllocSize);
		if (pAd->MgmtDescRing.AllocVa == NULL)
		{
			DBGPRINT_ERR(("Failed to allocate a big buffer for MgmtDescRing!\n"));
			Status = NDIS_STATUS_RESOURCES;
			goto err;
		}

#ifdef MT_MAC
		/* Initialize BCN Ring and associated buffer memory */
		/* Allocate BCN ring descriptor's memory*/
		pAd->BcnDescRing.AllocSize = BCN_RING_SIZE * sizeof(TX_CONTEXT);
		os_alloc_mem(pAd, (PUCHAR *)(&pAd->BcnDescRing.AllocVa), pAd->BcnDescRing.AllocSize);
		if (pAd->BcnDescRing.AllocVa == NULL)
		{
			DBGPRINT_ERR(("Failed to allocate a big buffer for MgmtDescRing!\n"));
			Status = NDIS_STATUS_RESOURCES;
			goto err;
		}
#endif /* MT_MAC */

		
		/* NullContext*/
		
		NdisZeroMemory(pNullContext, sizeof(TX_CONTEXT));
		/*Allocate URB*/
		Status = RTMPAllocUsbBulkBufStruct(pAd, 
											&pNullContext->pUrb, 
											(PVOID *)&pNullContext->TransferBuffer, 
											sizeof(TX_BUFFER), 
											&pNullContext->data_dma,
											"TxNullContext");
		if (Status != NDIS_STATUS_SUCCESS)
			goto err;

		
		/* PsPollContext*/
		
		NdisZeroMemory(pPsPollContext, sizeof(TX_CONTEXT));
		/*Allocate URB*/
		Status = RTMPAllocUsbBulkBufStruct(pAd, 
											&pPsPollContext->pUrb, 
											(PVOID *)&pPsPollContext->TransferBuffer, 
											sizeof(TX_BUFFER), 
											&pPsPollContext->data_dma,
											"TxPsPollContext");
		if (Status != NDIS_STATUS_SUCCESS)
			goto err;
		
		NdisAllocateSpinLock(pAd, &pAd->RxFreeQLock);
		NdisAllocateSpinLock(pAd, &pAd->RxProcessingQLock);
		NdisAllocateSpinLock(pAd, &pAd->RxBulkInQLock);
		
		RTMP_SPIN_LOCK_IRQSAVE(&pAd->RxFreeQLock, &Flags);
		DlListInit(&pAd->RxFreeQ);
		RTMP_SPIN_UNLOCK_IRQRESTORE(&pAd->RxFreeQLock, &Flags);

		RTMP_SPIN_LOCK_IRQSAVE(&pAd->RxProcessingQLock, &Flags);
		DlListInit(&pAd->RxProcessingQ);
		RTMP_SPIN_UNLOCK_IRQRESTORE(&pAd->RxProcessingQLock, &Flags);

		RTMP_SPIN_LOCK_IRQSAVE(&pAd->RxBulkInQLock, &Flags);
		DlListInit(&pAd->RxBulkInQ);
		RTMP_SPIN_UNLOCK_IRQRESTORE(&pAd->RxBulkInQLock, &Flags);

		
		/* Init receive data structures and related parameters*/
		for (i = 0; i < (RX_RING_SIZE); i++)
		{
			PRX_CONTEXT  pRxContext = &(pAd->RxContext[i]);

			/*Allocate URB*/
			Status = RTMPAllocUsbBulkBufStruct(pAd, 
												&pRxContext->pUrb, 
												(PVOID *)&pRxContext->TransferBuffer, 
												MAX_RXBULK_SIZE, 
												&pRxContext->data_dma, 
												"RxContext");
			if (Status != NDIS_STATUS_SUCCESS)
				goto err;		
#ifdef CONFIG_PRE_ALLOC		
			pRxContext->TransferBuffer = RTMPQMemAddr(MAX_RXBULK_SIZE, &pRxContext->data_dma, i+BLK_RX0);
			if (pRxContext->TransferBuffer == NULL)
				goto err;
		
			printk("[%d] pRxContext TransferBuffer=0x%x,DMA=0x%x\n", i, pRxContext->TransferBuffer, pRxContext->data_dma);							
#endif
		}

		/* Init command response event related parameters */
		Status = RTMPAllocUsbBulkBufStruct(pAd,
										   &pCmdRspEventContext->pUrb,
										   (PVOID *)&pCmdRspEventContext->CmdRspBuffer,
										   CMD_RSP_BULK_SIZE,
										   &pCmdRspEventContext->data_dma,
										   "CmdRspEventContext");

		if (Status != NDIS_STATUS_SUCCESS)
			goto err;
#ifdef CONFIG_PRE_ALLOC		
			pCmdRspEventContext->CmdRspBuffer = RTMPQMemAddr(CMD_RSP_BULK_SIZE, &pCmdRspEventContext->data_dma, BLK_CMD);
			if (pCmdRspEventContext->CmdRspBuffer == NULL)
				goto err;
		
			printk("[%d] pCmdRspEventContext TransferBuffer=0x%x,DMA=0x%x\n", i, pCmdRspEventContext->CmdRspBuffer, pCmdRspEventContext->data_dma);							
#endif
		

		NdisZeroMemory(&pAd->FragFrame, sizeof(FRAGMENT_FRAME));
		pAd->FragFrame.pFragPacket =  RTMP_AllocateFragPacketBuffer(pAd, RX_BUFFER_NORMSIZE);

		if (pAd->FragFrame.pFragPacket == NULL)
		{
			Status = NDIS_STATUS_RESOURCES;
		}
	} while (FALSE);
	
	DBGPRINT_S(("<-- RTMPAllocTxRxRingMemory, Status=%x\n", Status));

	return Status;

err:
	Status = NDIS_STATUS_RESOURCES;
	RTMPFreeTxRxRingMemory(pAd);
	
	return Status;
}


NDIS_STATUS RTMPInitTxRxRingMemory(RTMP_ADAPTER *pAd)
{
	INT num;
	NDIS_STATUS Status;
	unsigned long Flags;

	/* Init the CmdQ and CmdQLock*/
	NdisAllocateSpinLock(pAd, &pAd->CmdQLock);	
	NdisAcquireSpinLock(&pAd->CmdQLock);
	RTInitializeCmdQ(&pAd->CmdQ);
	NdisReleaseSpinLock(&pAd->CmdQLock);

	NdisAllocateSpinLock(pAd, &pAd->MLMEBulkOutLock);
	NdisAllocateSpinLock(pAd, &pAd->BulkInLock);
	NdisAllocateSpinLock(pAd, &pAd->CmdRspLock);

	for(num =0 ; num < 6; num++)
	{
		NdisAllocateSpinLock(pAd, &pAd->BulkOutLock[num]);
	}


	for (num = 0; num < NUM_OF_TX_RING; num++)
	{
		NdisAllocateSpinLock(pAd, &pAd->TxContextQueueLock[num]);
	}
	
#ifdef MULTI_WMM_SUPPORT
	for(num =0 ; num < NUM_OF_WMM1_TX_RING; num++)
	{
		NdisAllocateSpinLock(pAd, &pAd->BulkOutWmm1Lock[num]);
	}


	for (num = 0; num < NUM_OF_WMM1_TX_RING; num++)
	{
		NdisAllocateSpinLock(pAd, &pAd->TxContextQueueWmm1Lock[num]);
	}
#endif /* MULTI_WMM_SUPPORT */

#ifdef CONFIG_ATE
	NdisAllocateSpinLock(pAd, &pAd->GenericLock);
#endif /* CONFIG_ATE */

	NICInitRecv(pAd);


	Status = NICInitTransmit(pAd);
	
	return Status;
	
}


#else

/*
========================================================================
Routine Description:
    Initialize receive data structures.

Arguments:
    pAd					Pointer to our adapter

Return Value:
	NDIS_STATUS_SUCCESS
	NDIS_STATUS_RESOURCES

Note:
	Initialize all receive releated private buffer, include those define
	in RTMP_ADAPTER structure and all private data structures. The mahor
	work is to allocate buffer for each packet and chain buffer to 
	NDIS packet descriptor.
========================================================================
*/
NDIS_STATUS	NICInitRecv(
	IN	PRTMP_ADAPTER	pAd)
{
	UCHAR				i;
	NDIS_STATUS			Status = NDIS_STATUS_SUCCESS;
	POS_COOKIE			pObj = (POS_COOKIE) pAd->OS_Cookie;
	PCMD_RSP_CONTEXT pCmdRspEventContext = &pAd->CmdRspEventContext;

	DBGPRINT(RT_DEBUG_TRACE, ("--> NICInitRecv\n"));
	pObj = pObj;

	pAd->PendingRx = 0;
	pAd->NextRxBulkInPosition 	= 0;

	for (i = 0; i < (RX_RING_SIZE); i++)
	{
		PRX_CONTEXT  pRxContext = &(pAd->RxContext[i]);

		/*Allocate URB*/
		pRxContext->pUrb = RTUSB_ALLOC_URB(0);		
		if (pRxContext->pUrb == NULL) 
		{
			Status = NDIS_STATUS_RESOURCES;
			goto out1;
		}

		/* Allocate transfer buffer*/
		pRxContext->TransferBuffer = RTUSB_URB_ALLOC_BUFFER(pObj->pUsb_Dev, MAX_RXBULK_SIZE, &pRxContext->data_dma);
		if (pRxContext->TransferBuffer == NULL)
		{
			Status = NDIS_STATUS_RESOURCES;
			goto out1;
		}

		NdisZeroMemory(pRxContext->TransferBuffer, MAX_RXBULK_SIZE);

		pRxContext->Id = i;
		pRxContext->pAd	= pAd;
		pRxContext->BulkInOffset = 0;
		pRxContext->URBCancel = FALSE;

		RTMP_SPIN_LOCK_IRQSAVE(&pAd->RxFreeQLock, &Flags);
		DlListAddTail(&pAd->RxFreeQ, &pRxContext->list);
		RTMP_SPIN_UNLOCK_IRQRESTORE(&pAd->RxFreeQLock, &Flags);
	}

	pCmdRspEventContext->pAd = pAd;
	NdisZeroMemory(pCmdRspEventContext->TransferBuffer, CMD_RSP_BULK_SIZE);

	DBGPRINT(RT_DEBUG_TRACE, ("<-- NICInitRecv(Status=%d)\n", Status));
	return Status;

out1:
	for (i = 0; i < (RX_RING_SIZE); i++)
	{
		PRX_CONTEXT  pRxContext = &(pAd->RxContext[i]);

		if (NULL != pRxContext->TransferBuffer)
		{
			RTUSB_URB_FREE_BUFFER(pObj->pUsb_Dev, MAX_RXBULK_SIZE, 
								pRxContext->TransferBuffer, pRxContext->data_dma);
			pRxContext->TransferBuffer = NULL;
		}

		if (NULL != pRxContext->pUrb)
		{
			RTUSB_UNLINK_URB(pRxContext->pUrb);
			RTUSB_FREE_URB(pRxContext->pUrb);
			pRxContext->pUrb = NULL;
		}
	}
	
	return Status;
}


/*
========================================================================
Routine Description:
    Initialize transmit data structures.

Arguments:
    pAd					Pointer to our adapter

Return Value:
	NDIS_STATUS_SUCCESS
	NDIS_STATUS_RESOURCES

Note:
========================================================================
*/
NDIS_STATUS	NICInitTransmit(
	IN	PRTMP_ADAPTER	pAd)
{
	UCHAR			i, acidx;
	NDIS_STATUS     Status = NDIS_STATUS_SUCCESS;
	PTX_CONTEXT		pNullContext   = &(pAd->NullContext);
	PTX_CONTEXT		pPsPollContext = &(pAd->PsPollContext);
	PTX_CONTEXT		pMLMEContext = NULL;
    PTX_CONTEXT		pBcnContext = NULL;
	POS_COOKIE		pObj = (POS_COOKIE) pAd->OS_Cookie;
	PVOID			RingBaseVa;
	RTMP_MGMT_RING  *pMgmtRing;
#ifdef MT_MAC
    RTMP_BCN_RING   *pBcnRing;
#endif
#ifdef USB_BULK_BUF_ALIGMENT
	INT ringidx;
#endif /* USB_BULK_BUF_ALIGMENT */

	DBGPRINT(RT_DEBUG_TRACE, ("--> NICInitTransmit\n"));
	pObj = pObj;

	rtmp_tx_swq_init(pAd);
	/* Init 4 set of Tx parameters*/
	for(acidx = 0; acidx < NUM_OF_TX_RING; acidx++)
	{
		/* Next Local tx ring pointer waiting for buck out*/
		pAd->NextBulkOutIndex[acidx] = acidx;
		pAd->BulkOutPending[acidx] = FALSE; /* Buck Out control flag	*/
	}

#ifdef MULTI_WMM_SUPPORT
	/* Init 1 set of Wmm1 Tx parameters*/
	for(acidx = 0; acidx < NUM_OF_WMM1_TX_RING; acidx++)
	{
		/* Next Local tx ring pointer waiting for buck out*/
		pAd->NextBulkOutWmm1Index[acidx] = acidx;
		pAd->BulkOutWmm1Pending[acidx] = FALSE; /* Buck Out control flag	*/
	}
#endif /* MULTI_WMM_SUPPORT */
	do
	{
		
		/* TX_RING_SIZE, 4 ACs*/
		
		for(acidx=0; acidx < NUM_OF_TX_RING; acidx++)
		{
			PHT_TX_CONTEXT	pHTTXContext = &(pAd->TxContext[acidx]);

			NdisZeroMemory(pHTTXContext, sizeof(HT_TX_CONTEXT));
			/*Allocate URB*/
#ifdef USB_BULK_BUF_ALIGMENT
			for(ringidx=0;ringidx < BUF_ALIGMENT_RINGSIZE ;ringidx++)
			{

				Status = RTMPAllocUsbBulkBufStruct(pAd, 
													&pHTTXContext->pUrb[ringidx], 
													(PVOID *)&pHTTXContext->TransferBuffer[ringidx], 
													sizeof(HTTX_BUFFER),
													&pHTTXContext->data_dma[ringidx], 
													"HTTxContext");
				if (Status != NDIS_STATUS_SUCCESS)
				{
					goto err;
				}
				NdisZeroMemory(pHTTXContext->TransferBuffer[ringidx]->Aggregation, 4);			
			}
	
#else

			Status = RTMPAllocUsbBulkBufStruct(pAd, 
												&pHTTXContext->pUrb, 
												(PVOID *)&pHTTXContext->TransferBuffer, 
												sizeof(HTTX_BUFFER), 
												&pHTTXContext->data_dma, 
												"HTTxContext");
			if (Status != NDIS_STATUS_SUCCESS)
				goto err;

#ifdef CONFIG_PRE_ALLOC
			pHTTXContext->TransferBuffer = RTMPQMemAddr(sizeof(HTTX_BUFFER), &pHTTXContext->data_dma, acidx+BLK_TX0);
			if (pHTTXContext->TransferBuffer == NULL)
				goto err;
			
			printk("[%d] pHTTXContext TransferBuffer=0x%x,DMA=0x%x\n", acidx, pHTTXContext->TransferBuffer, pHTTXContext->data_dma);			
#endif
			NdisZeroMemory(pHTTXContext->TransferBuffer->Aggregation, 4);			
#endif /* USB_BULK_BUF_ALIGMENT */

			pHTTXContext->pAd = pAd;
			pHTTXContext->pIrp = NULL;
			pHTTXContext->IRPPending = FALSE;
			pHTTXContext->NextBulkOutPosition = 0;
			pHTTXContext->ENextBulkOutPosition = 0;
			pHTTXContext->CurWritePosition = 0;
			pHTTXContext->CurWriteRealPos = 0;
			pHTTXContext->BulkOutSize = 0;
			pHTTXContext->BulkOutPipeId = acidx;
			pHTTXContext->bRingEmpty = TRUE;
			pHTTXContext->bCopySavePad = FALSE;
			pAd->BulkOutPending[acidx] = FALSE;
		}

#ifdef MULTI_WMM_SUPPORT
		/* TX_RING_SIZE, 4 ACs*/
		for(acidx=0; acidx<NUM_OF_WMM1_TX_RING; acidx++)
		{
			PHT_TX_CONTEXT	pHTTXContext = &(pAd->TxContextWmm1[acidx]);

			NdisZeroMemory(pHTTXContext, sizeof(HT_TX_CONTEXT));
			/*Allocate URB*/
#ifdef USB_BULK_BUF_ALIGMENT
			for(ringidx=0;ringidx < BUF_ALIGMENT_RINGSIZE ;ringidx++)
			{

				Status = RTMPAllocUsbBulkBufStruct(pAd, 
													&pHTTXContext->pUrb[ringidx], 
													(PVOID *)&pHTTXContext->TransferBuffer[ringidx], 
													sizeof(HTTX_BUFFER),
													&pHTTXContext->data_dma[ringidx], 
													"HTTxContext");
				if (Status != NDIS_STATUS_SUCCESS)
				{
					goto err;
				}
				NdisZeroMemory(pHTTXContext->TransferBuffer[ringidx]->Aggregation, 4);			
			}
	
#else


			Status = RTMPAllocUsbBulkBufStruct(pAd, 
												&pHTTXContext->pUrb, 
												(PVOID *)&pHTTXContext->TransferBuffer, 
												sizeof(HTTX_BUFFER), 
												&pHTTXContext->data_dma, 
												"HTTxContext");
			if (Status != NDIS_STATUS_SUCCESS)
				goto err;
		
			NdisZeroMemory(pHTTXContext->TransferBuffer->Aggregation, 4);			
#endif /* USB_BULK_BUF_ALIGMENT */

			pHTTXContext->pAd = pAd;
			pHTTXContext->pIrp = NULL;
			pHTTXContext->IRPPending = FALSE;
			pHTTXContext->NextBulkOutPosition = 0;
			pHTTXContext->ENextBulkOutPosition = 0;
			pHTTXContext->CurWritePosition = 0;
			pHTTXContext->CurWriteRealPos = 0;
			pHTTXContext->BulkOutSize = 0;
			pHTTXContext->BulkOutPipeId = acidx;
			pHTTXContext->bRingEmpty = TRUE;
			pHTTXContext->bCopySavePad = FALSE;
			pAd->BulkOutWmm1Pending[acidx] = FALSE;
		}
#endif /* MULTI_WMM_SUPPORT */		
		/* MGMT Ring*/
		
		
		/* Allocate MGMT ring descriptor's memory*/
		pAd->MgmtDescRing.AllocSize = MGMT_RING_SIZE * sizeof(TX_CONTEXT);
		os_alloc_mem(pAd, (PUCHAR *)(&pAd->MgmtDescRing.AllocVa), pAd->MgmtDescRing.AllocSize);
		if (pAd->MgmtDescRing.AllocVa == NULL)
		{
			DBGPRINT_ERR(("Failed to allocate a big buffer for MgmtDescRing!\n"));
			Status = NDIS_STATUS_RESOURCES;
			goto err;
		}
		NdisZeroMemory(pAd->MgmtDescRing.AllocVa, pAd->MgmtDescRing.AllocSize);
		RingBaseVa     = pAd->MgmtDescRing.AllocVa;

		/* Initialize MGMT Ring and associated buffer memory*/
		pMgmtRing = &pAd->MgmtRing;
		for (i = 0; i < MGMT_RING_SIZE; i++)
		{
			/* link the pre-allocated Mgmt buffer to MgmtRing.Cell*/
			pMgmtRing->Cell[i].AllocSize = sizeof(TX_CONTEXT);
			pMgmtRing->Cell[i].AllocVa = RingBaseVa;
			pMgmtRing->Cell[i].pNdisPacket = NULL;
			pMgmtRing->Cell[i].pNextNdisPacket = NULL;

			/*Allocate URB for MLMEContext*/
			pMLMEContext = (PTX_CONTEXT) pAd->MgmtRing.Cell[i].AllocVa;
			pMLMEContext->pUrb = RTUSB_ALLOC_URB(0);
			if (pMLMEContext->pUrb == NULL)
			{
				DBGPRINT(RT_DEBUG_ERROR, ("<-- ERROR in Alloc TX MLMEContext[%d] urb!! \n", i));
				Status = NDIS_STATUS_RESOURCES;
				goto err; 
			}
			pMLMEContext->pAd = pAd;
			pMLMEContext->pIrp = NULL;
			pMLMEContext->TransferBuffer = NULL;
			pMLMEContext->InUse = FALSE;
			pMLMEContext->IRPPending = FALSE;
			pMLMEContext->bWaitingBulkOut = FALSE;
			pMLMEContext->BulkOutSize = 0;
			pMLMEContext->SelfIdx = i;
			
			/* Offset to next ring descriptor address*/
			RingBaseVa = (PUCHAR) RingBaseVa + sizeof(TX_CONTEXT);
		}
		DBGPRINT(RT_DEBUG_TRACE, ("MGMT Ring: total %d entry allocated\n", i));
		
		/*pAd->MgmtRing.TxSwFreeIdx = (MGMT_RING_SIZE - 1);*/
		pAd->MgmtRing.TxSwFreeIdx = MGMT_RING_SIZE;
		pAd->MgmtRing.TxCpuIdx = 0;
		pAd->MgmtRing.TxDmaIdx = 0;

#ifdef MT_MAC
        		/* Allocate BCN ring descriptor's memory*/
		pAd->BcnDescRing.AllocSize = BCN_RING_SIZE * sizeof(TX_CONTEXT);
		os_alloc_mem(pAd, (PUCHAR *)(&pAd->BcnDescRing.AllocVa), pAd->BcnDescRing.AllocSize);
		if (pAd->BcnDescRing.AllocVa == NULL)
		{
			DBGPRINT_ERR(("Failed to allocate a big buffer for BcnDescRing!\n"));
			Status = NDIS_STATUS_RESOURCES;
			goto err;
		}
		NdisZeroMemory(pAd->BcnDescRing.AllocVa, pAd->BcnDescRing.AllocSize);
		RingBaseVa     = pAd->BcnDescRing.AllocVa;

		/* Initialize BCN Ring and associated buffer memory*/
		pBcnRing = &pAd->BcnRing;
		for (i = 0; i < BCN_RING_SIZE; i++)
		{
			/* link the pre-allocated Bcn buffer to MgmtRing.Cell*/
			pBcnRing->Cell[i].AllocSize = sizeof(TX_CONTEXT);
			pBcnRing->Cell[i].AllocVa = RingBaseVa;
			pBcnRing->Cell[i].pNdisPacket = NULL;
			pBcnRing->Cell[i].pNextNdisPacket = NULL;

			/*Allocate URB for MLMEContext*/
			pBcnContext = (PTX_CONTEXT) pAd->BcnRing.Cell[i].AllocVa;
			pBcnContext->pUrb = RTUSB_ALLOC_URB(0);
			if (pBcnContext->pUrb == NULL)
			{
				DBGPRINT(RT_DEBUG_ERROR, ("<-- ERROR in Alloc TX BCNContext[%d] urb!! \n", i));
				Status = NDIS_STATUS_RESOURCES;
				goto err;
			}
			pBcnContext->pAd = pAd;
			pBcnContext->pIrp = NULL;
			pBcnContext->TransferBuffer = NULL;
			pBcnContext->InUse = FALSE;
			pBcnContext->IRPPending = FALSE;
			pBcnContext->bWaitingBulkOut = FALSE;
			pBcnContext->BulkOutSize = 0;
			pBcnContext->SelfIdx = i;

			/* Offset to next ring descriptor address*/
			RingBaseVa = (PUCHAR) RingBaseVa + sizeof(TX_CONTEXT);
		}
		DBGPRINT(RT_DEBUG_TRACE, ("BCN Ring: total %d entry allocated\n", i));

		pAd->BcnRing.TxSwFreeIdx = BCN_RING_SIZE;
		pAd->BcnRing.TxCpuIdx = 0;
		pAd->BcnRing.TxDmaIdx = 0;
#endif /* MT_MAC */

		/* NullContext URB and usb buffer*/
		
		NdisZeroMemory(pNullContext, sizeof(TX_CONTEXT));
		Status = RTMPAllocUsbBulkBufStruct(pAd,
											&pNullContext->pUrb,
											(PVOID *)&pNullContext->TransferBuffer,
											sizeof(TX_BUFFER),
											&pNullContext->data_dma,
											"TxNullContext");
		if (Status != NDIS_STATUS_SUCCESS)
			goto err;

		pNullContext->pAd = pAd;
		pNullContext->pIrp = NULL;
		pNullContext->InUse = FALSE;
		pNullContext->IRPPending = FALSE;

		
		/* PsPollContext URB and usb buffer*/
		
		Status = RTMPAllocUsbBulkBufStruct(pAd,
											&pPsPollContext->pUrb,
											(PVOID *)&pPsPollContext->TransferBuffer,
											sizeof(TX_BUFFER),
											&pPsPollContext->data_dma,
											"TxPsPollContext");
		if (Status != NDIS_STATUS_SUCCESS)
			goto err;

		pPsPollContext->pAd = pAd;
		pPsPollContext->pIrp = NULL;
		pPsPollContext->InUse = FALSE;
		pPsPollContext->IRPPending = FALSE;
		pPsPollContext->bAggregatible = FALSE;
		pPsPollContext->LastOne = TRUE;

	}while (FALSE);


	DBGPRINT(RT_DEBUG_TRACE, ("<-- NICInitTransmit(Status=%d)\n", Status));

	return Status;

	
	/* --------------------------- ERROR HANDLE --------------------------- */
err:
	/* Free PsPoll frame resource*/
	RTMPFreeUsbBulkBufStruct(pAd, 
								&pPsPollContext->pUrb, 
								(PUCHAR *)&pPsPollContext->TransferBuffer, 
								sizeof(TX_BUFFER), 
								pPsPollContext->data_dma);

#ifdef CONFIG_PRE_ALLOC        
   	if (NULL != pPsPollContext->TransferBuffer) {
		POS_COOKIE pObj = (POS_COOKIE) pAd->OS_Cookie;
       	RTUSB_URB_FREE_BUFFER(pObj->pUsb_Dev, sizeof(TX_BUFFER), pPsPollContext->TransferBuffer, pPsPollContext->data_dma);
        pPsPollContext->TransferBuffer = NULL;
    }
#endif
	/* Free NULL frame resource*/
	RTMPFreeUsbBulkBufStruct(pAd, 
								&pNullContext->pUrb, 
								(PUCHAR *)&pNullContext->TransferBuffer, 
								sizeof(TX_BUFFER), 
								pNullContext->data_dma);
#ifdef CONFIG_PRE_ALLOC        
    if (NULL != pNullContext->TransferBuffer) {
		POS_COOKIE pObj = (POS_COOKIE) pAd->OS_Cookie;
       	RTUSB_URB_FREE_BUFFER(pObj->pUsb_Dev, sizeof(TX_BUFFER), pNullContext->TransferBuffer, pNullContext->data_dma);
        pNullContext->TransferBuffer = NULL;
    }
#endif
	
	/* MGMT Ring*/
	if (pAd->MgmtDescRing.AllocVa)
	{
		pMgmtRing = &pAd->MgmtRing;
		for(i=0; i<MGMT_RING_SIZE; i++)
		{
			pMLMEContext = (PTX_CONTEXT) pAd->MgmtRing.Cell[i].AllocVa;
			if (pMLMEContext)
			{
				RTMPFreeUsbBulkBufStruct(pAd, 
											&pMLMEContext->pUrb, 
											(PUCHAR *)&pMLMEContext->TransferBuffer,
											sizeof(TX_BUFFER),
											pMLMEContext->data_dma);
#ifdef CONFIG_PRE_ALLOC        
   				if (NULL != pMLMEContext->TransferBuffer) {
					POS_COOKIE pObj = (POS_COOKIE) pAd->OS_Cookie;
       				RTUSB_URB_FREE_BUFFER(pObj->pUsb_Dev, sizeof(TX_BUFFER), pMLMEContext->TransferBuffer, pMLMEContext->data_dma);
        			pMLMEContext->TransferBuffer = NULL;
    			}
#endif											
			}
		}
		os_free_mem(pAd, pAd->MgmtDescRing.AllocVa);
		pAd->MgmtDescRing.AllocVa = NULL;
	}

#ifdef MT_MAC
    if (pAd->BcnDescRing.AllocVa)
	{
		pBcnRing = &pAd->BcnRing;
		for(i=0; i<BCN_RING_SIZE; i++)
		{
			pBcnContext = (PTX_CONTEXT) pAd->BcnRing.Cell[i].AllocVa;
			if (pBcnContext)
			{
				RTMPFreeUsbBulkBufStruct(pAd,
											&pBcnContext->pUrb,
											(PUCHAR *)&pBcnContext->TransferBuffer,
											sizeof(TX_BUFFER),
											pBcnContext->data_dma);
			}
		}
		os_free_mem(pAd, pAd->BcnDescRing.AllocVa);
		pAd->BcnDescRing.AllocVa = NULL;
	}
#endif

	/* Tx Ring*/
	for (acidx = 0; acidx < NUM_OF_TX_RING; acidx++)
	{
		PHT_TX_CONTEXT pHTTxContext = &(pAd->TxContext[acidx]);
		if (pHTTxContext)
		{
#ifdef USB_BULK_BUF_ALIGMENT
			for(ringidx=0;ringidx < BUF_ALIGMENT_RINGSIZE ;ringidx++)
			{
					RTMPFreeUsbBulkBufStruct(pAd, 
												&pHTTxContext->pUrb[ringidx], 
												(PUCHAR *)&pHTTxContext->TransferBuffer[ringidx],
												sizeof(HTTX_BUFFER),	
												pHTTxContext->data_dma[ringidx]);
			}
#else

			RTMPFreeUsbBulkBufStruct(pAd, 
										&pHTTxContext->pUrb, 
										(PUCHAR *)&pHTTxContext->TransferBuffer,
										sizeof(HTTX_BUFFER),
										pHTTxContext->data_dma);
#endif /* USB_BULK_BUF_ALIGMENT */

		}
	}

#ifdef MULTI_WMM_SUPPORT
	/* Tx Ring*/
	for (acidx = 0; acidx < NUM_OF_WMM1_TX_RING; acidx++)
	{
		PHT_TX_CONTEXT pHTTxContext = &(pAd->TxContextWmm1[acidx]);
		if (pHTTxContext)
		{
#ifdef USB_BULK_BUF_ALIGMENT
			for(ringidx=0;ringidx < BUF_ALIGMENT_RINGSIZE ;ringidx++)
			{
					RTMPFreeUsbBulkBufStruct(pAd, 
												&pHTTxContext->pUrb[ringidx], 
												(PUCHAR *)&pHTTxContext->TransferBuffer[ringidx],
												sizeof(HTTX_BUFFER),	
												pHTTxContext->data_dma[ringidx]);
			}
#else

			RTMPFreeUsbBulkBufStruct(pAd, 
										&pHTTxContext->pUrb, 
										(PUCHAR *)&pHTTxContext->TransferBuffer,
										sizeof(HTTX_BUFFER),
										pHTTxContext->data_dma);
#endif /* USB_BULK_BUF_ALIGMENT */

		}
	}
#endif /* MULTI_WMM_SUPPORT */
	/* Here we didn't have any pre-allocated memory need to free.*/
	
	return Status;	
}


/*
========================================================================
Routine Description:
    Allocate DMA memory blocks for send, receive.

Arguments:
    pAd					Pointer to our adapter

Return Value:
	NDIS_STATUS_SUCCESS
	NDIS_STATUS_FAILURE
	NDIS_STATUS_RESOURCES

Note:
========================================================================
*/
NDIS_STATUS	RTMPAllocTxRxRingMemory(
	IN	PRTMP_ADAPTER	pAd)
{
/*	COUNTER_802_11	pCounter = &pAd->WlanCounters;*/
	NDIS_STATUS		Status = NDIS_STATUS_SUCCESS;
	INT				num;

	
	DBGPRINT(RT_DEBUG_TRACE, ("--> RTMPAllocTxRxRingMemory\n"));


	do
	{
		/* Init the CmdQ and CmdQLock*/
		NdisAllocateSpinLock(pAd, &pAd->CmdQLock);	
		NdisAcquireSpinLock(&pAd->CmdQLock);
		RTInitializeCmdQ(&pAd->CmdQ);
		NdisReleaseSpinLock(&pAd->CmdQLock);
	
		NdisAllocateSpinLock(pAd, &pAd->RxFreeQLock);
		NdisAllocateSpinLock(pAd, &pAd->RxProcessingQLock);
		NdisAllocateSpinLock(pAd, &pAd->RxBulkInQLock);
	
		RTMP_SPIN_LOCK_IRQSAVE(&pAd->RxFreeQLock, &Flags);
		DlListInit(&pAd->RxFreeQ);
		RTMP_SPIN_UNLOCK_IRQRESTORE(&pAd->RxFreeQLock, &Flags);

		RTMP_SPIN_LOCK_IRQSAVE(&pAd->RxProcessingQLock, &Flags);
		DlListInit(&pAd->RxProcessingQ);
		RTMP_SPIN_UNLOCK_IRQRESTORE(&pAd->RxProcessingQLock, &Flags);

		RTMP_SPIN_LOCK_IRQSAVE(&pAd->RxBulkInQLock, &Flags);
		DlListInit(&pAd->RxBulkInQ);
		RTMP_SPIN_UNLOCK_IRQRESTORE(&pAd->RxBulkInQLock, &Flags);

		NdisAllocateSpinLock(pAd, &pAd->MLMEBulkOutLock);
		NdisAllocateSpinLock(pAd, &pAd->BulkInLock);
		NdisAllocateSpinLock(pAd, &pAd->CmdRspLock);
		for(num =0 ; num < 6; num++)
		{
			NdisAllocateSpinLock(pAd, &pAd->BulkOutLock[num]);
		}

		for (num = 0; num < NUM_OF_TX_RING; num++)
		{
			NdisAllocateSpinLock(pAd, &pAd->TxContextQueueLock[num]);
		}
		
#ifdef MULTI_WMM_SUPPORT		
		for(num =0 ; num < NUM_OF_WMM1_TX_RING; num++)
		{
			NdisAllocateSpinLock(pAd, &pAd->BulkOutWmm1Lock[num]);
		}


		for (num = 0; num < NUM_OF_WMM1_TX_RING; num++)
		{
			NdisAllocateSpinLock(pAd, &pAd->TxContextQueueWmm1Lock[num]);
		}
#endif /* MULTI_WMM_SUPPORT */	

#ifdef CONFIG_ATE
		NdisAllocateSpinLock(pAd, &pAd->GenericLock);
#endif /* CONFIG_ATE */


		
		/* Init send data structures and related parameters*/
		
		Status = NICInitTransmit(pAd);
		if (Status != NDIS_STATUS_SUCCESS)
			break;

		
		/* Init receive data structures and related parameters*/
		
		Status = NICInitRecv(pAd);
		if (Status != NDIS_STATUS_SUCCESS)
			break;

		NdisZeroMemory(&pAd->FragFrame, sizeof(FRAGMENT_FRAME));
		pAd->FragFrame.pFragPacket =  RTMP_AllocateFragPacketBuffer(pAd, RX_BUFFER_NORMSIZE);

		if (pAd->FragFrame.pFragPacket == NULL)
		{
			Status = NDIS_STATUS_RESOURCES;
		}
	} while (FALSE);
	
	DBGPRINT_S(("<-- RTMPAllocTxRxRingMemory, Status=%x\n", Status));

	return Status;
}


/*
========================================================================
Routine Description:
	Calls USB_InterfaceStop and frees memory allocated for the URBs
    calls NdisMDeregisterDevice and frees the memory
    allocated in VNetInitialize for the Adapter Object

Arguments:
	*pAd				the raxx interface data pointer

Return Value:
	None

Note:
========================================================================
*/
VOID	RTMPFreeTxRxRingMemory(
	IN	PRTMP_ADAPTER	pAd)
{
	UINT                i, acidx;
	PTX_CONTEXT			pNullContext   = &pAd->NullContext;
	PTX_CONTEXT			pPsPollContext = &pAd->PsPollContext;
	PCMD_RSP_CONTEXT pCmdRspEventContext = &(pAd->CmdRspEventContext);
#ifdef USB_BULK_BUF_ALIGMENT
	INT ringidx;
#endif /* USB_BULK_BUF_ALIGMENT */


	DBGPRINT(RT_DEBUG_ERROR, ("---> RTMPFreeTxRxRingMemory\n"));


	/* Free all resources for the RxRing buffer queue.*/
	for(i=0; i<(RX_RING_SIZE); i++)
	{
		PRX_CONTEXT  pRxContext = &(pAd->RxContext[i]);
		if (pRxContext)
			RTMPFreeUsbBulkBufStruct(pAd,
										&pRxContext->pUrb,
										(PUCHAR *)&pRxContext->TransferBuffer,
										MAX_RXBULK_SIZE,
										pRxContext->data_dma);
	}

	if (pCmdRspEventContext)
	{
		RTMPFreeUsbBulkBufStruct(pAd,
								 &pCmdRspEventContext->pUrb,
								 (PUCHAR *)&pCmdRspEventContext->TransferBuffer,
								 CMD_RSP_BULK_SIZE,
								 pCmdRspEventContext->data_dma);
	}

	/* Free PsPoll frame resource*/
	RTMPFreeUsbBulkBufStruct(pAd,
								&pPsPollContext->pUrb,
								(PUCHAR *)&pPsPollContext->TransferBuffer,
								sizeof(TX_BUFFER),
								pPsPollContext->data_dma);

#ifdef CONFIG_PRE_ALLOC        
   	if (NULL != pPsPollContext->TransferBuffer) {
		POS_COOKIE pObj = (POS_COOKIE) pAd->OS_Cookie;
       	RTUSB_URB_FREE_BUFFER(pObj->pUsb_Dev, sizeof(TX_BUFFER), pPsPollContext->TransferBuffer, pPsPollContext->data_dma);
        pPsPollContext->TransferBuffer = NULL;
    }
#endif
	/* Free NULL frame resource*/
	RTMPFreeUsbBulkBufStruct(pAd,
								&pNullContext->pUrb,
								(PUCHAR *)&pNullContext->TransferBuffer,
								sizeof(TX_BUFFER),
								pNullContext->data_dma);
#ifdef CONFIG_PRE_ALLOC        
    if (NULL != pNullContext->TransferBuffer) {
		POS_COOKIE pObj = (POS_COOKIE) pAd->OS_Cookie;
       	RTUSB_URB_FREE_BUFFER(pObj->pUsb_Dev, sizeof(TX_BUFFER), pNullContext->TransferBuffer, pNullContext->data_dma);
        pNullContext->TransferBuffer = NULL;
    }
#endif

	/* Free mgmt frame resource*/
	for(i = 0; i < MGMT_RING_SIZE; i++)
	{
		PTX_CONTEXT pMLMEContext = (PTX_CONTEXT)pAd->MgmtRing.Cell[i].AllocVa;
		if (pMLMEContext)
		{
			if (NULL != pMLMEContext->pUrb)
			{
				RTUSB_UNLINK_URB(pMLMEContext->pUrb);
				RTUSB_FREE_URB(pMLMEContext->pUrb);
				pMLMEContext->pUrb = NULL;
			}
		}
		
		if (NULL != pAd->MgmtRing.Cell[i].pNdisPacket) 
		{
			RELEASE_NDIS_PACKET(pAd, pAd->MgmtRing.Cell[i].pNdisPacket, NDIS_STATUS_FAILURE);
			pAd->MgmtRing.Cell[i].pNdisPacket = NULL;
			if (pMLMEContext)
			pMLMEContext->TransferBuffer = NULL; 
		}
		
	}
	if (pAd->MgmtDescRing.AllocVa)
		os_free_mem(pAd, pAd->MgmtDescRing.AllocVa);

#ifdef MT_MAC
    if (pAd->BcnDescRing.AllocVa)
		os_free_mem(pAd, pAd->BcnDescRing.AllocVa);
#endif /* MT_MAC */

	/* Free Tx frame resource*/
	for (acidx = 0; acidx < NUM_OF_TX_RING; acidx++)
		{
		PHT_TX_CONTEXT pHTTXContext = &(pAd->TxContext[acidx]);
			if (pHTTXContext)
#ifdef USB_BULK_BUF_ALIGMENT
			{
				for(ringidx=0;ringidx < BUF_ALIGMENT_RINGSIZE ;ringidx++)
				{
					RTMPFreeUsbBulkBufStruct(pAd,
												&pHTTXContext->pUrb[ringidx],
												(PUCHAR *)&pHTTXContext->TransferBuffer[ringidx],
												sizeof(HTTX_BUFFER),	
												pHTTXContext->data_dma[ringidx]);
				}
			}
#else

			RTMPFreeUsbBulkBufStruct(pAd,
										&pHTTXContext->pUrb,
										(PUCHAR *)&pHTTXContext->TransferBuffer,
										sizeof(HTTX_BUFFER),
										pHTTXContext->data_dma);
#endif /* USB_BULK_BUF_ALIGMENT */

		}
	
#ifdef MULTI_WMM_SUPPORT
	/* Free Tx frame resource*/
	for (acidx = 0; acidx < NUM_OF_TX_RING; acidx++)
	{
		PHT_TX_CONTEXT pHTTXContext = &(pAd->TxContextWmm1[acidx]);
		if (pHTTXContext)
			RTMPFreeUsbBulkBufStruct(pAd,
										&pHTTXContext->pUrb,
										(PUCHAR *)&pHTTXContext->TransferBuffer,
										sizeof(HTTX_BUFFER),
										pHTTXContext->data_dma);
	}
#endif /* MULTI_WMM_SUPPORT */
		
	/* Free fragement frame buffer*/
	if (pAd->FragFrame.pFragPacket)
		RELEASE_NDIS_PACKET(pAd, pAd->FragFrame.pFragPacket, NDIS_STATUS_SUCCESS);


	/* Free spinlocks*/
	for(i=0; i<6; i++)
	{
		NdisFreeSpinLock(&pAd->BulkOutLock[i]);
	}

	NdisFreeSpinLock(&pAd->BulkInLock);
	NdisFreeSpinLock(&pAd->CmdRspLock);
	NdisFreeSpinLock(&pAd->MLMEBulkOutLock);

	NdisFreeSpinLock(&pAd->RxFreeQLock);
	NdisFreeSpinLock(&pAd->RxProcessingQLock);
	NdisFreeSpinLock(&pAd->RxBulkInQLock);

	NdisFreeSpinLock(&pAd->CmdQLock);
#ifdef CONFIG_ATE
	NdisFreeSpinLock(&pAd->GenericLock);
#endif /* CONFIG_ATE */

	/* Clear all pending bulk-out request flags.*/
	RTUSB_CLEAR_BULK_FLAG(pAd, 0xffffffff);
	
	for (i = 0; i < NUM_OF_TX_RING; i++)
	{
		NdisFreeSpinLock(&pAd->TxContextQueueLock[i]);
	}
	
#ifdef MULTI_WMM_SUPPORT	
	for (i = 0; i < NUM_OF_WMM1_TX_RING; i++)
	{
		NdisFreeSpinLock(&pAd->TxContextQueueWmm1Lock[i]);
	}
#endif /* MULTI_WMM_SUPPORT */
	DBGPRINT(RT_DEBUG_ERROR, ("<--- RTMPFreeTxRxRingMemory\n"));
}

#endif /* RESOURCE_PRE_ALLOC */


/*
========================================================================
Routine Description:
    Write WLAN MAC address to USB 2870.

Arguments:
    pAd					Pointer to our adapter

Return Value:
	NDIS_STATUS_SUCCESS

Note:
========================================================================
*/
NDIS_STATUS	RTUSBWriteHWMACAddress(RTMP_ADAPTER *pAd)
{
#ifndef MT_MAC
	MAC_DW0_STRUC	StaMacReg0;
	MAC_DW1_STRUC	StaMacReg1;
	NDIS_STATUS		Status = NDIS_STATUS_SUCCESS;
	LARGE_INTEGER	NOW;


	/* initialize the random number generator*/
	RTMP_GetCurrentSystemTime(&NOW);
	
	/* Write New MAC address to MAC_CSR2 & MAC_CSR3 & let ASIC know our new MAC*/
	AsicSetDevMac(pAd, pAd->CurrentAddress, 0);

	return Status;
#endif
}


/*
========================================================================
Routine Description:
    Disable DMA.

Arguments:
	*pAd				the raxx interface data pointer

Return Value:
	None

Note:
========================================================================
*/
VOID RT28XXDMADisable(
	IN RTMP_ADAPTER 		*pAd)
{
	/* no use*/
}


/*
========================================================================
Routine Description:
    Enable DMA.

Arguments:
	*pAd				the raxx interface data pointer

Return Value:
	None

Note:
========================================================================
*/
VOID RT28XXDMAEnable(RTMP_ADAPTER *pAd)
{
	USB_DMA_CFG_STRUC	UsbCfg;


#ifdef RTMP_MAC
	if (pAd->chipCap.hif_type == HIF_RTMP) {
		AsicSetMacTxRx(pAd, ASIC_MAC_TX, TRUE);

		if (AsicWaitPDMAIdle(pAd, 200, 1000) == FALSE) {
			if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST))
				return;
		}

		RtmpusecDelay(50);

		AsicSetWPDMA(pAd, PDMA_TX_RX, TRUE);
		DBGPRINT(RT_DEBUG_TRACE, ("%s():EnableWPDMA\n", __FUNCTION__));

		UsbCfg.word = 0;
		UsbCfg.field.phyclear = 0;
		/* usb version is 1.1,do not use bulk in aggregation */
		if ((pAd->BulkInMaxPacketSize >= 512) && (pAd->usb_ctl.usb_aggregation))
				UsbCfg.field.RxBulkAggEn = 1;
		else
				UsbCfg.field.RxBulkAggEn = 0;
		/* for last packet, PBF might use more than limited, so minus 2 to prevent from error */
		UsbCfg.field.RxBulkAggLmt = (MAX_RXBULK_SIZE /1024)-3;
		UsbCfg.field.RxBulkAggTOut = 0x80; /* 2006-10-18 */
		UsbCfg.field.RxBulkEn = 1;
		UsbCfg.field.TxBulkEn = 1;
	}
#endif /* RTMP_MAC */



#ifdef MT7603
    if (IS_MT7603(pAd)) {
		USB_CFG_READ(pAd, &UsbCfg.word);
	
		/* USB1.1 do not use bulk in aggregation */
		if ((pAd->BulkInMaxPacketSize >= 512) && (pAd->usb_ctl.usb_aggregation))
			UsbCfg.field.RxBulkAggEn = 1;
		else {
			DBGPRINT(RT_DEBUG_OFF, ("disable usb rx aggregagion\n"));
			UsbCfg.field.RxBulkAggEn = 0;
		}

		/* for last packet, PBF might use more than limited, so minus 2 to prevent from error */
		UsbCfg.field.RxBulkAggLmt = (MAX_RXBULK_SIZE /1024) - 3;
		UsbCfg.field.RxBulkAggTOut = 0x80; 

		UsbCfg.field.RxBulkEn = 1;
		UsbCfg.field.TxBulkEn = 1;

		UsbCfg.word &= ~UDMA_WLCFG_0_RX_MPSZ_PAD0_MASK; //bit18
		UsbCfg.word |= UDMA_WLCFG_0_RX_MPSZ_PAD0(1);
	}
#endif

	USB_CFG_WRITE(pAd, UsbCfg.word);
}

/********************************************************************
  *
  *	2870 Beacon Update Related functions.
  *
  ********************************************************************/
#ifdef MT_MAC
VOID RT28xx_UpdateBeaconToAsic(
	IN RTMP_ADAPTER *pAd,
	IN INT apidx,
	IN ULONG FrameLen,
	IN ULONG UpdatePos)
{
	BCN_BUF_STRUC *bcn_buf = NULL;
	UCHAR *buf, *hdr;
	INT len;
	PNDIS_PACKET *pkt = NULL;
	UINT8 TXWISize = pAd->chipCap.TXWISize;
	UCHAR  			*ptr;
	BEACON_SYNC_STRUCT *pBeaconSync = pAd->CommonCfg.pBeaconSync;

#ifdef CONFIG_STA_SUPPORT
        IF_DEV_CONFIG_OPMODE_ON_STA(pAd) {
            if (pAd->StaCfg.BssType == BSS_ADHOC)
			{
                bcn_buf = &pAd->StaCfg.bcn_buf;
				goto check;
			}
        }
#endif /* CONFIG_STA_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
	if ((pAd->OpMode == OPMODE_AP)
#ifdef RT_CFG80211_P2P_SUPPORT	
	    /* TODO: ad-hoc & ap concurrent ? */	
	    || (pAd->cfg80211_ctrl.isCfgInApMode == RT_CMD_80211_IFTYPE_AP)
#endif /* RT_CFG80211_P2P_SUPPORT */		
        )
	{
		bcn_buf = &pAd->ApCfg.MBSSID[apidx].bcn_buf;
	}
#endif /* CONFIG_AP_SUPPORT */

check:
	if (!bcn_buf) {
		DBGPRINT(RT_DEBUG_ERROR, ("%s(): bcn_buf is NULL!\n", __FUNCTION__));
		return;
	}

	pkt = bcn_buf->BeaconPkt;
	if (pkt) {
		buf = (UCHAR *)GET_OS_PKT_DATAPTR(pkt);
		len = FrameLen + pAd->chipCap.tx_hw_hdr_len;
		//hex_dump("bcnContent", buf, len);

		/* For AP interface, set the DtimBitOn so that 
		   we can send Bcast/Mcast frame out after this 
		   beacon frame.
		   */
		   
#ifdef CONFIG_AP_SUPPORT
	if ((pAd->OpMode == OPMODE_AP)
#ifdef RT_CFG80211_P2P_SUPPORT	
	    /* TODO: ad-hoc & ap concurrent ? */	
	    || (pAd->cfg80211_ctrl.isCfgInApMode == RT_CMD_80211_IFTYPE_AP)
#endif /* RT_CFG80211_P2P_SUPPORT */
	   )	
{
		{
			ptr = buf;
			ptr = &buf[((TXWISize) + pAd->ApCfg.MBSSID[apidx].TimIELocationInBeacon)];
			//hex_dump("TIM", ptr, 5);
			if ((*(ptr + 4)) & 0x01)
				pBeaconSync->DtimBitOn |= (1 << apidx);
			else
				pBeaconSync->DtimBitOn &= ~(1 << apidx);
		}
}		
#endif /* CONFIG_AP_SUPPORT */
		
		
		SET_OS_PKT_LEN(pkt, len);
#ifdef RT_BIG_ENDIAN
		MTMacInfoEndianChange(pAd, buf, TYPE_TMACINFO, sizeof(TMAC_TXD_L));
#endif /* RT_BIG_ENDIAN */
		/* Now do hardware-depened kick out.*/
		HAL_KickOutMgmtTx(pAd, Q_IDX_BCN, pkt, buf, len);
		bcn_buf->bcn_state = BCN_TX_WRITE_TO_DMA;
	} else {
		DBGPRINT(RT_DEBUG_ERROR, ("%s(): BeaconPkt is NULL!\n", __FUNCTION__));
	}
}
#endif /* MT_MAC */


#if defined(RTMP_MAC) || defined(RLT_MAC)
/*
========================================================================
Routine Description:
    Write Beacon buffer to Asic.

Arguments:
	*pAd				the raxx interface data pointer

Return Value:
	None

Note:
========================================================================
*/
VOID RT28xx_UpdateBeaconToAsic(
	IN RTMP_ADAPTER		*pAd,
	IN INT				apidx,
	IN ULONG			FrameLen,
	IN ULONG			UpdatePos)
{
	UCHAR *pBeaconFrame = NULL, *tmac_info = NULL;
	UCHAR  			*ptr;
	UINT  			i, padding;
	BEACON_SYNC_STRUCT *pBeaconSync = pAd->CommonCfg.pBeaconSync;
	UINT32			longValue;
	BOOLEAN			bBcnReq = FALSE;
	UCHAR			bcn_idx = 0;
	UINT8 TXWISize = pAd->chipCap.TXWISize;

#ifdef CONFIG_AP_SUPPORT
	if ((apidx < pAd->ApCfg.BssidNum) && (apidx < MAX_MBSSID_NUM(pAd)))
	{
		if (!pAd->ApCfg.MBSSID[apidx].bcn_buf.BeaconPkt)
		{
			DBGPRINT(RT_DEBUG_ERROR,("%s():BeaconPkt is NULL\n",
						__FUNCTION__));
			return;
		}
		bcn_idx = pAd->ApCfg.MBSSID[apidx].bcn_buf.BcnBufIdx;
		tmac_info = (UCHAR *)GET_OS_PKT_DATAPTR(pAd->ApCfg.MBSSID[apidx].bcn_buf.BeaconPkt);
		pBeaconFrame = (UCHAR *)(tmac_info + TXWISize);
		bBcnReq = BeaconTransmitRequired(pAd, apidx, &pAd->ApCfg.MBSSID[apidx]);
	}
#endif /* CONFIG_AP_SUPPORT */

	if ((pBeaconFrame == NULL) || (pBeaconSync == NULL))
	{
		DBGPRINT(RT_DEBUG_ERROR,("%s():pBeaconFrame/Sync(%p/%p) is NULL\n",
					__FUNCTION__, pBeaconFrame, pBeaconSync));
		return;
	}

	if (pAd->BeaconOffset[bcn_idx] == 0) {
		DBGPRINT(RT_DEBUG_ERROR, ("%s():Invalid BcnOffset[%d]\n",
					__FUNCTION__, bcn_idx));
		return;
	}
	
	if (bBcnReq == FALSE)
	{
		/* when the ra interface is down, do not send its beacon frame */
		/* clear all zero */
		for(i=0; i < TXWISize; i+=4) {
			RTMP_CHIP_UPDATE_BEACON(pAd, pAd->BeaconOffset[bcn_idx] + i, 0x00, 4);
		}

		pBeaconSync->BeaconBitMap &= (~(BEACON_BITMAP_MASK & (1 << bcn_idx)));
		NdisZeroMemory(pBeaconSync->BeaconTxWI[bcn_idx], TXWISize);
	}
	else
	{
		ptr = tmac_info;
#ifdef RT_BIG_ENDIAN
		RTMPWIEndianChange(pAd, ptr, TYPE_TXWI);
#endif
		if (NdisEqualMemory(pBeaconSync->BeaconTxWI[bcn_idx], tmac_info, TXWISize) == FALSE)
		{	/* If BeaconTxWI changed, we need to rewrite the TxWI for the Beacon frames.*/
			pBeaconSync->BeaconBitMap &= (~(BEACON_BITMAP_MASK & (1 << bcn_idx)));
			NdisMoveMemory(pBeaconSync->BeaconTxWI[bcn_idx], &pAd->BeaconTxWI, TXWISize);
		}
		
		if ((pBeaconSync->BeaconBitMap & (1 << bcn_idx)) != (1 << bcn_idx))
		{
			for (i=0; i < TXWISize; i+=4)
			{
				longValue =  *ptr + (*(ptr+1)<<8) + (*(ptr+2)<<16) + (*(ptr+3)<<24);
				RTMP_CHIP_UPDATE_BEACON(pAd, pAd->BeaconOffset[bcn_idx] + i, longValue, 4);
				ptr += 4;
			}
		}

		ptr = pBeaconSync->BeaconBuf[bcn_idx];
		padding = (FrameLen & 0x01);
		NdisZeroMemory((PUCHAR)(pBeaconFrame + FrameLen), padding);
		FrameLen += padding;
		for (i = 0 ; i < FrameLen /*HW_BEACON_OFFSET*/; i += 2)
		{
			if (NdisEqualMemory(ptr, pBeaconFrame, 2) == FALSE)
			{
				NdisMoveMemory(ptr, pBeaconFrame, 2);
				longValue =  *ptr + (*(ptr+1)<<8);
				RTMP_CHIP_UPDATE_BEACON(pAd, pAd->BeaconOffset[bcn_idx] + TXWISize + i, longValue, 2);
			}
			ptr +=2;
			pBeaconFrame += 2;
		}


		pBeaconSync->BeaconBitMap |= (1 << bcn_idx);
	
		/* For AP interface, set the DtimBitOn so that we can send Bcast/Mcast frame out after this beacon frame.*/
#ifdef CONFIG_AP_SUPPORT
		{
			ptr = (PUCHAR) ((tmac_info + TXWISize) + pAd->ApCfg.MBSSID[apidx].TimIELocationInBeacon);
			if ((*(ptr + 4)) & 0x01)
				pBeaconSync->DtimBitOn |= (1 << apidx);
			else
				pBeaconSync->DtimBitOn &= ~(1 << apidx);
		}
#endif /* CONFIG_AP_SUPPORT */
}
}
#endif /* defined(RTMP_MAC) || defined(RLT_MAC) */

VOID RTUSBBssBeaconStop(
	IN RTMP_ADAPTER *pAd)
{
	BEACON_SYNC_STRUCT	*pBeaconSync;
	int i, offset;
	BOOLEAN	Cancelled = TRUE;
	UINT8 TXWISize = pAd->chipCap.TXWISize;

	pBeaconSync = pAd->CommonCfg.pBeaconSync;
	if (pBeaconSync && pBeaconSync->EnableBeacon)
	{
		INT NumOfBcn = 0;

#ifdef CONFIG_AP_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
		{
			NumOfBcn = pAd->ApCfg.BssidNum + MAX_MESH_NUM;
		}
#endif /* CONFIG_AP_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
		{
			NumOfBcn = MAX_MESH_NUM;
		}
#endif /* CONFIG_STA_SUPPORT */

#ifndef BCN_OFFLOAD_SUPPORT
		RTMPCancelTimer(&pAd->CommonCfg.BeaconUpdateTimer, &Cancelled);
#endif
		for(i=0; i<NumOfBcn; i++)
		{
			NdisZeroMemory(pBeaconSync->BeaconBuf[i], HW_BEACON_OFFSET);
			NdisZeroMemory(pBeaconSync->BeaconTxWI[i], TXWISize);

			for (offset=0; offset<HW_BEACON_OFFSET; offset+=4)
				RTMP_CHIP_UPDATE_BEACON(pAd, pAd->BeaconOffset[i] + offset, 0x00, 4);
			
			pBeaconSync->CapabilityInfoLocationInBeacon[i] = 0;
			pBeaconSync->TimIELocationInBeacon[i] = 0;
		}
		pBeaconSync->BeaconBitMap = 0;
		pBeaconSync->DtimBitOn = 0;
	}
}


VOID RTUSBBssBeaconStart(
	IN RTMP_ADAPTER *pAd)
{
	int apidx;
	BEACON_SYNC_STRUCT	*pBeaconSync;
	UINT8 TXWISize = pAd->chipCap.TXWISize;
/*	LARGE_INTEGER 	tsfTime, deltaTime;*/

	pBeaconSync = pAd->CommonCfg.pBeaconSync;
	if (pBeaconSync && pBeaconSync->EnableBeacon)
	{
		INT NumOfBcn = 0;

#ifdef CONFIG_AP_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
		{
			NumOfBcn = pAd->ApCfg.BssidNum + MAX_MESH_NUM;
		}
#endif /* CONFIG_AP_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
		IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
		{
			NumOfBcn = MAX_MESH_NUM;
		}
#endif /* CONFIG_STA_SUPPORT */

		for(apidx=0; apidx<NumOfBcn; apidx++)
		{
			UCHAR CapabilityInfoLocationInBeacon = 0;
			UCHAR TimIELocationInBeacon = 0;
#ifdef CONFIG_AP_SUPPORT
			IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
			{
				{
					CapabilityInfoLocationInBeacon = pAd->ApCfg.MBSSID[apidx].bcn_buf.cap_ie_pos;
					TimIELocationInBeacon = pAd->ApCfg.MBSSID[apidx].TimIELocationInBeacon;
				}
			}
#endif /* CONFIG_AP_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
#endif /* CONFIG_STA_SUPPORT */

			NdisZeroMemory(pBeaconSync->BeaconBuf[apidx], HW_BEACON_OFFSET);
			pBeaconSync->CapabilityInfoLocationInBeacon[apidx] = CapabilityInfoLocationInBeacon;
			pBeaconSync->TimIELocationInBeacon[apidx] = TimIELocationInBeacon;
			NdisZeroMemory(pBeaconSync->BeaconTxWI[apidx], TXWISize);
		}
		pBeaconSync->BeaconBitMap = 0;
		pBeaconSync->DtimBitOn = 0;
		pAd->CommonCfg.BeaconUpdateTimer.Repeat = TRUE;

		pAd->CommonCfg.BeaconAdjust = 0;
		pAd->CommonCfg.BeaconFactor = 0xffffffff / (pAd->CommonCfg.BeaconPeriod << 10);
		pAd->CommonCfg.BeaconRemain = (0xffffffff % (pAd->CommonCfg.BeaconPeriod << 10)) + 1;
		DBGPRINT(RT_DEBUG_TRACE, ("RTUSBBssBeaconStart:BeaconFactor=%d, BeaconRemain=%d!\n", 
									pAd->CommonCfg.BeaconFactor, pAd->CommonCfg.BeaconRemain));
#ifndef BCN_OFFLOAD_SUPPORT
		RTMPSetTimer(&pAd->CommonCfg.BeaconUpdateTimer, 10 /*pAd->CommonCfg.BeaconPeriod*/);
#endif

	}
}


VOID RTUSBBssBeaconInit(
	IN RTMP_ADAPTER *pAd)
{
	BEACON_SYNC_STRUCT	*pBeaconSync;
	int i, j;
	UINT8 TXWISize = pAd->chipCap.TXWISize;

	os_alloc_mem(pAd, (PUCHAR *)(&pAd->CommonCfg.pBeaconSync), sizeof(BEACON_SYNC_STRUCT));

	if (pAd->CommonCfg.pBeaconSync)
	{
		pBeaconSync = pAd->CommonCfg.pBeaconSync;
		NdisZeroMemory(pBeaconSync, sizeof(BEACON_SYNC_STRUCT));
		for(i=0; i < HW_BEACON_MAX_COUNT(pAd); i++)
		{
			NdisZeroMemory(pBeaconSync->BeaconBuf[i], HW_BEACON_OFFSET);
			pBeaconSync->CapabilityInfoLocationInBeacon[i] = 0;
			pBeaconSync->TimIELocationInBeacon[i] = 0;
			os_alloc_mem(pAd, &pBeaconSync->BeaconTxWI[i], TXWISize);
			if (pBeaconSync->BeaconTxWI[i])
				NdisZeroMemory(pBeaconSync->BeaconTxWI[i], TXWISize);
			else
				goto error2;
		}
		pBeaconSync->BeaconBitMap = 0;
		
		/*RTMPInitTimer(pAd, &pAd->CommonCfg.BeaconUpdateTimer, GET_TIMER_FUNCTION(BeaconUpdateExec), pAd, TRUE);*/
		pBeaconSync->EnableBeacon = TRUE;
	}else
		goto error1;

	return;

error2:
	for (j = 0; j < i; j++)
		os_free_mem(pAd, pBeaconSync->BeaconTxWI[j]);
	
	os_free_mem(pAd, pAd->CommonCfg.pBeaconSync);

error1:
	DBGPRINT(RT_DEBUG_ERROR, ("memory are not available\n"));
}


VOID RTUSBBssBeaconExit(
	IN RTMP_ADAPTER *pAd)
{
	BEACON_SYNC_STRUCT	*pBeaconSync;
	BOOLEAN	Cancelled = TRUE;
	int i;

	if (pAd->CommonCfg.pBeaconSync)
	{
		pBeaconSync = pAd->CommonCfg.pBeaconSync;
		pBeaconSync->EnableBeacon = FALSE;
#ifndef BCN_OFFLOAD_SUPPORT
		RTMPCancelTimer(&pAd->CommonCfg.BeaconUpdateTimer, &Cancelled);
#endif
		pBeaconSync->BeaconBitMap = 0;

		for(i=0; i<HW_BEACON_MAX_COUNT(pAd); i++)
		{
			NdisZeroMemory(pBeaconSync->BeaconBuf[i], HW_BEACON_OFFSET);
			pBeaconSync->CapabilityInfoLocationInBeacon[i] = 0;
			pBeaconSync->TimIELocationInBeacon[i] = 0;
			os_free_mem(pAd, pBeaconSync->BeaconTxWI[i]);
		}

		os_free_mem(pAd, pAd->CommonCfg.pBeaconSync);
		pAd->CommonCfg.pBeaconSync = NULL;
	}
}


/*
    ========================================================================
    Routine Description:
        For device work as AP mode but didn't have TBTT interrupt event, we need a mechanism 
        to update the beacon context in each Beacon interval. Here we use a periodical timer 
        to simulate the TBTT interrupt to handle the beacon context update.
        
    Arguments:
        SystemSpecific1         - Not used.
        FunctionContext         - Pointer to our Adapter context.
        SystemSpecific2         - Not used.
        SystemSpecific3         - Not used.
        
    Return Value:
        None
        
    ========================================================================
*/
VOID BeaconUpdateExec(
    IN PVOID SystemSpecific1, 
    IN PVOID FunctionContext, 
    IN PVOID SystemSpecific2, 
    IN PVOID SystemSpecific3)
{
	PRTMP_ADAPTER	pAd = (PRTMP_ADAPTER)FunctionContext;
	LARGE_INTEGER	tsfTime_a;/*, tsfTime_b, deltaTime_exp, deltaTime_ab;*/
	UINT32			delta, delta2MS, period2US, remain, remain_low, remain_high;
/*	BOOLEAN			positive;*/
	if (pAd->CommonCfg.IsUpdateBeacon==TRUE)
	{
#if defined(RTMP_MAC) || defined(RLT_MAC)
		if (pAd->chipCap.hif_type != HIF_MT)
			ReSyncBeaconTime(pAd);
#endif /* defined(RTMP_MAC) || defined(RLT_MAC) */
		
#ifdef CONFIG_AP_SUPPORT
#ifdef RT_CFG80211_P2P_SUPPORT
		if (pAd->cfg80211_ctrl.isCfgInApMode == RT_CMD_80211_IFTYPE_AP)	
#else
		IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
#endif /* RT_CFG80211_P2P_SUPPORT */
		{
			BEACON_SYNC_STRUCT *pBeaconSync = pAd->CommonCfg.pBeaconSync;
			ULONG UpTime;

			/* update channel utilization */
			NdisGetSystemUpTime(&UpTime);

#ifdef AP_QLOAD_SUPPORT
			QBSS_LoadUpdate(pAd, UpTime);
#endif /* AP_QLOAD_SUPPORT */

		
			if (pAd->ApCfg.DtimCount == 0 && pBeaconSync->DtimBitOn)
			{
				POS_COOKIE pObj;
			
				pObj = (POS_COOKIE) pAd->OS_Cookie;
				RTMP_OS_TASKLET_SCHE(&pObj->tbtt_task);
			}

#ifdef RT_CFG80211_SUPPORT
			RT_CFG80211_BEACON_TIM_UPDATE(pAd);
#else
			APUpdateAllBeaconFrame(pAd);
#endif /* RT_CFG80211_SUPPORT  */
		}
#endif /* CONFIG_AP_SUPPORT */

	}

	AsicGetTsfTime(pAd, &tsfTime_a.u.HighPart, &tsfTime_a.u.LowPart);

	/*
		Calculate next beacon time to wake up to update.

		BeaconRemain = (0xffffffff % (pAd->CommonCfg.BeaconPeriod << 10)) + 1;

		Background: Timestamp (us) % Beacon Period (us) shall be 0 at TBTT

		Formula:	(a+b) mod m = ((a mod m) + (b mod m)) mod m 
					(a*b) mod m = ((a mod m) * (b mod m)) mod m 

		==> ((HighPart * 0xFFFFFFFF) + LowPart) mod Beacon_Period
		==> (((HighPart * 0xFFFFFFFF) mod Beacon_Period) +
			(LowPart mod (Beacon_Period))) mod Beacon_Period
		==> ((HighPart mod Beacon_Period) * (0xFFFFFFFF mod Beacon_Period)) mod
			Beacon_Period

		Steps:
		1. Calculate the delta time between now and next TBTT;

			delta time = (Beacon Period) - ((64-bit timestamp) % (Beacon Period))

			(1) If no overflow for LowPart, 32-bit, we can calcualte the delta
				time by using LowPart;

				delta time = LowPart % (Beacon Period)

			(2) If overflow for LowPart, we need to care about HighPart value;

				delta time = (BeaconRemain * HighPart + LowPart) % (Beacon Period)

				Ex: if the maximum value is 0x00 0xFF (255), Beacon Period = 100,
					TBTT timestamp will be 100, 200, 300, 400, ...
					when TBTT timestamp is 300 = 1*56 + 44, means HighPart = 1,
					Low Part = 44

		2. Adjust next update time of the timer to (delta time + 10ms).
	*/

	/*positive=getDeltaTime(tsfTime_a, expectedTime, &deltaTime_exp);*/
	period2US = (pAd->CommonCfg.BeaconPeriod << 10);
	remain_high = pAd->CommonCfg.BeaconRemain * tsfTime_a.u.HighPart;
	remain_low = tsfTime_a.u.LowPart % (pAd->CommonCfg.BeaconPeriod << 10);
	remain = (remain_high + remain_low)%(pAd->CommonCfg.BeaconPeriod << 10);
	delta = (pAd->CommonCfg.BeaconPeriod << 10) - remain;

	delta2MS = (delta>>10);
	if (delta2MS > 150)
	{
		pAd->CommonCfg.BeaconUpdateTimer.TimerValue = 100;
		pAd->CommonCfg.IsUpdateBeacon=FALSE;
	}
	else
	{
		pAd->CommonCfg.BeaconUpdateTimer.TimerValue = delta2MS + 10;
		pAd->CommonCfg.IsUpdateBeacon=TRUE;
	}
#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
	{
		if ((pAd->CommonCfg.Channel > 14)
			&& (pAd->CommonCfg.bIEEE80211H == 1)
			&& (pAd->Dot11_H.RDMode == RD_SWITCHING_MODE))
		{
			ChannelSwitchingCountDownProc(pAd);
		}
	}
#endif /* CONFIG_AP_SUPPORT */
}


/********************************************************************
  *
  *	2870 Radio on/off Related functions.
  *
  ********************************************************************/
VOID UsbMlmeRadioOn(
	IN PRTMP_ADAPTER pAd)
{
	
    DBGPRINT(RT_DEBUG_TRACE,("UsbMlmeRadioOn()\n"));

	if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF))
		return;

#ifdef MT_MAC
	if (pAd->chipCap.hif_type == HIF_MT)
	{
		MTUsbMlmeRadioOn(pAd);
		return;
	}
#endif /* MT_MAC */

	ASIC_RADIO_ON(pAd, MLME_RADIO_ON);
	

	/* Clear Radio off flag*/
	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF);

#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
		APStartUp(pAd);
#endif /* CONFIG_AP_SUPPORT */

#ifdef LED_CONTROL_SUPPORT
	/* Set LED*/
#ifdef CONFIG_STA_SUPPORT
	RTMPSetLED(pAd, LED_RADIO_ON);
#endif /* CONFIG_STA_SUPPORT */
#ifdef CONFIG_AP_SUPPORT
	RTMPSetLED(pAd, LED_LINK_UP);
#endif /* CONFIG_AP_SUPPORT */
#endif /* LED_CONTROL_SUPPORT */

}


VOID UsbMlmeRadioOFF(
	IN PRTMP_ADAPTER pAd)
{
	
#ifdef MT_MAC
	if (pAd->chipCap.hif_type == HIF_MT)
	{
		MTUsbMlmeRadioOff(pAd);
		return;
	}
#endif /* MT_MAC */

	DBGPRINT(RT_DEBUG_TRACE,("UsbMlmeRadioOFF()\n"));

	if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF))
		return;

	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_MCU_SEND_IN_BAND_CMD);


#ifdef CONFIG_STA_SUPPORT	
	/* Clear PMKID cache.*/
	pAd->StaCfg.SavedPMKNum = 0;
	RTMPZeroMemory(pAd->StaCfg.SavedPMK, (PMKID_NO * sizeof(BSSID_INFO)));

	/* Link down first if any association exists*/
	if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST))
	{
		if (INFRA_ON(pAd) || ADHOC_ON(pAd))
		{
			MLME_DISASSOC_REQ_STRUCT DisReq;
			MLME_QUEUE_ELEM *pMsgElem;

			os_alloc_mem(pAd, (UCHAR **)&pMsgElem, sizeof(MLME_QUEUE_ELEM));
			if (pMsgElem)
			{
				COPY_MAC_ADDR(&DisReq.Addr, pAd->CommonCfg.Bssid);
				DisReq.Reason =  REASON_DISASSOC_STA_LEAVING;

				pMsgElem->Machine = ASSOC_STATE_MACHINE;
				pMsgElem->MsgType = MT2_MLME_DISASSOC_REQ;
				pMsgElem->MsgLen = sizeof(MLME_DISASSOC_REQ_STRUCT);
				NdisMoveMemory(pMsgElem->Msg, &DisReq, sizeof(MLME_DISASSOC_REQ_STRUCT));
			
				MlmeDisassocReqAction(pAd, pMsgElem);
				os_free_mem(NULL, pMsgElem);
				
				RtmpusecDelay(1000);
			}
		}
	}
#endif /* CONFIG_STA_SUPPORT */
		
	/* Set Radio off flag*/
	RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF);

#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
	{
		/* Link down first if any association exists*/
		if (INFRA_ON(pAd) || ADHOC_ON(pAd))
			LinkDown(pAd, FALSE);
		RtmpusecDelay(10000);

		/*==========================================*/
		/* Clean up old bss table*/
#ifndef ANDROID_SUPPORT
/* because abdroid will get scan table when interface down, so we not clean scan table */
		BssTableInit(&pAd->ScanTab);
#endif /* ANDROID_SUPPORT */

	}
#endif /* CONFIG_STA_SUPPORT */

#ifdef CONFIG_AP_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_AP(pAd)
		APStop(pAd);
#endif /* CONFIG_AP_SUPPORT */

#ifdef LED_CONTROL_SUPPORT
	RTMPSetLED(pAd, LED_RADIO_OFF);
#endif /* LED_CONTROL_SUPPORT */

	ASIC_RADIO_OFF(pAd, MLME_RADIO_OFF);
}


VOID RT28xxUsbAsicRadioOff(RTMP_ADAPTER *pAd)
{
	DBGPRINT(RT_DEBUG_TRACE, ("--> %s\n", __FUNCTION__));

	return;
	RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_IDLE_RADIO_OFF);

	if (pAd->CommonCfg.CentralChannel)
		AsicTurnOffRFClk(pAd, pAd->CommonCfg.CentralChannel);
	else
		AsicTurnOffRFClk(pAd, pAd->CommonCfg.Channel);



#ifdef CONFIG_STA_SUPPORT
	AsicSendCommandToMcu(pAd, 0x30, 0xff, 0xff, 0x02, FALSE);   /* send POWER-SAVE command to MCU. Timeout 40us.*/
	/* Stop bulkin pipe*/
	if((!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)))
	{
		RTUSBCancelPendingBulkInIRP(pAd);
	}
#endif /* CONFIG_STA_SUPPORT */
	DBGPRINT(RT_DEBUG_TRACE, ("<== %s\n", __FUNCTION__));

}


VOID RT28xxUsbAsicRadioOn(RTMP_ADAPTER *pAd)
{
	UINT32 MACValue = 0;
	BOOLEAN brc;
	UINT RetryRound = 0;
#ifdef RTMP_RF_RW_SUPPORT
	UCHAR rfreg;
#endif /* RTMP_RF_RW_SUPPORT */
	RTMP_CHIP_OP *pChipOps = &pAd->chipOps;

	
	/* make some traffic to invoke EvtDeviceD0Entry callback function*/
	
	RTMP_IO_READ32(pAd,0x1000, &MACValue);
	DBGPRINT(RT_DEBUG_TRACE,("A MAC query to invoke EvtDeviceD0Entry, MACValue = 0x%x\n",MACValue));

	/* 1. Send wake up command.*/
	{
		RetryRound = 0;
		do
		{
			brc = AsicSendCommandToMcu(pAd, 0x31, PowerWakeCID, 0x00, 0x02, FALSE);   
			if (brc)
			{
				/* Wait command ok.*/
				brc = AsicCheckCommandOk(pAd, PowerWakeCID);
			}
			if(brc){
				break;      /* PowerWakeCID cmd successed*/
			}
			DBGPRINT(RT_DEBUG_WARN, ("PSM :WakeUp Cmd Failed, retry %d\n", RetryRound));

			/* try 10 times at most*/
			if ((RetryRound++) > 10)
				break;
			/* delay and try again*/
			RtmpusecDelay(200);
		} while (TRUE);
		if (RetryRound > 10)
			DBGPRINT(RT_DEBUG_WARN, ("PSM :ASIC 0x31 WakeUp Cmd may Fail %d*******\n", RetryRound));
	}


	/* 2. Enable Tx/Rx DMA.*/
	AsicSetMacTxRx(pAd, ASIC_MAC_TX, TRUE);
	AsicWaitPDMAIdle(pAd, 200, 1000);

	RtmpusecDelay(50);
	AsicSetWPDMA(pAd, PDMA_TX_RX, TRUE);
	DBGPRINT(RT_DEBUG_TRACE, (" %s():Enable WPDMA\n", __FUNCTION__));

	/* enable RX of MAC block*/
	AsicSetRxFilter(pAd);

	if (pAd->CommonCfg.bTXRX_RXV_ON)
		AsicSetMacTxRx(pAd, ASIC_MAC_TXRX_RXV, TRUE);
	else
		AsicSetMacTxRx(pAd, ASIC_MAC_TXRX, TRUE);

	/* 3. Turn on RF*/
/*	RT28xxUsbAsicRFOn(pAd);*/
	if (pChipOps->AsicReverseRfFromSleepMode)
		pChipOps->AsicReverseRfFromSleepMode(pAd, FALSE);

#ifdef RTMP_RF_RW_SUPPORT
/*for 3xxx ? need to reset R07 for VO......*/
#ifdef RT_RF
           RT30xxReadRFRegister(pAd, RF_R07, &rfreg);
           rfreg = rfreg | 0x1;
           RT30xxWriteRFRegister(pAd, RF_R07, rfreg);
#endif /* RT_RF */
#endif /* RTMP_RF_RW_SUPPORT */

	/* 4. Clear idle flag*/
	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_IDLE_RADIO_OFF);
	
	/* Send Bulkin IRPs after flag fRTMP_ADAPTER_IDLE_RADIO_OFF is cleared.*/
	/*	*/
#ifdef CONFIG_STA_SUPPORT
	IF_DEV_CONFIG_OPMODE_ON_STA(pAd)
		RTUSBBulkReceive(pAd);
#endif /* CONFIG_STA_SUPPORT */
	DBGPRINT(RT_DEBUG_TRACE, ("<== %s\n", __FUNCTION__));


}


BOOLEAN AsicCheckCommandOk(
	IN PRTMP_ADAPTER pAd,
	IN UCHAR		 Command)
{
#ifndef MT_MAC
	UINT32	CmdStatus, CID, i;
	UINT32	ThisCIDMask = 0;
	INT ret;


#ifdef RTMP_MAC_USB
	if (IS_USB_INF(pAd))
	{
		RTMP_SEM_EVENT_WAIT(&pAd->reg_atomic, ret);
		if (ret != 0) {
			DBGPRINT(RT_DEBUG_ERROR, ("reg_atomic get failed(ret=%d)\n", ret));
			return FALSE;
		}
	}
#endif /* RTMP_MAC_USB */
	
	i = 0;
	do
	{
		RTMP_IO_READ32(pAd, H2M_MAILBOX_CID, &CID);

		if ((CID & CID0MASK) == Command)
		{
			ThisCIDMask = CID0MASK;
			break;
		}
		else if ((((CID & CID1MASK)>>8) & 0xff) == Command)
		{
			ThisCIDMask = CID1MASK;
			break;
		}
		else if ((((CID & CID2MASK)>>16) & 0xff) == Command)
		{
			ThisCIDMask = CID2MASK;
			break;
		}
		else if ((((CID & CID3MASK)>>24) & 0xff) == Command)
		{
			ThisCIDMask = CID3MASK;
			break;
		}

		RtmpusecDelay(100);
		i++;
	}while (i < 200);

	ret = FALSE;

	RTMP_IO_READ32(pAd, H2M_MAILBOX_STATUS, &CmdStatus);
	
	if (i < 200)
	{
		if (((CmdStatus & ThisCIDMask) == 0x1) || ((CmdStatus & ThisCIDMask) == 0x100) 
			|| ((CmdStatus & ThisCIDMask) == 0x10000) || ((CmdStatus & ThisCIDMask) == 0x1000000))
			ret = TRUE;
	}
	RTMP_IO_WRITE32(pAd, H2M_MAILBOX_STATUS, 0xffffffff);
	RTMP_IO_WRITE32(pAd, H2M_MAILBOX_CID, 0xffffffff);

#ifdef RTMP_MAC_USB
	if (IS_USB_INF(pAd))
	{
		RTMP_SEM_EVENT_UP(&pAd->reg_atomic);
	}
#endif /* RTMP_MAC_USB */


	return ret;
#endif
}


#ifdef WOW_SUPPORT
#ifdef RTMP_MAC
VOID RT28xxUsbAsicWOWEnable(
	IN PRTMP_ADAPTER pAd)
{
	UINT32 Value;
	
	/* load WOW-enable firmware */
	AsicLoadWOWFirmware(pAd, TRUE);
	/* put null frame data to MCU memory from 0x7780 */
	AsicWOWSendNullFrame(pAd, pAd->CommonCfg.TxRate, (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_WMM_INUSED) ? TRUE:FALSE));
	/* send WOW enable command to MCU. */
	AsicSendCommandToMcu(pAd, 0x33, 0xff, pAd->WOW_Cfg.nSelectedGPIO, pAd->WOW_Cfg.nDelay, FALSE);   
	/* set GPIO pulse hold time at MSB (Byte) */
	RTMP_IO_READ32(pAd, GPIO_HOLDTIME_OFFSET, &Value);
	Value &= 0x00FFFFFF;
	Value |= (pAd->WOW_Cfg.nHoldTime << 24);
	RTMP_IO_WRITE32(pAd, GPIO_HOLDTIME_OFFSET, Value);	
	DBGPRINT(RT_DEBUG_OFF, ("Send WOW enable cmd (%d/%d/%d)\n", pAd->WOW_Cfg.nDelay, pAd->WOW_Cfg.nSelectedGPIO, pAd->WOW_Cfg.nHoldTime));
	RTMP_IO_READ32(pAd, GPIO_HOLDTIME_OFFSET, &Value);
	DBGPRINT(RT_DEBUG_OFF, ("Hold time: 0x7020 ==> %x\n", Value));
}

VOID RT28xxUsbAsicWOWDisable(
	IN PRTMP_ADAPTER pAd)
{
	UINT32 Value;
	/* load normal firmware */
	AsicLoadWOWFirmware(pAd, FALSE);
	/* for suspend/resume, needs to restore RX Queue operation mode to auto mode */
	RTMP_IO_READ32(pAd, PBF_CFG, &Value);
	Value &= ~0x2200;
	RTMP_IO_WRITE32(pAd, PBF_CFG, Value);
    //AsicSendCommandToMcu(pAd, 0x34, 0xff, 0x00, 0x00, FALSE);   /* send WOW disable command to MCU*/
    DBGPRINT(RT_DEBUG_OFF, ("MCU back to normal mode (%d/%d)\n", pAd->WOW_Cfg.nDelay, pAd->WOW_Cfg.nSelectedGPIO));
}
#endif /* RTMP_MAC */
#endif /* WOW_SUPPORT */
#endif /* RTMP_MAC_USB */
