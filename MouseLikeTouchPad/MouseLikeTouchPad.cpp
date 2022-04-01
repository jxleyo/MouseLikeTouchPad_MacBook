#include "MouseLikeTouchPad.h"
#define debug_on 0


VOID RegDebug(WCHAR* strValueName, PVOID dataValue, ULONG datasizeValue)//RegDebug(L"Run debug here",pBuffer,pBufferSize);//RegDebug(L"Run debug here",NULL,0x12345678);
{
	if (!debug_on) {//调试开关
		return;
	}

	//初始化注册表项
	UNICODE_STRING stringKey;
	RtlInitUnicodeString(&stringKey, L"\\Registry\\Machine\\Software\\RegDebug");

	//初始化OBJECT_ATTRIBUTES结构
	OBJECT_ATTRIBUTES  ObjectAttributes;
	InitializeObjectAttributes(&ObjectAttributes, &stringKey, OBJ_CASE_INSENSITIVE, NULL, NULL);//OBJ_CASE_INSENSITIVE对大小写敏感

	//创建注册表项
	HANDLE hKey;
	ULONG Des;
	NTSTATUS status = ZwCreateKey(&hKey, KEY_ALL_ACCESS, &ObjectAttributes, 0, NULL, REG_OPTION_NON_VOLATILE, &Des);
	if (NT_SUCCESS(status))
	{
		if (Des == REG_CREATED_NEW_KEY)
		{
			KdPrint(("新建注册表项！\n"));
		}
		else
		{
			KdPrint(("要创建的注册表项已经存在！\n"));
		}
	}
	else {
		return;
	}

	//初始化valueName
	UNICODE_STRING valueName;
	RtlInitUnicodeString(&valueName, strValueName);

	if (dataValue == NULL) {
		//设置REG_DWORD键值
		status = ZwSetValueKey(hKey, &valueName, 0, REG_DWORD, &datasizeValue, 4);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("设置REG_DWORD键值失败！\n"));
		}
	}
	else {
		//设置REG_BINARY键值
		status = ZwSetValueKey(hKey, &valueName, 0, REG_BINARY, dataValue, datasizeValue);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("设置REG_BINARY键值失败！\n"));
		}
	}
	ZwFlushKey(hKey);
	//关闭注册表句柄
	ZwClose(hKey);
}


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

NTSTATUS
EvtDevicePrepareHardware(
	_In_ WDFDEVICE Device,
	_In_ WDFCMRESLIST ResourceList,
	_In_ WDFCMRESLIST ResourceListTranslated
)
{
	NTSTATUS Status = STATUS_SUCCESS;
	DEV_EXT* pDeviceContext;

	WDF_MEMORY_DESCRIPTOR HidAttributeMemoryDescriptor;
	HID_DEVICE_ATTRIBUTES DeviceAttributes;

	const SPI_TRACKPAD_INFO* pTrackpadInfo;
	BOOLEAN DeviceFound = FALSE;

	PAGED_CODE();
	UNREFERENCED_PARAMETER(ResourceList);
	UNREFERENCED_PARAMETER(ResourceListTranslated);


	pDeviceContext = GetDeviceContext(Device);
	if (pDeviceContext == NULL)
	{
		Status = STATUS_INVALID_DEVICE_STATE;
		goto exit;
	}

	// Request device attribute descriptor for self-identification.
	RtlZeroMemory(&DeviceAttributes, sizeof(DeviceAttributes));
	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(
		&HidAttributeMemoryDescriptor,
		(PVOID)&DeviceAttributes,
		sizeof(DeviceAttributes)
	);

	Status = WdfIoTargetSendInternalIoctlSynchronously(
		pDeviceContext->IoTarget,
		NULL,
		IOCTL_HID_GET_DEVICE_ATTRIBUTES,
		NULL,
		&HidAttributeMemoryDescriptor,
		NULL,
		NULL
	);

	if (!NT_SUCCESS(Status))
	{
		RegDebug(L"EvtDevicePrepareHardware WdfIoTargetSendInternalIoctlSynchronously failed", NULL, Status);
		goto exit;
	}

	pDeviceContext->HidVendorID = DeviceAttributes.VendorID;
	pDeviceContext->HidProductID = DeviceAttributes.ProductID;
	pDeviceContext->HidVersionNumber = DeviceAttributes.VersionNumber;

	// Find proper metadata in HID registry
	for (pTrackpadInfo = SpiTrackpadConfigTable; pTrackpadInfo->VendorId; ++pTrackpadInfo)
	{
		if (pTrackpadInfo->VendorId == DeviceAttributes.VendorID &&
			pTrackpadInfo->ProductId == DeviceAttributes.ProductID)
		{
			pDeviceContext->TrackpadInfo.ProductId = pTrackpadInfo->ProductId;
			pDeviceContext->TrackpadInfo.VendorId = pTrackpadInfo->VendorId;
			pDeviceContext->TrackpadInfo.XMin = pTrackpadInfo->XMin;
			pDeviceContext->TrackpadInfo.XMax = pTrackpadInfo->XMax;
			pDeviceContext->TrackpadInfo.YMin = pTrackpadInfo->YMin;
			pDeviceContext->TrackpadInfo.YMax = pTrackpadInfo->YMax;

			RegDebug(L"TrackpadInfo.XMin=", NULL, pDeviceContext->TrackpadInfo.XMin);
			RegDebug(L"TrackpadInfo.XMax=", NULL, pDeviceContext->TrackpadInfo.XMax);
			RegDebug(L"TrackpadInfo.YMin=", NULL, pDeviceContext->TrackpadInfo.YMin);
			RegDebug(L"TrackpadInfo.YMax=", NULL, pDeviceContext->TrackpadInfo.YMax);
			DeviceFound = TRUE;
			break;
		}
	}

	if (!DeviceFound)
	{
		Status = STATUS_NOT_FOUND;
		RegDebug(L"EvtDevicePrepareHardware DeviceFound err", NULL, Status);
		goto exit;
	}

	RegDebug(L"EvtDevicePrepareHardware ok", NULL, Status);

	// We don't really care if that param read fails.
	Status = STATUS_SUCCESS;

exit:
	RegDebug(L"EvtDevicePrepareHardware end", NULL, Status);
	return Status;
}

NTSTATUS EvtDeviceD0Entry( WDFDEVICE Device, WDF_POWER_DEVICE_STATE PreviousState )
{
	DPT("Enter: EvtDeviceD0Entry \n");

	DEV_EXT* ext = GetDeviceContext(Device);
	////
	MouseLikeTouchPad_parse_init(ext);//初始化触摸板解析器
	NTSTATUS status = SetSpecialFeature(ext);
	if (!NT_SUCCESS(status)) {
		DPT("IN EvtDeviceD0Entry : SetSpecialFeature err=0x%X\n", status );
		/////

	}

	/////
	StartReadReport(ext);  ///
	ext->runtimes_ReadReport = 0;
	ext->PtpInputModeOn = FALSE;

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
	BOOLEAN requestPendingFlag = FALSE;

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
				(PVOID)ParallelMode_PtpReportDescriptor,//ParallelMode_PtpReportDescriptor//MouseReportDescriptor
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

			attr->VendorID = ext->HidVendorID;
			attr->ProductID = ext->HidProductID;
			attr->VersionNumber = ext->HidVersionNumber;
			RegDebug(L"attr->VendorID=", NULL, attr->VendorID);
			RegDebug(L"attr->ProductID=", NULL, attr->ProductID);
			RegDebug(L"attr->VersionNumber=", NULL, attr->VersionNumber);
			///
			WdfRequestSetInformation(Request, sizeof(HID_DEVICE_ATTRIBUTES));
		}
		break;

	case IOCTL_HID_GET_FEATURE:
	{
		RegDebug(L"IOCTL_HID_GET_FEATURE", NULL, 0);

		//status = HidGetFeature(pDevContext, Request, ReportTypeFeature);

		status = PtpReportFeatures(
			device,
			Request
		);

		break;
	}

	case IOCTL_HID_SET_FEATURE:
		{
			DPT("-- IOCTL_SET_FEATURE -- \n");
			////
		}
		break;

	case IOCTL_HID_READ_REPORT:
		{
			status = HidReadReport(ext, Request, &requestPendingFlag);
			if (requestPendingFlag) {
				return;
			}
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

NTSTATUS
PtpReportFeatures(
	_In_ WDFDEVICE Device,
	_In_ WDFREQUEST Request
)
{
	NTSTATUS Status;
	DEV_EXT* pDevContext;
	PHID_XFER_PACKET pHidPacket;
	WDF_REQUEST_PARAMETERS RequestParameters;
	size_t ReportSize;

	PAGED_CODE();

	Status = STATUS_SUCCESS;
	pDevContext = GetDeviceContext(Device);

	WDF_REQUEST_PARAMETERS_INIT(&RequestParameters);
	WdfRequestGetParameters(Request, &RequestParameters);

	if (RequestParameters.Parameters.DeviceIoControl.OutputBufferLength < sizeof(HID_XFER_PACKET))
	{
		RegDebug(L"STATUS_BUFFER_TOO_SMALL", NULL, 0x12345678);
		Status = STATUS_BUFFER_TOO_SMALL;
		goto exit;
	}

	pHidPacket = (PHID_XFER_PACKET)WdfRequestWdmGetIrp(Request)->UserBuffer;
	if (pHidPacket == NULL)
	{
		RegDebug(L"STATUS_INVALID_DEVICE_REQUEST", NULL, 0x12345678);
		Status = STATUS_INVALID_DEVICE_REQUEST;
		goto exit;
	}

	UCHAR reportId = pHidPacket->reportId;
	if (reportId == FAKE_REPORTID_DEVICE_CAPS) {//FAKE_REPORTID_DEVICE_CAPS//pDevContext->REPORTID_DEVICE_CAPS
		ReportSize = sizeof(PTP_DEVICE_CAPS_FEATURE_REPORT);
		if (pHidPacket->reportBufferLen < ReportSize) {
			Status = STATUS_INVALID_BUFFER_SIZE;
			RegDebug(L"PtpGetFeatures REPORTID_DEVICE_CAPS STATUS_INVALID_BUFFER_SIZE", NULL, pHidPacket->reportId);
			goto exit;
		}

		PPTP_DEVICE_CAPS_FEATURE_REPORT capsReport = (PPTP_DEVICE_CAPS_FEATURE_REPORT)pHidPacket->reportBuffer;

		capsReport->MaximumContactPoints = PTP_MAX_CONTACT_POINTS;// pDevContext->CONTACT_COUNT_MAXIMUM;// PTP_MAX_CONTACT_POINTS;
		capsReport->ButtonType = PTP_BUTTON_TYPE_CLICK_PAD;// pDevContext->PAD_TYPE;// PTP_BUTTON_TYPE_CLICK_PAD;
		capsReport->ReportID = FAKE_REPORTID_DEVICE_CAPS;// pDevContext->REPORTID_DEVICE_CAPS;//FAKE_REPORTID_DEVICE_CAPS
		RegDebug(L"PtpGetFeatures pHidPacket->reportId REPORTID_DEVICE_CAPS", NULL, pHidPacket->reportId);
		RegDebug(L"PtpGetFeatures REPORTID_DEVICE_CAPS MaximumContactPoints", NULL, capsReport->MaximumContactPoints);
		RegDebug(L"PtpGetFeatures REPORTID_DEVICE_CAPS REPORTID_DEVICE_CAPS ButtonType", NULL, capsReport->ButtonType);
	}
	else if (reportId == FAKE_REPORTID_PTPHQA) {//FAKE_REPORTID_PTPHQA//pDevContext->REPORTID_PTPHQA
			// Size sanity check
		ReportSize = sizeof(PTP_DEVICE_HQA_CERTIFICATION_REPORT);
		if (pHidPacket->reportBufferLen < ReportSize)
		{
			Status = STATUS_INVALID_BUFFER_SIZE;
			RegDebug(L"PtpGetFeatures REPORTID_PTPHQA STATUS_INVALID_BUFFER_SIZE", NULL, pHidPacket->reportId);
			goto exit;
		}

		PPTP_DEVICE_HQA_CERTIFICATION_REPORT certReport = (PPTP_DEVICE_HQA_CERTIFICATION_REPORT)pHidPacket->reportBuffer;

		*certReport->CertificationBlob = DEFAULT_PTP_HQA_BLOB;
		certReport->ReportID = FAKE_REPORTID_PTPHQA;//FAKE_REPORTID_PTPHQA//pDevContext->REPORTID_PTPHQA
		pDevContext->PtpInputModeOn = TRUE;//测试

		RegDebug(L"PtpGetFeatures pHidPacket->reportId REPORTID_PTPHQA", NULL, pHidPacket->reportId);

	}
	else {

		Status = STATUS_NOT_SUPPORTED;
		RegDebug(L"PtpGetFeatures pHidPacket->reportId STATUS_NOT_SUPPORTED", NULL, pHidPacket->reportId);
		goto exit;
	}

	WdfRequestSetInformation(Request, ReportSize);
	RegDebug(L"PtpGetFeatures STATUS_SUCCESS pDeviceContext->PtpInputOn", NULL, pDevContext->PtpInputModeOn);


exit:

	return Status;
}