/// 测试适合 macbook pro 2017 13寸带 multi-touch bar.  Apple SPI Device 总线驱动日期 2016/5/26， 版本 6.1.6500.0
/// Apple SPI Device 总线驱动传递上来的数据结构，根据实际测试获取，因为这种数据结构不公开，在以后的 Apple SPI Device版本可能被Apple公司修改。 

#pragma once

typedef unsigned short __le16;
typedef unsigned char  u8;

#define STABLE_INTERVAL_MSEC         100   // 手指按到触摸板的稳定时间间隔 

#define MouseReport_INTERVAL_MSEC     4   // 鼠标报告间隔时间ms，以频率250hz为基准
#define MButton_Interval_MSEC         100   // 鼠标左中右键与指针操作间隔时间ms，

#define Jitter_Time_MSEC         50   // 修正触摸点抖动的时间间隔阈值ms，
#define Jitter_Offset         10    // 修正触摸点抖动的位移阈值



#define TP_HEADER_SIZE    46
#define TP_FINGER_SIZE    30

//// 46 length 
struct tp_protocol
{
	u8                  type;      // unknown type  =2
	u8                  clicked;   // 按住了触摸板， 不管几个按住，都是 1
	u8                  unknown1[5]; //
	u8                  is_finger;   // 触摸板有手指 1，当离开瞬间，出现 0
	u8                  unknown2[8]; // 
	u8                  unknown3[8]; // 未知，固定 00-01-07-97-02-00-06-00
	u8                  finger_data_length; // 手指数据总长度， 手指个数*30
	u8                  unknown4[5]; //
	u8                  finger_number; //手指个数
	u8                  Clicked; // 同上边的clicked
	u8                  state;   // 手指在上边好像是 0x10， 手指离开瞬间最高设置 1，变成 0x80（0x90），最后离开后，还会出现 0x00
	u8                  state2;  // 手指在上边 0x20，离开瞬间 变 0
	u8                  state3;  // 平时0， Clicked为 0x10
	u8                  zero;    // 始终 0
	u8                  unknown5[10]; /////
};


///// 30 length
struct tp_finger
{
	short             org_x; //按下后，这个数字不变，
	short             org_y; //
	short             x;     //随着手指移动改变，
	short             y;     //
	__le16            unknown[11];
};


/////////////触摸板数据分析并且模拟鼠标

struct mouse_event_t
{
	unsigned char     button; /// 0 没按 ， 1 左键按住， 2 右键按住。
    short             dx;
	short             dy;
	char              v_wheel; //垂直滚轮
	char              h_wheel; //水平滚轮
};

typedef void(*MOUSEEVENTCALLBACK)(mouse_event_t* evt, void* param);

void MouseLikeTouchPad_init(MOUSEEVENTCALLBACK cbk, void* param);

void MouseLikeTouchPad_parse(u8* data, LONG length);

void MouseLikeTouchPad_parse_init();


