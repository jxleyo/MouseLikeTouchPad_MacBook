#define REPORT_BUFFER_SIZE   1024

#include "MouseLikeTouchPad_SPI.h"

///// copy from WDK sample
NTSTATUS
RequestCopyFromBuffer(
      WDFREQUEST        Request,
      PVOID             SourceBuffer,
      size_t            NumBytesToCopyFrom
    )
/*++

Routine Description:

    A helper function to copy specified bytes to the request's output memory

Arguments:

    Request - A handle to a framework request object.

    SourceBuffer - The buffer to copy data from.

    NumBytesToCopyFrom - The length, in bytes, of data to be copied.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS                status;
    WDFMEMORY               memory;
    size_t                  outputBufferLength;

    status = WdfRequestRetrieveOutputMemory(Request, &memory);
    if( !NT_SUCCESS(status) ) {
        KdPrint(("WdfRequestRetrieveOutputMemory failed 0x%x\n",status));
        return status;
    }

    WdfMemoryGetBuffer(memory, &outputBufferLength);
    if (outputBufferLength < NumBytesToCopyFrom) {
        status = STATUS_INVALID_BUFFER_SIZE;
        KdPrint(("RequestCopyFromBuffer: buffer too small. Size %d, expect %d\n",
                (int)outputBufferLength, (int)NumBytesToCopyFrom));
        return status;
    }

    status = WdfMemoryCopyFromBuffer(memory,
                                    0,
                                    SourceBuffer,
                                    NumBytesToCopyFrom);
    if( !NT_SUCCESS(status) ) {
        KdPrint(("WdfMemoryCopyFromBuffer failed 0x%x\n",status));
        return status;
    }

    WdfRequestSetInformation(Request, NumBytesToCopyFrom);
    return status;
}

static __forceinline NTSTATUS completeEventRequest(WDFREQUEST Request, ptp_event_t* evt)
{
	NTSTATUS status = STATUS_SUCCESS;
	///
	if (evt->collectionType == MOUSE_CollectionType) {
		status = RequestCopyFromBuffer(Request, &evt->mReport, sizeof(mouse_report_t));
	}
	else {
		status = RequestCopyFromBuffer(Request, &evt->ptpReport, sizeof(PTP_REPORT));
	}
	
	WdfRequestComplete(Request, status);
	return status;
}

///???????? WDFCOLLECTION ????????????????????
static void processExistsRequests(DEV_EXT* ext)
{
	NTSTATUS status;
	WDFMEMORY  Memory;
	WDFREQUEST Request;

	while (TRUE) {
		Memory = (WDFMEMORY)WdfCollectionGetFirstItem(ext->ReportCollection);
		if (!Memory)break;
		status = WdfIoQueueRetrieveNextRequest(ext->ReportQueue, &Request);
		if (!NT_SUCCESS(status))break;
		////
		WdfCollectionRemoveItem(ext->ReportCollection, 0); ////
		//////
		ptp_event_t* evt = (ptp_event_t*)WdfMemoryGetBuffer(Memory, NULL);

		completeEventRequest(Request, evt);
		//////
		WdfObjectDelete(Memory);
	}
}

void mltp_Event(DEV_EXT* ext, ptp_event_t* evt) //????????????????
{

	////
	NTSTATUS status;
	WDFMEMORY Memory;
	////

	processExistsRequests(ext); //??????????????????????

	////
	WDFREQUEST Request;
	status = WdfIoQueueRetrieveNextRequest(ext->ReportQueue, &Request);
	if (NT_SUCCESS(status)) { //??????????????????????

		Memory = (WDFMEMORY)WdfCollectionGetFirstItem(ext->ReportCollection); //????????
		if (Memory) {

			WdfCollectionRemoveItem(ext->ReportCollection, 0); ////
			ptp_event_t* e = (ptp_event_t*)WdfMemoryGetBuffer(Memory, NULL);
			completeEventRequest(Request, e);

			WdfObjectDelete(Memory);
			/////
		}
		else {
			/////
			completeEventRequest(Request, evt);

			return;
		}
	}

	//////??????Collection??
	status = WdfMemoryCreateFromLookaside(ext->LookasideHandle, &Memory);
	if (!NT_SUCCESS(status)) {
		DPT("Error WdfMemoryCreateFromLookaside =0x%X\n", status);
		return;
	}

	status = WdfMemoryCopyFromBuffer(Memory, 0, evt, sizeof(ptp_event_t));
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(Memory);
		return;
	}

	status = WdfCollectionAdd(ext->ReportCollection, Memory);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(Memory);
		return;
	}
	/////

	processExistsRequests(ext); //?????????????????????? 

	//////
}

static VOID timerFunc(WDFTIMER timer)
{
	WDFDEVICE device = (WDFDEVICE)WdfTimerGetParentObject(timer);
	DEV_EXT* ext = GetDeviceContext(device);
	////
	BOOLEAN bOK = WdfRequestSend(ext->ReuseRequest, ext->IoTarget, NULL);

	if (!bOK) {
		DPT("In timerFunc: WdfRequestSend err\n");
		if (ext->bRequestStop) {
			DPT("In timerFunc: Device Removed\n");

			KeSetEvent(&ext->RequestEvent, 0, FALSE); ///
			
			return;
		}

		WdfTimerStart(ext->timerHandle, WDF_REL_TIMEOUT_IN_MS(200)); // ????????
	}
}

static void CompletionRoutine(
	WDFREQUEST Request,
	WDFIOTARGET Target,
	PWDF_REQUEST_COMPLETION_PARAMS Params,
	WDFCONTEXT Context)
{
	DEV_EXT* ext = (DEV_EXT*)Context;
	NTSTATUS status = WdfRequestGetStatus(Request);
	BOOLEAN bOK = TRUE;
	
	++ext->ActiveCount; /// !!!

	UNREFERENCED_PARAMETER(Target);
	UNREFERENCED_PARAMETER(Params);

	/////
	if (NT_SUCCESS(status)) { // success
		////
		LONG retlen = (LONG)WdfRequestGetInformation(Request);
		UINT8* data = (UINT8*)WdfMemoryGetBuffer(ext->RequestBuffer, NULL);

		///
		RegDebug(L"CompletionRoutine runtimes=", NULL, ext->runtimes++);
		/// 
		MouseLikeTouchPad_SPI_parse(ext, data, retlen);
		///////
	}
	else {
		bOK = FALSE;
	}
	
	/////??????????
	WDF_REQUEST_REUSE_PARAMS reuseParams;


	WDF_REQUEST_REUSE_PARAMS_INIT(
		&reuseParams,
		WDF_REQUEST_REUSE_NO_FLAGS,
		STATUS_NOT_SUPPORTED
	);

	WdfRequestReuse(Request, &reuseParams);//??????????????????????????????

	WdfIoTargetFormatRequestForInternalIoctl(ext->IoTarget, ext->ReuseRequest,
		IOCTL_HID_READ_REPORT,
		ext->RequestBuffer, NULL, ext->RequestBuffer, NULL); //?????????????????????????????? ??????MSDN

	WdfRequestSetCompletionRoutine(ext->ReuseRequest, CompletionRoutine, ext); //????????

	///
	if (ext->bRequestStop) {
		DPT("In completionRoutine: Device Removed\n");
		
		KeSetEvent(&ext->RequestEvent, 0, FALSE); ///

		return;
	}
	//////
	if (bOK) { //????????????????????????
		////
		bOK = WdfRequestSend(Request, ext->IoTarget, NULL);
		
	}
	
	//////
	if (!bOK) { //????????????????????????????????????????????????CPU
		///
		WdfTimerStart(ext->timerHandle, WDF_REL_TIMEOUT_IN_MS(200) );

		DPT("---- In completionRoute Request err = 0x%X\n", status );
	}
	////////
}

////
NTSTATUS create_reuse_request(DEV_EXT* ext)
{
	NTSTATUS status = STATUS_SUCCESS;
	WDF_OBJECT_ATTRIBUTES      RequestAttributes = { 0 };
	WDF_TIMER_CONFIG        timerConfig;
	WDF_OBJECT_ATTRIBUTES   timerAttributes;

	///create timer
	WDF_TIMER_CONFIG_INIT(&timerConfig, timerFunc);//???? ????????

	WDF_OBJECT_ATTRIBUTES_INIT(&timerAttributes); 
	timerAttributes.ParentObject = ext->hDevice; /// parent object

	status = WdfTimerCreate(&timerConfig,
		&timerAttributes,
		&ext->timerHandle);
	if (!NT_SUCCESS(status)) {
		DPT("WdfTimerCreate err=0x%X\n", status );
		return status;
	}

	///create IOCTL_HID_READ_REPORT Request
	WDF_OBJECT_ATTRIBUTES_INIT(&RequestAttributes);
	RequestAttributes.ParentObject = ext->hDevice; //????????????????????????????????????request
	status = WdfRequestCreate(&RequestAttributes,
		ext->IoTarget,
		&ext->ReuseRequest);
	if (!NT_SUCCESS(status)) {
		DPT("WdfRequestCreate err=0x%X\n", status );
		return status;
	}

	WDF_OBJECT_ATTRIBUTES_INIT(&RequestAttributes);
	RequestAttributes.ParentObject = ext->hDevice; //????????????????????????????????????
	status = WdfMemoryCreate( &RequestAttributes, NonPagedPool, PoolTag, REPORT_BUFFER_SIZE, &ext->RequestBuffer, NULL);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(ext->ReuseRequest); 
		ext->ReuseRequest = NULL;
		DPT("WdfMemoryCreate err=0x%X\n", status );
		return status;
	}

	status = WdfIoTargetFormatRequestForInternalIoctl(ext->IoTarget, ext->ReuseRequest, 
		IOCTL_HID_READ_REPORT,
		ext->RequestBuffer, NULL, ext->RequestBuffer, NULL); //????IOCTL????

	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(ext->ReuseRequest); ext->ReuseRequest = 0;
		WdfObjectDelete(ext->RequestBuffer); ext->RequestBuffer = NULL;
		DPT("WdfIoTargetFormatRequestForInternalIoctl err=0x%X\n", status );
		return status;
	}

	///

	WdfRequestSetCompletionRoutine(ext->ReuseRequest, CompletionRoutine, ext); //????????

	return status;
}


NTSTATUS
HidReadReport(
	DEV_EXT* pDevContext,
	WDFREQUEST Request,
	BOOLEAN* requestPendingFlag
)
{
	NTSTATUS status = STATUS_SUCCESS;
	*requestPendingFlag = FALSE;

	status = WdfRequestForwardToIoQueue(Request, pDevContext->ReportQueue);
	if (!NT_SUCCESS(status))
	{
		RegDebug(L"HidReadReport WdfRequestForwardToIoQueue failed", NULL, status);
		goto exit;
	}

	*requestPendingFlag = TRUE;
	//RegDebug(L"HidReadReport runtimes_ReadReport=", NULL, pDevContext->runtimes_ReadReport++);

exit:
	//RegDebug(L"HidReadReport end", NULL, status);
	return status;
}


///////????????????????????????????????????????????????????. 

struct mon_workitem
{
	WORK_QUEUE_ITEM item;
	DEV_EXT* ext;
};
static void workitem_thread(void* _p)
{
	mon_workitem* p = (mon_workitem*)_p;
	DEV_EXT* ext = p->ext;
	////
	const LONG MS = 200;
	const LONG PerCnt = 5 * 1000 / MS;
	const LONG AllCnt = 20; /// 1????

	ULONG activeCount = ext->ActiveCount;
	LONG cnt = 0;
	LONG Cnt2 = 0;
	while ( !ext->bRequestStop) {
		//
		LARGE_INTEGER d; d.QuadPart = WDF_REL_TIMEOUT_IN_MS(200);
		KeDelayExecutionThread(KernelMode, FALSE, &d ); //
		////
		cnt++;
		if (cnt >= PerCnt) {
			cnt = 0;
			Cnt2++;
			///
			if (ext->ActiveCount == activeCount) {  //??????????????????????????????????????????????

				////
				if (ext->ReuseRequest)WdfRequestCancelSentRequest(ext->ReuseRequest); //????????
				////
				NTSTATUS status = SetSpecialFeature(ext);
				DPT("--- Repeat SetSpecialFeature In Thread for Bootcamp... status=0x%X\n", status );
			}
			else {
				activeCount = ext->ActiveCount;
			}
			/////
		}
		if (Cnt2 >= AllCnt)break;
		////
	}
	////
	ExFreePool(p);
	DPT("Mon Thraed Quit.\n");
	KeSetEvent(&ext->RequestEvent2, 0, FALSE);
	////
}

NTSTATUS StartReadReport(DEV_EXT* ext)
{
	NTSTATUS status = STATUS_SUCCESS;

	ext->bRequestStop = FALSE;
	KeClearEvent(&ext->RequestEvent);

	/////
	WdfTimerStart(ext->timerHandle, WDF_REL_TIMEOUT_IN_MS(100));

	///????
	mon_workitem* m = (mon_workitem*)ExAllocatePoolWithTag(NonPagedPool, sizeof(mon_workitem), PoolTag);
	if (m) {
		KeClearEvent(&ext->RequestEvent2);

		m->ext = ext; ///

		ExInitializeWorkItem(&m->item, &workitem_thread, m );
		ExQueueWorkItem(&m->item, DelayedWorkQueue);
		////
	}

	return status;
}

void StopReadReport(DEV_EXT* ext)
{
	if (!ext->ReuseRequest || ext->bRequestStop )return;
	////
	ext->bRequestStop = TRUE; //????????
	WdfRequestCancelSentRequest(ext->ReuseRequest); //????????

	KeWaitForSingleObject(&ext->RequestEvent, Executive, KernelMode, FALSE, NULL); //????????????????

	KeWaitForSingleObject(&ext->RequestEvent2, Executive, KernelMode, FALSE, NULL); //????????????????

	/////
}

