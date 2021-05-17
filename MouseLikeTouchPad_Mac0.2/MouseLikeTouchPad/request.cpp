#define REPORT_BUFFER_SIZE   1024

#include "MouseLikeTouchPad.h"

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
static __forceinline NTSTATUS completeMouseEventRequest(WDFREQUEST Request, mouse_event_t* evt)
{
	NTSTATUS status = STATUS_SUCCESS;
	///
	mouse_report_t report; 
	report.report_id = REPORTID_MOUSE;
	report.button = evt->button;
	report.dx = (CHAR)evt->dx;
	report.dy = (CHAR)evt->dy;
	report.v_wheel = evt->v_wheel;
	report.h_wheel = evt->h_wheel;

	status = RequestCopyFromBuffer(Request, &report, sizeof(mouse_report_t));
	
	WdfRequestComplete(Request, status);
	return status;
}

///这里不对 WDFCOLLECTION 加锁，因为是串行处理
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
		mouse_event_t* evt = (mouse_event_t*)WdfMemoryGetBuffer(Memory, NULL);

		completeMouseEventRequest(Request, evt);
		//////
		WdfObjectDelete(Memory);
	}
}

static void mltp_MouseEvent(mouse_event_t* evt, void* ctx) //这个函数串行执行
{
	//
	DPT("button=%d, dx=%d, dy=%d, v_wheel=%d, h_wheel=%d\n", evt->button,evt->dx,evt->dy,evt->v_wheel,evt->h_wheel);
	////
	DEV_EXT* ext = (DEV_EXT*)ctx;
	NTSTATUS status;
	WDFMEMORY Memory;
	////

	processExistsRequests(ext); //首先处理队列中没处理的

	////
	WDFREQUEST Request;
	status = WdfIoQueueRetrieveNextRequest(ext->ReportQueue, &Request);
	if (NT_SUCCESS(status)) { //队列中有等待处理的请求

		Memory = (WDFMEMORY)WdfCollectionGetFirstItem(ext->ReportCollection); //再次判断
		if (Memory) {

			WdfCollectionRemoveItem(ext->ReportCollection, 0); ////
			mouse_event_t* e = (mouse_event_t*)WdfMemoryGetBuffer(Memory, NULL);
			completeMouseEventRequest(Request, e);

			WdfObjectDelete(Memory);
			/////
		}
		else {
			/////
			completeMouseEventRequest(Request, evt);

			return;
		}
	}

	//////添加到Collection，
	status = WdfMemoryCreateFromLookaside(ext->LookasideHandle, &Memory);
	if (!NT_SUCCESS(status)) {
		DPT("Error WdfMemoryCreateFromLookaside =0x%X\n", status );
		return;
	}
    
	status = WdfMemoryCopyFromBuffer(Memory, 0, evt, sizeof(mouse_event_t)); 
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

	processExistsRequests(ext); //再次处理队列中没处理的 

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

		WdfTimerStart(ext->timerHandle, WDF_REL_TIMEOUT_IN_MS(200)); // 重新开始
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
		MouseLikeTouchPad_parse(data, retlen);
		///////
	}
	else {
		bOK = FALSE;
	}
	
	/////重新初始化
	WDF_REQUEST_REUSE_PARAMS reuseParams;


	WDF_REQUEST_REUSE_PARAMS_INIT(
		&reuseParams,
		WDF_REQUEST_REUSE_NO_FLAGS,
		STATUS_NOT_SUPPORTED
	);

	WdfRequestReuse(Request, &reuseParams);//在完成函数里，函数返回总是成功

	WdfIoTargetFormatRequestForInternalIoctl(ext->IoTarget, ext->ReuseRequest,
		IOCTL_HID_READ_REPORT,
		ext->RequestBuffer, NULL, ext->RequestBuffer, NULL); //因为参数没变，函数返回总是成功 ，查看MSDN

	WdfRequestSetCompletionRoutine(ext->ReuseRequest, CompletionRoutine, ext); //完成函数

	///
	if (ext->bRequestStop) {
		DPT("In completionRoutine: Device Removed\n");
		
		KeSetEvent(&ext->RequestEvent, 0, FALSE); ///

		return;
	}
	//////
	if (bOK) { //成功，立即投递下一个请求
		////
		bOK = WdfRequestSend(Request, ext->IoTarget, NULL);
		
	}
	
	//////
	if (!bOK) { //不成功，延迟投递请求，主要是防止频繁出错投递占用CPU
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

	///init apple touchpad struct
	MouseLikeTouchPad_init(mltp_MouseEvent, ext);

	///create timer
	WDF_TIMER_CONFIG_INIT(&timerConfig, timerFunc);//默认 串行执行

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
	RequestAttributes.ParentObject = ext->hDevice; //设置父对象，父对象删除时候，自动删除request
	status = WdfRequestCreate(&RequestAttributes,
		ext->IoTarget,
		&ext->ReuseRequest);
	if (!NT_SUCCESS(status)) {
		DPT("WdfRequestCreate err=0x%X\n", status );
		return status;
	}

	WDF_OBJECT_ATTRIBUTES_INIT(&RequestAttributes);
	RequestAttributes.ParentObject = ext->hDevice; //设置父对象，父对象删除时候，自动删除
	status = WdfMemoryCreate( &RequestAttributes, NonPagedPool, PoolTag, REPORT_BUFFER_SIZE, &ext->RequestBuffer, NULL);
	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(ext->ReuseRequest); 
		ext->ReuseRequest = NULL;
		DPT("WdfMemoryCreate err=0x%X\n", status );
		return status;
	}

	status = WdfIoTargetFormatRequestForInternalIoctl(ext->IoTarget, ext->ReuseRequest, 
		IOCTL_HID_READ_REPORT,
		ext->RequestBuffer, NULL, ext->RequestBuffer, NULL); //设置IOCTL请求

	if (!NT_SUCCESS(status)) {
		WdfObjectDelete(ext->ReuseRequest); ext->ReuseRequest = 0;
		WdfObjectDelete(ext->RequestBuffer); ext->RequestBuffer = NULL;
		DPT("WdfIoTargetFormatRequestForInternalIoctl err=0x%X\n", status );
		return status;
	}

	///

	WdfRequestSetCompletionRoutine(ext->ReuseRequest, CompletionRoutine, ext); //完成函数

	return status;
}

NTSTATUS ProcessReadReport(DEV_EXT* ext, WDFREQUEST Request)
{
	NTSTATUS status = STATUS_SUCCESS;

	/////
	status = WdfRequestForwardToIoQueue(Request, ext->ReportQueue);
	if (!NT_SUCCESS(status)) {
		DPT("WdfRequestForwardToIoQueue err=0x%X\n", status );
		WdfRequestComplete(Request, status);
		///
		return status;
	}

	////

	return status;
}

///////创建一个监控线程为了解决开机运行系统后，触摸板被停止. 

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
	const LONG AllCnt = 20; /// 1分钟

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
			if (ext->ActiveCount == activeCount) {  //没发生变化，触摸板没动或者出了问题，重新初始化

				////
				if (ext->ReuseRequest)WdfRequestCancelSentRequest(ext->ReuseRequest); //取消请求
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

	///监控
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
	ext->bRequestStop = TRUE; //停止指示
	WdfRequestCancelSentRequest(ext->ReuseRequest); //取消请求

	KeWaitForSingleObject(&ext->RequestEvent, Executive, KernelMode, FALSE, NULL); //等待请求完全结束

	KeWaitForSingleObject(&ext->RequestEvent2, Executive, KernelMode, FALSE, NULL); //等待监控线程结束

	/////
}

