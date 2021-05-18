#pragma once

#include <ntddk.h>

#include <wdf.h>

#include <hidport.h>

#include "apple_tp_proto.h"
#include "hid_mouse_desc.h"
#include <ntstrsafe.h>

#define DPT DbgPrint

#define FUNCTION_FROM_CTL_CODE(ctrlCode) (((ULONG)((ctrlCode) & 0x3FFC)) >> 2)

////
#define VHID_HARDWARE_IDS    L"HID\\MouseLikeTouchPad\0\0"
#define VHID_HARDWARE_IDS_LENGTH sizeof (VHID_HARDWARE_IDS)
#define PoolTag               'mltp'



////
struct DEV_EXT
{
	WDFDEVICE     hDevice;
	WDFIOTARGET   IoTarget; ///过滤驱动的next device

	/////
	WDFLOOKASIDE  LookasideHandle;
	WDFCOLLECTION ReportCollection;
	////
	WDFQUEUE      ReportQueue; //上层的Request请求，底层还没产生数据，暂时挂起这个请求
	///
	BOOLEAN       bRequestStop;   //指示请求停止
	KEVENT        RequestEvent;   //请求等待停止事件
	KEVENT        RequestEvent2;   //请求等待停止事件
	WDFREQUEST    ReuseRequest;   //重复使用的 Request
	WDFMEMORY     RequestBuffer;  // 请求的Buffer

	WDFTIMER      timerHandle;  //用于控制请求

	ULONG         ActiveCount;
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEV_EXT, GetDeviceContext)

/////
////// function

NTSTATUS EvtDeviceAdd(IN WDFDRIVER Driver, IN PWDFDEVICE_INIT DeviceInit);

VOID EvtInternalDeviceControl(
	IN WDFQUEUE     Queue,
	IN WDFREQUEST   Request,
	IN size_t       OutputBufferLength,
	IN size_t       InputBufferLength,
	IN ULONG        IoControlCode);

NTSTATUS EvtPnpQueryIds(WDFDEVICE device, PIRP Irp);

NTSTATUS create_reuse_request(DEV_EXT* ext);

NTSTATUS ProcessReadReport(DEV_EXT* ext, WDFREQUEST Request);

NTSTATUS EvtDeviceD0Entry(WDFDEVICE Device, WDF_POWER_DEVICE_STATE PreviousState);
NTSTATUS EvtDeviceD0Exit(WDFDEVICE Device, WDF_POWER_DEVICE_STATE TargetState);

NTSTATUS StartReadReport(DEV_EXT* ext);
void StopReadReport(DEV_EXT* ext);

NTSTATUS SetSpecialFeature(DEV_EXT* ext);

