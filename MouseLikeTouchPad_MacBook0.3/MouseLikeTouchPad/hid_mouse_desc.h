#pragma once

#define REPORTID_MOUSE  0x02

#define REPORTID_FEATURE 0x03
#define REPORTID_TOUCHPAD 0x04
#define REPORTID_MAX_COUNT 0x05
#define REPORTID_PTPHQA 0x08
#define REPORTID_STANDARDMOUSE 0x02
#define REPORTID_MULTITOUCH 0x05
#define REPORTID_REPORTMODE 0x04
#define REPORTID_FUNCTION_SWITCH 0x06   
#define REPORTID_DEVICE_CAPS 0x07
#define REPORTID_UMAPP_CONF  0x09


const unsigned char TouchpadReportDescriptor[] = {//关键问题是每个REPORTID_必须不同以区分报告类别，并且值在1-255之间
	//TOUCH PAD input TLC
		0x05, 0x0d,                         // USAGE_PAGE (Digitizers)          
		0x09, 0x05,                         // USAGE (Touch Pad)             
		0xa1, 0x01,                         // COLLECTION (Application)         
		0x85, REPORTID_MULTITOUCH,            //   REPORT_ID (Touch pad)      //REPORTID_MULTITOUCH       REPORTID_TOUCHPAD               
		0x09, 0x22,                         //   USAGE (Finger)                 
		0xa1, 0x02,                         //   COLLECTION (Logical)  
		0x15, 0x00,                         //       LOGICAL_MINIMUM (0)
		0x25, 0x01,                         //       LOGICAL_MAXIMUM (1)
		0x09, 0x47,                         //       USAGE (Confidence) 
		0x09, 0x42,                         //       USAGE (Tip switch)
		0x95, 0x02,                         //       REPORT_COUNT (2)
		0x75, 0x01,                         //       REPORT_SIZE (1)
		0x81, 0x02,                         //       INPUT (Data,Var,Abs)
		0x95, 0x01,                         //       REPORT_COUNT (1)
		0x75, 0x02,                         //       REPORT_SIZE (2)
		0x25, 0x02,                         //       LOGICAL_MAXIMUM (2)
		0x09, 0x51,                         //       USAGE (Contact Identifier)
		0x81, 0x02,                         //       INPUT (Data,Var,Abs)
		0x75, 0x01,                         //       REPORT_SIZE (1)
		0x95, 0x04,                         //       REPORT_COUNT (4)             
		0x81, 0x03,                         //       INPUT (Cnst,Var,Abs)
		0x05, 0x01,                         //       USAGE_PAGE (Generic Desk..
		0x15, 0x00,                         //       LOGICAL_MINIMUM (0)
		0x26, 0xff, 0x0f,                   //       LOGICAL_MAXIMUM (4095)         
		0x75, 0x10,                         //       REPORT_SIZE (16)             
		0x55, 0x0e,                         //       UNIT_EXPONENT (-2)           
		0x65, 0x13,                         //       UNIT(Inch,EngLinear)                  
		0x09, 0x30,                         //       USAGE (X)                    
		0x35, 0x00,                         //       PHYSICAL_MINIMUM (0)         
		0x46, 0x90, 0x01,                   //       PHYSICAL_MAXIMUM (400)
		0x95, 0x01,                         //       REPORT_COUNT (1)         
		0x81, 0x02,                         //       INPUT (Data,Var,Abs)         
		0x46, 0x13, 0x01,                   //       PHYSICAL_MAXIMUM (275)
		0x09, 0x31,                         //       USAGE (Y)                    
		0x81, 0x02,                         //       INPUT (Data,Var,Abs)    
		0xc0,                               //    END_COLLECTION
		0x55, 0x0C,                         //    UNIT_EXPONENT (-4)           
		0x66, 0x01, 0x10,                   //    UNIT (Seconds)        
		0x47, 0xff, 0xff, 0x00, 0x00,      //     PHYSICAL_MAXIMUM (65535)
		0x27, 0xff, 0xff, 0x00, 0x00,         //  LOGICAL_MAXIMUM (65535) 
		0x75, 0x10,                           //  REPORT_SIZE (16)             
		0x95, 0x01,                           //  REPORT_COUNT (1) 
		0x05, 0x0d,                         //    USAGE_PAGE (Digitizers)
		0x09, 0x56,                         //    USAGE (Scan Time)    
		0x81, 0x02,                           //  INPUT (Data,Var,Abs)         
		0x09, 0x54,                         //    USAGE (Contact count)
		0x25, 0x7f,                           //  LOGICAL_MAXIMUM (127) 
		0x95, 0x01,                         //    REPORT_COUNT (1)
		0x75, 0x08,                         //    REPORT_SIZE (8)    
		0x81, 0x02,                         //    INPUT (Data,Var,Abs)
		0x05, 0x09,                         //    USAGE_PAGE (Button)         
		0x09, 0x01,                         //    USAGE_(Button 1)     
		0x25, 0x01,                         //    LOGICAL_MAXIMUM (1)          
		0x75, 0x01,                         //    REPORT_SIZE (1)              
		0x95, 0x01,                         //    REPORT_COUNT (1)             
		0x81, 0x02,                         //    INPUT (Data,Var,Abs)
		0x95, 0x07,                          //   REPORT_COUNT (7)                 
		0x81, 0x03,                         //    INPUT (Cnst,Var,Abs)
		0x05, 0x0d,                         //    USAGE_PAGE (Digitizer)
		0x85, REPORTID_DEVICE_CAPS,            //   REPORT_ID (Feature)  /硬件支持点数    REPORTID_DEVICE_CAPS，REPORTID_MAX_COUNT,                   
		0x09, 0x55,                         //    USAGE (Contact Count Maximum)
		0x09, 0x59,                         //    USAGE (Pad TYpe)
		0x75, 0x04,                         //    REPORT_SIZE (4) 
		0x95, 0x02,                         //    REPORT_COUNT (2)
		0x25, 0x0f,                         //    LOGICAL_MAXIMUM (15)
		0xb1, 0x02,                         //    FEATURE (Data,Var,Abs)
		0x06, 0x00, 0xff,                   //    USAGE_PAGE (Vendor Defined)
		0x85, REPORTID_PTPHQA,               //    REPORT_ID (PTPHQA)  
		0x09, 0xC5,                         //    USAGE (Vendor Usage 0xC5)    
		0x15, 0x00,                         //    LOGICAL_MINIMUM (0)          
		0x26, 0xff, 0x00,                   //    LOGICAL_MAXIMUM (0xff) 
		0x75, 0x08,                         //    REPORT_SIZE (8)             
		0x96, 0x00, 0x01,                   //    REPORT_COUNT (0x100 (256))             
		0xb1, 0x02,                         //    FEATURE (Data,Var,Abs)
		0xc0,                               // END_COLLECTION
		//CONFIG TLC
		0x05, 0x0d,                         //    USAGE_PAGE (Digitizer)
		0x09, 0x0E,                         //    USAGE (Configuration)
		0xa1, 0x01,                         //   COLLECTION (Application)
		0x85, REPORTID_REPORTMODE,             //   REPORT_ID (Feature)      REPORTID_FEATURE       
		0x09, 0x22,                         //   USAGE (Finger)              
		0xa1, 0x02,                         //   COLLECTION (logical)     
		0x09, 0x52,                         //    USAGE (Input Mode)         
		0x15, 0x00,                         //    LOGICAL_MINIMUM (0)      
		0x25, 0x0a,                         //    LOGICAL_MAXIMUM (10)
		0x75, 0x08,                         //    REPORT_SIZE (8)         
		0x95, 0x01,                         //    REPORT_COUNT (1)         
		0xb1, 0x02,                         //    FEATURE (Data,Var,Abs    
		0xc0,                               //   END_COLLECTION
		0x09, 0x22,                         //   USAGE (Finger)              
		0xa1, 0x00,                         //   COLLECTION (physical)     
		0x85, REPORTID_FUNCTION_SWITCH,     //     REPORT_ID (Feature)              
		0x09, 0x57,                         //     USAGE(Surface switch)
		0x09, 0x58,                         //     USAGE(Button switch)
		0x75, 0x01,                         //     REPORT_SIZE (1)
		0x95, 0x02,                         //     REPORT_COUNT (2)
		0x25, 0x01,                         //     LOGICAL_MAXIMUM (1)
		0xb1, 0x02,                         //     FEATURE (Data,Var,Abs)
		0x95, 0x06,                         //     REPORT_COUNT (6)             
		0xb1, 0x03,                         //     FEATURE (Cnst,Var,Abs)
		0xc0,                               //   END_COLLECTION
		0xc0,                               // END_COLLECTION
		//MOUSE TLC
		0x05, 0x01,                         // USAGE_PAGE (Generic Desktop)     
		0x09, 0x02,                         // USAGE (Mouse)                    
		0xa1, 0x01,                         // COLLECTION (Application)        
		0x85, REPORTID_MOUSE,               //   REPORT_ID (Mouse)              
		0x09, 0x01,                         //   USAGE (Pointer)                
		0xa1, 0x00,                         //   COLLECTION (Physical)          
		0x05, 0x09,                         //     USAGE_PAGE (Button)          
		0x19, 0x01,                         //     USAGE_MINIMUM (Button 1)     
		0x29, 0x02,                         //     USAGE_MAXIMUM (Button 2)     
		0x25, 0x01,                         //     LOGICAL_MAXIMUM (1)          
		0x75, 0x01,                         //     REPORT_SIZE (1)              
		0x95, 0x02,                         //     REPORT_COUNT (2)             
		0x81, 0x02,                         //     INPUT (Data,Var,Abs)         
		0x95, 0x06,                         //     REPORT_COUNT (6)             
		0x81, 0x03,                         //     INPUT (Cnst,Var,Abs)         
		0x05, 0x01,                         //     USAGE_PAGE (Generic Desktop) 
		0x09, 0x30,                         //     USAGE (X)                    
		0x09, 0x31,                         //     USAGE (Y)                    
		0x75, 0x10,                         //     REPORT_SIZE (16)             
		0x95, 0x02,                         //     REPORT_COUNT (2)             
		0x25, 0x0a,                          //    LOGICAL_MAXIMUM (10)      
		0x81, 0x06,                         //     INPUT (Data,Var,Rel)         
		0xc0,                               //   END_COLLECTION                 
		0xc0,                                //END_COLLECTION      

};


/////
const unsigned char MouseReportDescriptor[] = {
	///
	0x05, 0x01, // USAGE_PAGE(Generic Desktop)
	0x09, 0x02, //   USAGE(Mouse)
	0xA1, 0x01, //   COLLECTION(APPlication)
	0x09, 0x01, //   USAGE(Pointer)
	0xA1, 0x00, //     COLLECTION(Physical)
	0x85, REPORTID_MOUSE, //     ReportID(Mouse ReportID)
	0x05, 0x09, //     USAGE_PAGE(Button)
	0x19, 0x01, //     USAGE_MINIMUM(button 1)   Button 按键， 位 0 左键， 位1 右键， 位2 中键
	0x29, 0x03, //     USAGE_MAXMUM(button 3)  //0x03限制最大的鼠标按键数量
	0x15, 0x00, //     LOGICAL_MINIMUM(0)
	0x25, 0x01, //     LOGICAL_MAXIMUM(1)
	0x95, 0x03, //     REPORT_COUNT(3)  //0x03鼠标按键数量
	0x75, 0x01, //     REPORT_SIZE(1)
	0x81, 0x02, //     INPUT(Data,Var,Abs)
	0x95, 0x01, //     REPORT_COUNT(1)
	0x75, 0x05, //     REPORT_SIZE(5)  //需要补足多少个bit使得加上鼠标按键数量的3个bit位成1个字节8bit
	0x81, 0x03, //     INPUT(Data,Var, Abs)
	0x05, 0x01, //     USAGE_PAGE(Generic Desktop)
	0x09, 0x30, //     USAGE(X)       X移动
	0x09, 0x31, //     USAGE(Y)       Y移动
	0x09, 0x38, //     USAGE(Wheel)   垂直滚动
	0x15, 0x81, //     LOGICAL_MINIMUM(-127)
	0x25, 0x7F, //     LOGICAL_MAXIMUM(127)
	0x75, 0x08, //     REPORT_SIZE(8)
	0x95, 0x03, //     REPORT_COUNT(3)
	0x81, 0x06, //     INPUT(Data,Var, Rel) //X,Y,垂直滚轮三个参数， 相对值

	//下边水平滚动
	0x05, 0x0C, //     USAGE_PAGE (Consumer Devices)
	0x0A, 0x38, 0x02, // USAGE(AC Pan)
	0x15, 0x81, //       LOGICAL_MINIMUM(-127)
	0x25, 0x7F, //       LOGICAL_MAXIMUM(127)
	0x75, 0x08, //       REPORT_SIZE(8)
	0x95, 0x01, //       REPORT_COUNT(1)
	0x81, 0x06, //       INPUT(data,Var, Rel) //水平滚轮，相对值
	0xC0,       //       End Connection(PhySical)
	0xC0,       //     End Connection

	/////////////

};

CONST HID_DESCRIPTOR DefaultHidDescriptor = {
	0x09,   // length of HID descriptor
	0x21,   // descriptor type == HID  0x21
	0x0100, // hid spec release
	0x00,   // country code == Not Specified
	0x01,   // number of HID class descriptors
	{ 0x22,   // descriptor type 
	sizeof(MouseReportDescriptor) }  // total length of report descriptor
};

#pragma pack(push)
#pragma pack(1)
typedef struct _PTP_CONTACT {
	UCHAR		Confidence : 1;
	UCHAR		TipSwitch : 1;
	UCHAR		ContactID : 3;
	UCHAR		Padding : 3;
	USHORT		X;
	USHORT		Y;
} PTP_CONTACT, * PPTP_CONTACT;
#pragma pack(pop)

typedef struct _PTP_REPORT {
	UCHAR       ReportID;
	PTP_CONTACT Contacts[5];
	USHORT      ScanTime;
	UCHAR       ContactCount;
	UCHAR       IsButtonClicked;
} PTP_REPORT, * PPTP_REPORT;

///鼠标状态报告,对应的HID是上边的报告
#pragma pack(1)
struct mouse_report_t
{
	BYTE    report_id;
	BYTE    button; //0x000000mrl 8bit值,mrl分别对应m中键r右键、l左键的状态，
	CHAR    dx;
	CHAR    dy;
	CHAR    v_wheel; // 垂直
	CHAR    h_wheel; // 水平
};
#pragma pack()

