#include "MouseLikeTouchPad.h"
#include "apple_tp_proto.h"
#include<math.h>
extern "C" int _fltused = 0;


#define MAXFINGER_CNT 10

#define STABLE_INTERVAL_FingerSeparated_MSEC   20   // 手指分开按到触摸板的稳定时间间隔
#define STABLE_INTERVAL_FingerClosed_MSEC      100   // 手指并拢按到触摸板的稳定时间间隔 

#define MouseReport_INTERVAL_MSEC         8   // 鼠标报告间隔时间ms，以频率125hz为基准
#define ButtonPointer_Interval_MSEC      200   // 鼠标左中右键与指针操作间隔时间ms，

#define Jitter_Offset         10    // 修正触摸点轻微抖动的位移阈值


struct MouseLikeTouchPad_state
{
	MOUSEEVENTCALLBACK evt_cbk;
	void* evt_param;
	ULONG tick_count;
};


struct FingerPosition
{
	short  pos_x;
	short pos_y;
};

static struct SPI_TRACKPAD_PACKET* PtpReport;//定义触摸板报告数据指针
static struct SPI_TRACKPAD_FINGER* pFinger_data;//定义触摸板报告手指触摸点数据指针

//保存追踪的手指
static struct SPI_TRACKPAD_FINGER lastfinger[MAXFINGER_CNT];
static struct SPI_TRACKPAD_FINGER currentfinger[MAXFINGER_CNT];
static BYTE currentfinger_count; //定义当前触摸点数量
static BYTE lastfinger_count; //定义上次触摸点数量

static char Mouse_Pointer_CurrentIndexID; //定义当前鼠标指针触摸点坐标的数据索引号，-1为未定义
static char Mouse_LButton_CurrentIndexID; //定义当前鼠标左键触摸点坐标的数据索引号，-1为未定义
static char Mouse_RButton_CurrentIndexID; //定义当前鼠标右键触摸点坐标的数据索引号，-1为未定义
static char Mouse_MButton_CurrentIndexID; //定义当前鼠标中键触摸点坐标的数据索引号，-1为未定义
static char Mouse_Wheel_CurrentIndexID; //定义当前鼠标滚轮辅助参考手指触摸点坐标的数据索引号，-1为未定义

static char Mouse_Pointer_LastIndexID; //定义上次鼠标指针触摸点坐标的数据索引号，-1为未定义
static char Mouse_LButton_LastIndexID; //定义上次鼠标左键触摸点坐标的数据索引号，-1为未定义
static char Mouse_RButton_LastIndexID; //定义上次鼠标右键触摸点坐标的数据索引号，-1为未定义
static char Mouse_MButton_LastIndexID; //定义上次鼠标中键触摸点坐标的数据索引号，-1为未定义
static char Mouse_Wheel_LastIndexID; //定义上次鼠标滚轮辅助参考手指触摸点坐标的数据索引号，-1为未定义

static BOOLEAN Mouse_LButton_Status; //定义临时鼠标左键状态，0为释放，1为按下
static BOOLEAN Mouse_MButton_Status; //定义临时鼠标中键状态，0为释放，1为按下
static BOOLEAN Mouse_RButton_Status; //定义临时鼠标右键状态，0为释放，1为按下

static BOOLEAN Mouse_Wheel_mode; //定义鼠标滚轮状态，0为滚轮未激活，1为滚轮激活

static LARGE_INTEGER MousePointer_DefineTime;//鼠标指针定义时间，用于计算按键间隔时间来区分判定鼠标中间和滚轮操作
static float TouchPad_ReportInterval;//定义触摸板报告间隔时间

static LARGE_INTEGER JitterFixStartTime; // 修正触摸点抖动修正时间计时器

static BYTE Scroll_IntervalCount; //定义鼠标滚动间隔计数

static LARGE_INTEGER last_ticktime; //上次报告计时
static LARGE_INTEGER current_ticktime;//当前报告计时
static LARGE_INTEGER ticktime_Interval;//报告间隔时间


//定义手指头尺寸大小
static float thumb_width;//手指头宽度
static float thumb_height;//手指头高度
static float thumb_scale;//手指头尺寸缩放比例
static float FingerTracingMaxOffset;//定义追踪单次采样间隔时允许的手指最大位移量
static float FingerMinDistance;//定义有效的相邻手指最小距离(和FingerTracingMaxOffset无直接关系)
static float FingerClosedThresholdDistance;//定义相邻手指合拢时的最小距离(和FingerTracingMaxOffset无直接关系)
static float FingerMaxDistance;//定义有效的相邻手指最大距离
static float TouchPad_DPI;//定义触摸板分辨率
static float PointerSensitivity;//定义指针灵敏度即指针点移动量缩放比例

//定义相当于全局变量的tp
static MouseLikeTouchPad_state tp_state;
#define tp  (&tp_state)

void MouseLikeTouchPad_init(MOUSEEVENTCALLBACK cbk, void* param)
{
	RtlZeroMemory(tp, sizeof(MouseLikeTouchPad_state));

	tp->tick_count = KeQueryTimeIncrement(); ///
	tp->evt_cbk = cbk;
	tp->evt_param = param;
}

static __forceinline short abs(short x)
{
	if (x < 0)return -x;
	return x;
}

void MouseLikeTouchPad_parse_init()
{
		Mouse_Pointer_CurrentIndexID = -1; //定义当前鼠标指针触摸点坐标的数据索引号，-1为未定义
		Mouse_LButton_CurrentIndexID = -1; //定义当前鼠标左键触摸点坐标的数据索引号，-1为未定义
		Mouse_RButton_CurrentIndexID = -1; //定义当前鼠标右键触摸点坐标的数据索引号，-1为未定义
		Mouse_MButton_CurrentIndexID = -1; //定义当前鼠标中键触摸点坐标的数据索引号，-1为未定义
		Mouse_Wheel_CurrentIndexID = -1; //定义当前鼠标滚轮辅助参考手指触摸点坐标的数据索引号，-1为未定义
		
		Mouse_Pointer_LastIndexID = -1; //定义上次鼠标指针触摸点坐标的数据索引号，-1为未定义
		Mouse_LButton_LastIndexID = -1; //定义上次鼠标左键触摸点坐标的数据索引号，-1为未定义
		Mouse_RButton_LastIndexID = -1; //定义上次鼠标右键触摸点坐标的数据索引号，-1为未定义
		Mouse_MButton_LastIndexID = -1; //定义上次鼠标中键触摸点坐标的数据索引号，-1为未定义
		Mouse_Wheel_LastIndexID = -1; //定义上次鼠标滚轮辅助参考手指触摸点坐标的数据索引号，-1为未定义

		Mouse_LButton_Status = 0; //定义临时鼠标左键状态，0为释放，1为按下
		Mouse_MButton_Status = 0;//定义临时鼠标中键状态，0为释放，1为按下
		Mouse_RButton_Status = 0; //定义临时鼠标右键状态，0为释放，1为按下

		Mouse_Wheel_mode = 0;

		lastfinger_count = 0;
		currentfinger_count = 0;
		for (UINT8 i = 0; i < MAXFINGER_CNT; ++i) {
			lastfinger[i].X = 0;
			lastfinger[i].Y = 0;
			currentfinger[i].X = 0;
			currentfinger[i].Y = 0;
		}

		Scroll_IntervalCount = 0;

		KeQueryTickCount(&last_ticktime);

		//获取触摸板分辨率
		TouchPad_DPI = 100; //以水平100点/mm为基准
		//动态调整手指头大小常量
		thumb_width = 18;//手指头宽度,默认以中指18mm宽为基准
		thumb_scale = 1.0;//手指头尺寸缩放比例，
		FingerTracingMaxOffset = 6 * TouchPad_DPI * thumb_scale;//定义追踪单次采样间隔时允许的手指最大位移量像素
		FingerMinDistance = 12 * TouchPad_DPI * thumb_scale;//定义有效的相邻手指最小距离(和FingerTracingMaxOffset无直接关系)
		FingerClosedThresholdDistance = 18 * TouchPad_DPI * thumb_scale;//定义相邻手指合拢时的最小距离(和FingerTracingMaxOffset无直接关系)
		FingerMaxDistance = FingerMinDistance * 4;//定义有效的相邻手指最大距离(FingerMinDistance*4)  
		PointerSensitivity = TouchPad_DPI / 20;
}

void MouseLikeTouchPad_parse(UINT8* data, LONG length)
{
	PtpReport = (struct SPI_TRACKPAD_PACKET*)data;
	if (length < TP_HEADER_SIZE || length < TP_HEADER_SIZE + TP_FINGER_SIZE * PtpReport->NumOfFingers || PtpReport->NumOfFingers >= MAXFINGER_CNT) return; //

	//计算报告频率和时间间隔
	KeQueryTickCount(&current_ticktime);
	ticktime_Interval.QuadPart = (current_ticktime.QuadPart - last_ticktime.QuadPart) * tp->tick_count / 10000;//单位ms毫秒
	TouchPad_ReportInterval = (float)ticktime_Interval.LowPart;//触摸板报告间隔时间ms
	last_ticktime = current_ticktime;

	//保存当前手指坐标
	if (!PtpReport->IsFinger) {//is_finger参数判断手指全部离开，不能用pr->state & 0x80)、pr->state 判断因为多点触摸时任意一个手指离开都会产生该信号，也不能用pr->finger_number判断因为该值不会为0
		currentfinger_count = 0;
	}
	else {
		currentfinger_count = PtpReport->NumOfFingers;
		if (currentfinger_count > 0) {
			for (char i = 0; i < currentfinger_count; i++) {
				pFinger_data = (struct SPI_TRACKPAD_FINGER*)(data + TP_HEADER_SIZE + i * TP_FINGER_SIZE);//
				currentfinger[i]= *pFinger_data;
				//currentfinger[i].X = pFinger_data->X;
				//currentfinger[i].Y = pFinger_data->Y;
			}
		}
	}

	//初始化鼠标事件
	mouse_event_t mEvt;
	mEvt.button = 0;
	mEvt.dx = 0;
	mEvt.dy = 0;
	mEvt.h_wheel = 0;
	mEvt.v_wheel = 0;
	Mouse_LButton_Status = 0; //定义临时鼠标左键状态，0为释放，1为按下，每次都需要重置确保后面逻辑
	Mouse_MButton_Status = 0; //定义临时鼠标中键状态，0为释放，1为按下，每次都需要重置确保后面逻辑
	Mouse_RButton_Status = 0; //定义临时鼠标右键状态，0为释放，1为按下，每次都需要重置确保后面逻辑


	//初始化当前触摸点索引号，跟踪后未再赋值的表示不存在了
	Mouse_Pointer_CurrentIndexID = -1;
	Mouse_LButton_CurrentIndexID = -1;
	Mouse_RButton_CurrentIndexID = -1;
	Mouse_MButton_CurrentIndexID = -1;
	Mouse_Wheel_CurrentIndexID = -1;

	//指针特殊跟踪
	if (currentfinger_count == 1 && Mouse_Pointer_LastIndexID != -1) {//只有一个触摸点时默认为指针
		Mouse_Pointer_CurrentIndexID = 0;//找到指针
	}
	
	//所有手指触摸点跟踪
	for (char i = 0; i < currentfinger_count; i++) {
		if (i == Mouse_Pointer_CurrentIndexID || i == Mouse_LButton_CurrentIndexID || i == Mouse_RButton_CurrentIndexID || i == Mouse_MButton_CurrentIndexID || i == Mouse_Wheel_CurrentIndexID) {//i为正值所以无需检查索引号是否为-1
				continue;// 已经定义的跳过
		}

		if (Mouse_Pointer_LastIndexID != -1) {
			short dx = currentfinger[i].X - lastfinger[Mouse_Pointer_LastIndexID].X;
			short dy = currentfinger[i].Y - lastfinger[Mouse_Pointer_LastIndexID].Y;

			if ((abs(dx) < FingerTracingMaxOffset) && (abs(dy) < FingerTracingMaxOffset)) {
				Mouse_Pointer_CurrentIndexID = i;//找到指针
				continue;//查找其他功能
			}
		}
		if (Mouse_Wheel_LastIndexID != -1) {
			short dx = currentfinger[i].X - lastfinger[Mouse_Wheel_LastIndexID].X;
			short dy = currentfinger[i].Y - lastfinger[Mouse_Wheel_LastIndexID].Y;

			if ((abs(dx) < FingerTracingMaxOffset) && (abs(dy) < FingerTracingMaxOffset)) {
				Mouse_Wheel_CurrentIndexID = i;//找到滚轮辅助键
				continue;//查找其他功能
			}
		}
		if (Mouse_LButton_LastIndexID != -1) {
			short dx = currentfinger[i].X - lastfinger[Mouse_LButton_LastIndexID].X;
			short dy = currentfinger[i].Y - lastfinger[Mouse_LButton_LastIndexID].Y;

			if ((abs(dx) < FingerTracingMaxOffset) && (abs(dy) < FingerTracingMaxOffset)) {
				Mouse_LButton_Status = 1; //找到左键，
				Mouse_LButton_CurrentIndexID = i;//赋值左键触摸点新索引号
				continue;//查找其他功能
			}
		}

		if (Mouse_RButton_LastIndexID != -1) {
			short dx = currentfinger[i].X - lastfinger[Mouse_RButton_LastIndexID].X;
			short dy = currentfinger[i].Y - lastfinger[Mouse_RButton_LastIndexID].Y;

			if ((abs(dx) < FingerTracingMaxOffset) && (abs(dy) < FingerTracingMaxOffset)) {
				Mouse_RButton_Status = 1; //找到右键，
				Mouse_RButton_CurrentIndexID = i;//赋值右键触摸点新索引号
				continue;//查找其他功能
			}
		}
		if (Mouse_MButton_LastIndexID != -1) {
			short dx = currentfinger[i].X - lastfinger[Mouse_MButton_LastIndexID].X;
			short dy = currentfinger[i].Y - lastfinger[Mouse_MButton_LastIndexID].Y;

			if ((abs(dx) < FingerTracingMaxOffset) && (abs(dy) < FingerTracingMaxOffset)) {
				Mouse_MButton_Status = 1; //找到中键，
				Mouse_MButton_CurrentIndexID = i;//赋值中键触摸点新索引号
				continue;//查找其他功能
			}
		}
		
	}

	//开始鼠标事件逻辑判定，注意多手指非同时快速接触触摸板时触摸板报告可能存在一帧中同时新增多个触摸点的情况所以不能用当前只有一个触摸点作为定义指针的判断条件，并且也不能用Mouse_Pointer_LastIndexID==-1作为新指针定义条件避免有其他剩余手指存在后误判
	if (lastfinger_count == 0 && currentfinger_count > 0) {//鼠标指针、左键、右键、中键都未定义,
		Mouse_Pointer_CurrentIndexID = 0;  //首个触摸点作为指针
		MousePointer_DefineTime = current_ticktime;//定义当前指针起始时间
	}
	else if (Mouse_Pointer_CurrentIndexID == -1 && Mouse_Pointer_LastIndexID != -1) {//指针消失
		Mouse_Wheel_mode = FALSE;//结束滚轮模式

		Mouse_Pointer_CurrentIndexID = -1;
		Mouse_LButton_CurrentIndexID = -1;
		Mouse_RButton_CurrentIndexID = -1;
		Mouse_MButton_CurrentIndexID = -1;
		Mouse_Wheel_CurrentIndexID = -1;
	}
	//指针已定义的非滚轮事件处理  //指针触摸点压力、接触面长宽比阈值特征区分判定手掌打字误触和正常操作
	else if (Mouse_Pointer_CurrentIndexID != -1 && !Mouse_Wheel_mode && (currentfinger[Mouse_Pointer_CurrentIndexID].Pressure > 16  && currentfinger[Mouse_Pointer_CurrentIndexID].ToolMajor / currentfinger[Mouse_Pointer_CurrentIndexID].ToolMinor<1.5)) {  
		//查找指针左侧或者右侧是否有并拢的手指作为滚轮模式或者按键模式，当指针左侧/右侧的手指按下时间与指针手指定义时间间隔小于设定阈值时判定为鼠标滚轮否则为鼠标按键，这一规则能有效区别按键与滚轮操作,但鼠标按键和滚轮不能一起使用
		//按键定义后会跟踪坐标所以左键和中键不能滑动食指互相切换需要抬起食指后进行改变，左键/中键/右键按下的情况下不能转变为滚轮模式，
		LARGE_INTEGER MouseButton_Interval;
		MouseButton_Interval.QuadPart = (current_ticktime.QuadPart - MousePointer_DefineTime.QuadPart) * tp->tick_count / 10000;//单位ms毫秒
		float Mouse_Button_Interval = (float)MouseButton_Interval.LowPart;//指针左右侧的手指按下时间与指针定义起始时间的间隔ms

		for (char i = 0; i < currentfinger_count; i++) {
			if (i == Mouse_Pointer_CurrentIndexID || i == Mouse_LButton_CurrentIndexID || i == Mouse_RButton_CurrentIndexID || i == Mouse_MButton_CurrentIndexID || i == Mouse_Wheel_CurrentIndexID) {//i为正值所以无需检查索引号是否为-1
				continue;  // 已经定义的跳过
			}
			float dx = (float)(currentfinger[i].X - currentfinger[Mouse_Pointer_CurrentIndexID].X);
			float dy = (float)(currentfinger[i].Y - currentfinger[Mouse_Pointer_CurrentIndexID].Y);
			float distance = sqrt(dx * dx + dy * dy);//触摸点与指针的距离

			
			if (abs(distance) > FingerMinDistance && abs(distance) < FingerClosedThresholdDistance && Mouse_Button_Interval < ButtonPointer_Interval_MSEC) {//指针左右侧有并拢的手指按下并且与指针手指起始定义时间间隔小于阈值
				Mouse_Wheel_mode = TRUE;  //开启滚轮模式
				Mouse_Wheel_CurrentIndexID = i;//滚轮辅助参考手指索引值

				Mouse_LButton_CurrentIndexID = -1;
				Mouse_RButton_CurrentIndexID = -1;
				Mouse_MButton_CurrentIndexID = -1;
				break;
			}
			else if (Mouse_MButton_CurrentIndexID == -1 && abs(distance) > FingerMinDistance && abs(distance) < FingerClosedThresholdDistance && Mouse_Button_Interval > ButtonPointer_Interval_MSEC && dx < 0) {//指针左侧有并拢的手指按下并且与指针手指起始定义时间间隔大于阈值
				Mouse_MButton_Status = 1; //找到中键
				Mouse_MButton_CurrentIndexID = i;//赋值中键触摸点新索引号
				continue;  //继续找其他按键，食指已经被中键占用所以原则上左键已经不可用
			}
			else if (Mouse_LButton_CurrentIndexID == -1 && abs(distance) > FingerClosedThresholdDistance && abs(distance) < FingerMaxDistance && dx < 0) {//指针左侧有手指按下，前面滚轮模式条件判断已经排除了所以不需要考虑与指针手指起始定义时间间隔，
				Mouse_LButton_Status = 1; //找到左键
				Mouse_LButton_CurrentIndexID = i;//赋值左键触摸点新索引号
				continue;  //继续找其他按键
			}
			else if (Mouse_RButton_CurrentIndexID == -1 && abs(distance) > FingerMinDistance && abs(distance) < FingerMaxDistance && dx > 0) {//指针右侧有手指按下，前面滚轮模式条件判断已经排除了所以不需要考虑与指针手指起始定义时间间隔
				Mouse_RButton_Status = 1; //找到右键
				Mouse_RButton_CurrentIndexID = i;//赋值右键触摸点新索引号
				continue;  //继续找其他按键
			}
			
		}

		//鼠标指针位移设置
		if (currentfinger_count != lastfinger_count) {//手指变化瞬间时电容可能不稳定指针坐标突发性漂移需要忽略
			JitterFixStartTime = current_ticktime;//抖动修正开始计时
		}
		else{
			LARGE_INTEGER FixTimer;
			FixTimer.QuadPart = (current_ticktime.QuadPart - JitterFixStartTime.QuadPart) * tp->tick_count / 10000;//单位ms毫秒
			float JitterFixTimer = (float)FixTimer.LowPart;//当前抖动时间计时

			float STABLE_INTERVAL;
			if (Mouse_MButton_CurrentIndexID != -1) {//中键状态下手指并拢的抖动修正值区别处理
				STABLE_INTERVAL = STABLE_INTERVAL_FingerClosed_MSEC;
			}
			else{
				STABLE_INTERVAL = STABLE_INTERVAL_FingerSeparated_MSEC;
			}
			if (JitterFixTimer> STABLE_INTERVAL) {//触摸点稳定时间后
				float px = (float)(currentfinger[Mouse_Pointer_CurrentIndexID].X - lastfinger[Mouse_Pointer_LastIndexID].X) / thumb_scale;
				float py = (float)(currentfinger[Mouse_Pointer_CurrentIndexID].Y - lastfinger[Mouse_Pointer_LastIndexID].Y) / thumb_scale;

				if (Mouse_LButton_CurrentIndexID != -1 || Mouse_RButton_CurrentIndexID != -1 || Mouse_MButton_CurrentIndexID != -1) {//有按键时修正，单指针时不需要使得指针更精确
					if (abs(px) <= Jitter_Offset) {//指针轻微抖动修正
						px = 0;
					}
					if (abs(py) <= Jitter_Offset) {//指针轻微抖动修正
						py = 0;
					}
				}

				mEvt.dx = (short)(px / PointerSensitivity);
				mEvt.dy = -(short)(py / PointerSensitivity);
			}
		}
	}
	else if (Mouse_Pointer_CurrentIndexID != -1 && Mouse_Wheel_mode) {//滚轮操作模式
		//鼠标指针位移设置
		if (currentfinger_count != lastfinger_count) {//手指变化瞬间时电容可能不稳定指针坐标突发性漂移需要忽略
			JitterFixStartTime = current_ticktime;//抖动修正开始计时
		}
		else {
			LARGE_INTEGER FixTimer;
			FixTimer.QuadPart = (current_ticktime.QuadPart - JitterFixStartTime.QuadPart) * tp->tick_count / 10000;//单位ms毫秒
			float JitterFixTimer = (float)FixTimer.LowPart;//当前抖动时间计时

			if (JitterFixTimer > STABLE_INTERVAL_FingerClosed_MSEC) {//只需在触摸点稳定时间内修正
				float px = (float)(currentfinger[Mouse_Pointer_CurrentIndexID].X - lastfinger[Mouse_Pointer_LastIndexID].X) / thumb_scale;
				float py = (float)(currentfinger[Mouse_Pointer_CurrentIndexID].Y - lastfinger[Mouse_Pointer_LastIndexID].Y) / thumb_scale;

				int direction_hscale = 1;//滚动方向缩放比例
				int direction_vscale = 1;//滚动方向缩放比例

				if (abs(px) > abs(py) / 4) {//滚动方向稳定性修正
					direction_hscale = 1;
					direction_vscale = 8;
				}
				if (abs(py) > abs(px) / 4) {//滚动方向稳定性修正
					direction_hscale = 8;
					direction_vscale = 1;
				}

				px = px / direction_hscale;
				py = py / direction_vscale;

				if (abs(px) > 4 && abs(px) < 64) {
					if (Scroll_IntervalCount == 0) {//先滚动一次再间隔计数
						mEvt.h_wheel = (char)(px > 0 ? 1 : -1);
						Scroll_IntervalCount = 16;
					}
					else {
						Scroll_IntervalCount--;
					}
				}
				else if (abs(px) >= 64) {
					mEvt.h_wheel = (char)(px > 0 ? 1 : -1);
					Scroll_IntervalCount = 0;
				}
				if (abs(py) > 4 && abs(py) < 64) {
					if (Scroll_IntervalCount == 0) {
						mEvt.v_wheel = (char)(py > 0 ? 1 : -1);
						Scroll_IntervalCount = 16;
					}
					else {
						Scroll_IntervalCount--;
					}
				}
				else if (abs(py) >= 64) {
					mEvt.v_wheel = (char)(py > 0 ? 1 : -1);
					Scroll_IntervalCount = 0;
				}
			}	
		}
	}
	else {
		//其他组合无效
	}
	
	mEvt.button = Mouse_LButton_Status + (Mouse_RButton_Status << 1) + (Mouse_MButton_Status << 2);  //左中右键状态合成
	tp->evt_cbk(&mEvt, tp->evt_param);//设置鼠标事件
	
	//保存下一轮所有触摸点的初始坐标及功能定义索引号
	for (char i = 0; i < currentfinger_count; i++) {
		lastfinger[i] = currentfinger[i];
	}

	lastfinger_count = currentfinger_count;
	Mouse_Pointer_LastIndexID = Mouse_Pointer_CurrentIndexID;
	Mouse_LButton_LastIndexID = Mouse_LButton_CurrentIndexID;
	Mouse_RButton_LastIndexID = Mouse_RButton_CurrentIndexID;
	Mouse_MButton_LastIndexID = Mouse_MButton_CurrentIndexID;
	Mouse_Wheel_LastIndexID = Mouse_Wheel_CurrentIndexID;

}

