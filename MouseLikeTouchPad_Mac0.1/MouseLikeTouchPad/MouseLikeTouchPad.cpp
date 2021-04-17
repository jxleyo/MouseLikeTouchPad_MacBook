#include "MouseLikeTouchPad.h"


//////
static NTSTATUS SetSpecialFeatureComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
	*Irp->UserIosb = Irp->IoStatus;

	KeSetEvent(Irp->UserEvent, 0, FALSE);

	IoFreeIrp(Irp);

	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Context);

	return STATUS_MORE_PROCESSING_REQUIRED;
}

////设置FEATURE, 用于开启 SPI的触摸板设备
NTSTATUS SetSpecialFeature(DEV_EXT* ext)
{
	NTSTATUS status = STATUS_SUCCESS;
	KEVENT event;
	IO_STATUS_BLOCK IoStatusBlock = { 0 };

	KeInitializeEvent(&event, SynchronizationEvent, FALSE);
	PIRP irp = IoAllocateIrp( WdfDeviceWdmGetDeviceObject(ext->hDevice)->StackSize + 2 , FALSE);
	if (!irp) {
		return STATUS_NO_MEMORY;
	}

	/// fill data
	static CHAR buffer[256]; ////
	HID_XFER_PACKET* hxp = (HID_XFER_PACKET*)buffer;
	hxp->reportBuffer = (PUCHAR)buffer + sizeof(HID_XFER_PACKET);
	hxp->reportBufferLen = 2;
	hxp->reportId = REPORTID_MOUSE;

	hxp->reportBuffer[0] = 0x02; // 应该是代表触摸板在SPI总线的位置
	hxp->reportBuffer[1] = 0x01; // 应该是开启的意思

	MouseLikeTouchPad_parse_init();//初始化触摸板解析器

	/////
	PDEVICE_OBJECT DeviceObject = WdfIoTargetWdmGetTargetDeviceObject(ext->IoTarget);
	/////
	irp->AssociatedIrp.SystemBuffer = buffer;
	irp->UserBuffer = buffer;
	irp->UserEvent = &event;
	irp->UserIosb = &IoStatusBlock;
	irp->Tail.Overlay.Thread = PsGetCurrentThread();
	irp->Tail.Overlay.OriginalFileObject = NULL;
	irp->RequestorMode = KernelMode;
	irp->Flags = 0;

	/////
	PIO_STACK_LOCATION irpStack = IoGetNextIrpStackLocation(irp);
	irpStack->DeviceObject = DeviceObject;
	irpStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;

	irpStack->Parameters.DeviceIoControl.IoControlCode = IOCTL_HID_SET_FEATURE;
	irpStack->Parameters.DeviceIoControl.InputBufferLength = sizeof(HID_XFER_PACKET);

	////
	IoSetCompletionRoutine(irp, SetSpecialFeatureComplete, 0, TRUE, TRUE, TRUE);

	status = IoCallDriver( DeviceObject, irp);
	if (status == STATUS_PENDING) {
		//
		KeWaitForSingleObject(&event, Executive, KernelMode, TRUE, 0);

		status = IoStatusBlock.Status;
	}


	//////
	return status;
}

NTSTATUS EvtDeviceD0Entry( WDFDEVICE Device, WDF_POWER_DEVICE_STATE PreviousState )
{
	DPT("Enter: EvtDeviceD0Entry \n");

	DEV_EXT* ext = GetDeviceContext(Device);
	////
	NTSTATUS status = SetSpecialFeature(ext);
	if (!NT_SUCCESS(status)) {
		DPT("IN EvtDeviceD0Entry : SetSpecialFeature err=0x%X\n", status );
		/////

	}

	/////
	StartReadReport(ext);  ///

	UNREFERENCED_PARAMETER(PreviousState);
	/////
	return STATUS_SUCCESS;
}

NTSTATUS EvtDeviceD0Exit( WDFDEVICE Device,  WDF_POWER_DEVICE_STATE TargetState)
{
	DEV_EXT* ext = GetDeviceContext(Device);
	////
	DPT("Exit: EvtDeviceD0Exit \n");
	
	StopReadReport(ext);

	UNREFERENCED_PARAMETER(TargetState);

	return STATUS_SUCCESS;
}

////////
VOID EvtInternalDeviceControl(
	IN WDFQUEUE     Queue,
	IN WDFREQUEST   Request,
	IN size_t       OutputBufferLength,
	IN size_t       InputBufferLength,
	IN ULONG        IoControlCode )
{
	NTSTATUS status = STATUS_SUCCESS;
	WDFDEVICE device = WdfIoQueueGetDevice(Queue);
	DEV_EXT* ext = GetDeviceContext(device);
	////

	UNREFERENCED_PARAMETER(OutputBufferLength);
	UNREFERENCED_PARAMETER(InputBufferLength);

	switch (IoControlCode) 
	{
	case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
		{
			DPT("++ IOCTL_HID_GET_DEVICE_DESCRIPTOR -- \n");
			///

			/////
			WDFMEMORY memory;
			status = WdfRequestRetrieveOutputMemory(Request, &memory);
			if (!NT_SUCCESS(status))break;

			status = WdfMemoryCopyFromBuffer(memory, 0, (PVOID)&DefaultHidDescriptor, DefaultHidDescriptor.bLength);
			if (!NT_SUCCESS(status))break;
			////
			WdfRequestSetInformation(Request, DefaultHidDescriptor.bLength);

		}
		break;

	case IOCTL_HID_GET_REPORT_DESCRIPTOR:
		{
		    DPT("--- IOCTL_HID_GET_REPORT_DESCRIPTOR -- \n");
			///
			WDFMEMORY memory;
			status = WdfRequestRetrieveOutputMemory(Request, &memory);
			if (!NT_SUCCESS(status)) {
				DPT("-- IOCTL_HID_DEVICE_DESC: err=0x%X\n", status );
				break;
			}

			LONG outlen = DefaultHidDescriptor.DescriptorList[0].wReportLength;
			status = WdfMemoryCopyFromBuffer(memory,
				0,
				(PVOID)MouseReportDescriptor,
				outlen );
			if (!NT_SUCCESS(status)) {
				break;
			}
			////
			WdfRequestSetInformation(Request, outlen );

		}
		break;

	case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
		{
			///
			DPT("-- IOCTL_HID_GET_DEVICE_ATTRIBUTES -- \n");
			PHID_DEVICE_ATTRIBUTES   attr = NULL;
			status = WdfRequestRetrieveOutputBuffer(Request,
				sizeof(HID_DEVICE_ATTRIBUTES),
				(PVOID*)&attr,
				NULL);
			if (!NT_SUCCESS(status)) {
				break;
			}
			////
			RtlZeroMemory(attr, sizeof(HID_DEVICE_ATTRIBUTES));
			attr->Size = sizeof(HID_DEVICE_ATTRIBUTES);

			attr->VendorID = 0x05AC;   //
			attr->ProductID = 0x0277;  //
			attr->VersionNumber = 0x0101; 
			///
			WdfRequestSetInformation(Request, sizeof(HID_DEVICE_ATTRIBUTES));
		}
		break;

	case IOCTL_HID_SET_FEATURE:
		{
			DPT("-- IOCTL_SET_FEATURE -- \n");
			////
		}
		break;

	case IOCTL_HID_READ_REPORT:
		{
			/////
	//		DPT("--IOCTL_HID_READ_REPORT \n");
			/////
			ProcessReadReport(ext, Request);
			return; 
			////////
		}
		break;

	default:
		DPT("Not Supported Code=0x%X, func_code=%d\n", IoControlCode, FUNCTION_FROM_CTL_CODE(IoControlCode) );
		status = STATUS_NOT_SUPPORTED;
	}

	/////complete irp
	WdfRequestComplete(Request, status);
	////
}

//需要处理 IRP_MN_QUERY_ID子请求，否则安装驱动后无法识别兼容鼠标驱动.
NTSTATUS EvtPnpQueryIds(WDFDEVICE device, PIRP Irp)
{
	NTSTATUS status = Irp->IoStatus.Status;

	PIO_STACK_LOCATION irpStack, previousSp;
	irpStack = IoGetCurrentIrpStackLocation(Irp);

	PDEVICE_OBJECT DeviceObject = WdfDeviceWdmGetDeviceObject(device);

	//
	// This check is required to filter out QUERY_IDs forwarded
	// by the HIDCLASS for the parent FDO. These IDs are sent
	// by PNP manager for the parent FDO if you root-enumerate this driver.
	//
	previousSp = ((PIO_STACK_LOCATION)((UCHAR *)(irpStack)+
		sizeof(IO_STACK_LOCATION)));

	if (previousSp->DeviceObject == DeviceObject) {
		//
		// Filtering out this basically prevents the Found New Hardware
		// popup for the root-enumerated VHIDMINI on reboot.
		//
		status = Irp->IoStatus.Status;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);

		return status;
	}

	/////
	switch (irpStack->Parameters.QueryId.IdType)
	{
	case BusQueryDeviceID:
	case BusQueryHardwareIDs:
		{
			PWCHAR buffer = (PWCHAR)ExAllocatePoolWithTag(
				NonPagedPool,
				VHID_HARDWARE_IDS_LENGTH,
				PoolTag);
			if (buffer) {
				//
				// Do the copy, store the buffer in the Irp
				//
				RtlCopyMemory(buffer,
					VHID_HARDWARE_IDS,
					VHID_HARDWARE_IDS_LENGTH
				);

				Irp->IoStatus.Information = (ULONG_PTR)buffer;
				status = STATUS_SUCCESS;
			}
			else {
				//
				//  No memory
				//
				status = STATUS_INSUFFICIENT_RESOURCES;
			}

			Irp->IoStatus.Status = status;

		}
		break;

	default:
		status = Irp->IoStatus.Status;
	}

	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

/////////////////

