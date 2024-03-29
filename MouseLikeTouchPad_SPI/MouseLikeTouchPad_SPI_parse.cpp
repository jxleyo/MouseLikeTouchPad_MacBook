#include "MouseLikeTouchPad_SPI.h"
#include "apple_tp_proto.h"
#include<math.h>
extern "C" int _fltused = 0;


#define MAXFINGER_CNT 10

#define STABLE_INTERVAL_FingerSeparated_MSEC   20   // 手指分开按到触摸板的稳定时间间隔
#define STABLE_INTERVAL_FingerClosed_MSEC      500   // 手指并拢按到触摸板的稳定时间间隔 //原始值为100，因为macbook触控板有并拢抖动偏移问题所以设高一点

#define ButtonPointer_Interval_MSEC      150   // 鼠标左中右键与指针操作间隔时间ms，

#define Jitter_Offset         10    // 修正触摸点轻微抖动的位移阈值


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
static float scales[3];//手指大小采样
static UINT8 sampcount;//手指大小采样计数


static __forceinline short abs(short x)
{
	if (x < 0)return -x;
	return x;
}

void MouseLikeTouchPad_SPI_parse_init(DEV_EXT* pDevContext)
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

		Mouse_Wheel_mode = FALSE;
		pDevContext->bMouse_Wheel_Mode_JudgeEnable = TRUE;//开启滚轮判别
		pDevContext->bGestureCompleted = FALSE; //手势操作结束标志
		pDevContext->bPtpReportCollection = FALSE;//默认鼠标集合

		lastfinger_count = 0;
		currentfinger_count = 0;
		for (UINT8 i = 0; i < MAXFINGER_CNT; ++i) {
			lastfinger[i].X = 0;
			lastfinger[i].Y = 0;
			currentfinger[i].X = 0;
			currentfinger[i].Y = 0;
		}


		pDevContext->tick_Count = KeQueryTimeIncrement();
		pDevContext->runtimes = 0;

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
		PointerSensitivity = TouchPad_DPI / 25;
		sampcount = 0;//手指大小采样计数

}

void MouseLikeTouchPad_SPI_parse(DEV_EXT* pDevContext, UINT8* data, LONG length)
{
	NTSTATUS status = STATUS_SUCCESS;

	PtpReport = (struct SPI_TRACKPAD_PACKET*)data;
	if (length < TP_HEADER_SIZE || length < TP_HEADER_SIZE + TP_FINGER_SIZE * PtpReport->NumOfFingers || PtpReport->NumOfFingers >= MAXFINGER_CNT) return; //

	//计算报告频率和时间间隔
	KeQueryTickCount(&current_ticktime);
	LONGLONG CounterDelta = (current_ticktime.QuadPart - last_ticktime.QuadPart) * pDevContext->tick_Count / 100;//单位100US秒
	ticktime_Interval.QuadPart = (current_ticktime.QuadPart - last_ticktime.QuadPart) * pDevContext->tick_Count / 10000;//单位ms毫秒
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


	//初始化ptp事件
	PTP_REPORT ptp_Report;
	RtlZeroMemory(&ptp_Report, sizeof(PTP_REPORT));

	//初始化鼠标事件
	mouse_report_t mReport;
	mReport.report_id = FAKE_REPORTID_MOUSE;
	mReport.button = 0;
	mReport.dx = 0;
	mReport.dy = 0;
	mReport.h_wheel = 0;
	mReport.v_wheel = 0;
	Mouse_LButton_Status = 0; //定义临时鼠标左键状态，0为释放，1为按下，每次都需要重置确保后面逻辑
	Mouse_MButton_Status = 0; //定义临时鼠标中键状态，0为释放，1为按下，每次都需要重置确保后面逻辑
	Mouse_RButton_Status = 0; //定义临时鼠标右键状态，0为释放，1为按下，每次都需要重置确保后面逻辑


	//初始化当前触摸点索引号，跟踪后未再赋值的表示不存在了
	Mouse_Pointer_CurrentIndexID = -1;
	Mouse_LButton_CurrentIndexID = -1;
	Mouse_RButton_CurrentIndexID = -1;
	Mouse_MButton_CurrentIndexID = -1;
	Mouse_Wheel_CurrentIndexID = -1;
	
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

	//开始鼠标事件逻辑判定
	//注意多手指非同时快速接触触摸板时触摸板报告可能存在一帧中同时新增多个触摸点的情况所以不能用当前只有一个触摸点作为定义指针的判断条件
	if (Mouse_Pointer_LastIndexID == -1 && currentfinger_count > 0) {//鼠标指针、左键、右键、中键都未定义,
		//指针触摸点压力、接触面长宽比阈值特征区分判定手掌打字误触和正常操作,压力越小接触面长宽比阈值越大、长度阈值越小
		BOOLEAN FakePointer = TRUE;
		short press = currentfinger[0].Pressure;
		float len = currentfinger[0].ToolMajor/ thumb_scale;
		double ratio;
		if (currentfinger[0].ToolMinor != 0) {
			ratio = (currentfinger[0].ToolMajor / currentfinger[0].ToolMinor) / thumb_scale;
		}
		else {
			ratio = 2;
		}
		if (press < 4) {
			if (ratio < 1.7 && len < 800) {
				FakePointer = FALSE;
			}
		}
		else if (press < 8) {
			if (ratio < 1.6 && len < 900) {
				FakePointer = FALSE;
			}
		}
		else if (press >= 8 && press <12) {
			if (ratio < 1.5 && len < 1000) {
				FakePointer = FALSE;
			}
		}
		else if (press >= 12 && press < 16) {
			if (ratio < 1.45 && len < 1200) {
				FakePointer = FALSE;
			}
		}
		else if (press >= 16) {
			if (ratio < 1.4 && len < 1350) {
				FakePointer = FALSE;
			}
		}
		
		if (!FakePointer) {
			Mouse_Pointer_CurrentIndexID = 0;  //首个触摸点作为指针
			MousePointer_DefineTime = current_ticktime;//定义当前指针起始时间
		}
	}
	else if (Mouse_Pointer_CurrentIndexID == -1 && Mouse_Pointer_LastIndexID != -1) {//指针消失
		Mouse_Wheel_mode = FALSE;//结束滚轮模式
		pDevContext->bMouse_Wheel_Mode_JudgeEnable = TRUE;//开启滚轮判别
		pDevContext->bGestureCompleted = TRUE;//手势模式结束,但bPtpReportCollection不要重置待其他代码来处理

		Mouse_Pointer_CurrentIndexID = -1;
		Mouse_LButton_CurrentIndexID = -1;
		Mouse_RButton_CurrentIndexID = -1;
		Mouse_MButton_CurrentIndexID = -1;
		Mouse_Wheel_CurrentIndexID = -1;
	}
	else if (Mouse_Pointer_CurrentIndexID != -1 && !Mouse_Wheel_mode) {  //指针已定义的非滚轮事件处理
		//查找指针左侧或者右侧是否有并拢的手指作为滚轮模式或者按键模式，当指针左侧/右侧的手指按下时间与指针手指定义时间间隔小于设定阈值时判定为鼠标滚轮否则为鼠标按键，这一规则能有效区别按键与滚轮操作,但鼠标按键和滚轮不能一起使用
		//按键定义后会跟踪坐标所以左键和中键不能滑动食指互相切换需要抬起食指后进行改变，左键/中键/右键按下的情况下不能转变为滚轮模式，
		LARGE_INTEGER MouseButton_Interval;
		MouseButton_Interval.QuadPart = (current_ticktime.QuadPart - MousePointer_DefineTime.QuadPart) * pDevContext->tick_Count / 10000;//单位ms毫秒
		float Mouse_Button_Interval = (float)MouseButton_Interval.LowPart;//指针左右侧的手指按下时间与指针定义起始时间的间隔ms

		if (currentfinger_count > 1) {//触摸点数量超过1才需要判断按键操作
			for (char i = 0; i < currentfinger_count; i++) {
				if (i == Mouse_Pointer_CurrentIndexID || i == Mouse_LButton_CurrentIndexID || i == Mouse_RButton_CurrentIndexID || i == Mouse_MButton_CurrentIndexID || i == Mouse_Wheel_CurrentIndexID) {//i为正值所以无需检查索引号是否为-1
					continue;  // 已经定义的跳过
				}
				float dx = (float)(currentfinger[i].X - currentfinger[Mouse_Pointer_CurrentIndexID].X);
				float dy = (float)(currentfinger[i].Y - currentfinger[Mouse_Pointer_CurrentIndexID].Y);
				float distance = sqrt(dx * dx + dy * dy);//触摸点与指针的距离

				// 指针左右侧有手指按下并且与指针手指起始定义时间间隔小于阈值，指针被定义后区分滚轮操作只需判断一次直到指针消失，后续按键操作判断不会被时间阈值约束使得响应速度不受影响
				BOOLEAN isWheel = pDevContext->bMouse_Wheel_Mode_JudgeEnable && abs(distance) > FingerMinDistance && abs(distance) < FingerMaxDistance && Mouse_Button_Interval < ButtonPointer_Interval_MSEC;
				if (isWheel) {//滚轮模式条件成立
					Mouse_Wheel_mode = TRUE;  //开启滚轮模式
					pDevContext->bMouse_Wheel_Mode_JudgeEnable = FALSE;//关闭滚轮判别
					pDevContext->bGestureCompleted = FALSE; //手势操作结束标志,但bPtpReportCollection不要重置待其他代码来处理

					Mouse_Wheel_CurrentIndexID = i;//滚轮辅助参考手指索引值

					Mouse_LButton_CurrentIndexID = -1;
					Mouse_RButton_CurrentIndexID = -1;
					Mouse_MButton_CurrentIndexID = -1;
					break;
				}
				else {//前面滚轮模式条件判断已经排除了所以不需要考虑与指针手指起始定义时间间隔，
					if (Mouse_MButton_CurrentIndexID == -1 && abs(distance) > FingerMinDistance && abs(distance) < FingerClosedThresholdDistance && dx < 0) {//指针左侧有并拢的手指按下
						Mouse_MButton_Status = 1; //找到中键
						Mouse_MButton_CurrentIndexID = i;//赋值中键触摸点新索引号
						continue;  //继续找其他按键，食指已经被中键占用所以原则上左键已经不可用
					}
					else if (Mouse_LButton_CurrentIndexID == -1 && abs(distance) > FingerClosedThresholdDistance && abs(distance) < FingerMaxDistance && dx < 0) {//指针左侧有分开的手指按下
						Mouse_LButton_Status = 1; //找到左键
						Mouse_LButton_CurrentIndexID = i;//赋值左键触摸点新索引号
						continue;  //继续找其他按键
					}
					else if (Mouse_RButton_CurrentIndexID == -1 && abs(distance) > FingerMinDistance && abs(distance) < FingerMaxDistance && dx > 0) {//指针右侧有手指按下
						Mouse_RButton_Status = 1; //找到右键
						Mouse_RButton_CurrentIndexID = i;//赋值右键触摸点新索引号
						continue;  //继续找其他按键
					}
				}
			}
		}

		//鼠标指针位移设置
		if (currentfinger_count != lastfinger_count) {//手指变化瞬间时电容可能不稳定指针坐标突发性漂移需要忽略
			JitterFixStartTime = current_ticktime;//抖动修正开始计时
		}
		else {
			LARGE_INTEGER FixTimer;
			FixTimer.QuadPart = (current_ticktime.QuadPart - JitterFixStartTime.QuadPart) * pDevContext->tick_Count / 10000;//单位ms毫秒
			float JitterFixTimer = (float)FixTimer.LowPart;//当前抖动时间计时

			float STABLE_INTERVAL;
			if (Mouse_MButton_CurrentIndexID != -1) {//中键状态下手指并拢的抖动修正值区别处理
				STABLE_INTERVAL = STABLE_INTERVAL_FingerClosed_MSEC;
			}
			else {
				STABLE_INTERVAL = STABLE_INTERVAL_FingerSeparated_MSEC;
			}
			if (JitterFixTimer > STABLE_INTERVAL) {//触摸点稳定时间后
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


				mReport.dx = (char)(px / PointerSensitivity);
				mReport.dy = -(char)(py / PointerSensitivity);
				if (abs(px) <= Jitter_Offset/2) {//指针慢速精细移动时修正
					mReport.dx = (char)(px * 2 / PointerSensitivity);
				}
				if (abs(py) <= Jitter_Offset/2) {//指针慢速精细移动时修正
					mReport.dy = -(char)(py * 2 / PointerSensitivity);
				}

				//thumb_scale手指大小采样以适配不同的使用者,3次采样取平均值,并限制最大最小值
				if (sampcount < 3) {
					if (currentfinger[Mouse_Pointer_CurrentIndexID].Pressure > 20 && currentfinger[Mouse_Pointer_CurrentIndexID].Pressure < 25) {
						if (currentfinger[Mouse_Pointer_CurrentIndexID].ToolMinor > 600 || currentfinger[Mouse_Pointer_CurrentIndexID].ToolMinor < 850) {
							scales[sampcount] = (float)currentfinger[Mouse_Pointer_CurrentIndexID].ToolMinor / 750;
							sampcount++;
						}
					}
					else if (currentfinger[Mouse_Pointer_CurrentIndexID].Pressure > 25 && currentfinger[Mouse_Pointer_CurrentIndexID].Pressure < 75) {
						if (currentfinger[Mouse_Pointer_CurrentIndexID].ToolMinor > 600 || currentfinger[Mouse_Pointer_CurrentIndexID].ToolMinor < 850) {
							scales[sampcount] = (float)currentfinger[Mouse_Pointer_CurrentIndexID].ToolMinor / 800;
							sampcount++;
						}
					}
				}
				else if (sampcount == 3) {
					thumb_scale = (scales[0] + scales[1] + scales[2]) / 3;
					sampcount = 0;
				}
				
			}
		}
	}
	else if (Mouse_Pointer_CurrentIndexID != -1 && Mouse_Wheel_mode) {//滚轮操作模式
		RegDebug(L"Mouse_Wheel_mode on", NULL, 0x12345678);
		pDevContext->bPtpReportCollection = TRUE;//确认触控板报告模式,后续再做进一步判断
	}
	else {
		//其他组合无效
	}
	
	ptp_event_t pEvt;
	RtlZeroMemory(&pEvt, sizeof(ptp_event_t));

	if (pDevContext->bPtpReportCollection) {//触摸板集合，手势模式判断
		//构造ptp报告
		ptp_Report.ReportID = FAKE_REPORTID_MULTITOUCH;//测试实际值
		ptp_Report.ContactCount = PtpReport->NumOfFingers;
		RegDebug(L"ptp_Report.ContactCount=", NULL, ptp_Report.ContactCount);
		UINT8 AdjustedCount = (PtpReport->NumOfFingers > 5) ? 5 : PtpReport->NumOfFingers;
		for (int i = 0; i < AdjustedCount; i++) {
			ptp_Report.Contacts[i].Confidence = 1;
			ptp_Report.Contacts[i].TipSwitch = (currentfinger[i].Pressure > 0) ? 1 : 0; //1;
			ptp_Report.Contacts[i].ContactID = i;// ContactID;

			LONG x = currentfinger[i].X - pDevContext->TrackpadInfo.XMin;
			if (x < 0) { x = 0; }

			LONG y = pDevContext->TrackpadInfo.YMax - currentfinger[i].Y;
			if (y < 0) { y = 0; }

			ptp_Report.Contacts[i].X = (USHORT)(x / pDevContext->LOGICAL_MAXIMUM_scale);
			ptp_Report.Contacts[i].Y = (USHORT)(y / pDevContext->LOGICAL_MAXIMUM_scale);

			if (i == 0) {
				RegDebug(L"ptp_Report.Contacts[0].Confidence=", NULL, ptp_Report.Contacts[i].Confidence);
				RegDebug(L"ptp_Report.Contacts[0].TipSwitch=", NULL, ptp_Report.Contacts[i].TipSwitch);
				RegDebug(L"ptp_Report.Contacts[0].X=", NULL, ptp_Report.Contacts[i].X);
				RegDebug(L"ptp_Report.Contacts[0].Y=", NULL, ptp_Report.Contacts[i].Y);
			}
			if (i == 1) {
				RegDebug(L"ptp_Report.Contacts[1].Confidence=", NULL, ptp_Report.Contacts[i].Confidence);
				RegDebug(L"ptp_Report.Contacts[1].TipSwitch=", NULL, ptp_Report.Contacts[i].TipSwitch);
				RegDebug(L"ptp_Report.Contacts[1].X=", NULL, ptp_Report.Contacts[i].X);
				RegDebug(L"ptp_Report.Contacts[1].Y=", NULL, ptp_Report.Contacts[i].Y);
			}
			if (i == 2) {
				RegDebug(L"ptp_Report.Contacts[2].Confidence=", NULL, ptp_Report.Contacts[i].Confidence);
				RegDebug(L"ptp_Report.Contacts[2].TipSwitch=", NULL, ptp_Report.Contacts[i].TipSwitch);
				RegDebug(L"ptp_Report.Contacts[2].X=", NULL, ptp_Report.Contacts[i].X);
				RegDebug(L"ptp_Report.Contacts[2].Y=", NULL, ptp_Report.Contacts[i].Y);
			}
		}

		if (CounterDelta < 0xF)
		{
			ptp_Report.ScanTime = 0xF;
		}
		else if (CounterDelta >= 0x64)
		{
			ptp_Report.ScanTime = 0x64;//此数值关系到滚轮和手势的速度，约小约灵敏快速
		}
		else {
			ptp_Report.ScanTime = (USHORT)CounterDelta;
		}
		RegDebug(L"ptp_Report.ScanTime=", NULL, ptp_Report.ScanTime);


		if (!Mouse_Wheel_mode) {//以指针手指释放为滚轮模式结束标志，下一帧bPtpReportCollection会设置FALSE所以只会发送一次构造的手势结束报告
			pDevContext->bPtpReportCollection = FALSE;//PTP触摸板集合报告模式结束
			pDevContext->bGestureCompleted = TRUE;//结束手势操作，该数据和bMouse_Wheel_Mode区分开了，因为bGestureCompleted可能会比bMouse_Wheel_Mode提前结束
			RegDebug(L"MouseLikeTouchPad_SPI_parse bPtpReportCollection bGestureCompleted0", NULL, status);

			//构造全部手指释放的临时数据包,TipSwitch域归零，windows手势操作结束时需要手指离开的点xy坐标数据
			PTP_REPORT CompletedGestureReport;
			RtlCopyMemory(&CompletedGestureReport, &ptp_Report, sizeof(PTP_REPORT));
			for (int i = 0; i < currentfinger_count; i++) {
				CompletedGestureReport.Contacts[i].TipSwitch = 0;
			}

			//发送ptp报告
			pEvt.collectionType = PTP_CollectionType;//
			pEvt.ptpReport = CompletedGestureReport;
			RegDebug(L"pEvt.ptpReport=", &pEvt.ptpReport, sizeof(PTP_REPORT));
			//发送触控板事件
			mltp_Event(pDevContext, &pEvt);

		}
		else if (Mouse_Wheel_mode && currentfinger_count == 1 && !pDevContext->bGestureCompleted) {//滚轮模式未结束并且剩下指针手指留在触摸板上,需要配合bGestureCompleted标志判断使得构造的手势结束报告只发送一次
			pDevContext->bPtpReportCollection = FALSE;//PTP触摸板集合报告模式结束
			pDevContext->bGestureCompleted = TRUE;//提前结束手势操作，该数据和bMouse_Wheel_Mode区分开了，因为bGestureCompleted可能会比bMouse_Wheel_Mode提前结束
			RegDebug(L"MouseLikeTouchPad_SPI_parse bPtpReportCollection bGestureCompleted1", NULL, status);

			//构造指针手指释放的临时数据包,TipSwitch域归零，windows手势操作结束时需要手指离开的点xy坐标数据
			PTP_REPORT CompletedGestureReport2;
			RtlCopyMemory(&CompletedGestureReport2, &ptp_Report, sizeof(PTP_REPORT));
			CompletedGestureReport2.Contacts[0].TipSwitch = 0;

			//发送ptp报告
			pEvt.collectionType = PTP_CollectionType;//
			pEvt.ptpReport = CompletedGestureReport2;
			RegDebug(L"pEvt.ptpReport=", &pEvt.ptpReport, sizeof(PTP_REPORT));
			//发送触控板事件
			mltp_Event(pDevContext, &pEvt);
		}

		if (!pDevContext->bGestureCompleted) {//手势未结束，正常发送报告
			RegDebug(L"MouseLikeTouchPad_SPI_parse bPtpReportCollection bGestureCompleted2", NULL, status);
			//发送ptp报告
			pEvt.collectionType = PTP_CollectionType;//
			pEvt.ptpReport = ptp_Report;
			RegDebug(L"pEvt.ptpReport=", &pEvt.ptpReport, sizeof(PTP_REPORT));
			//发送触控板事件
			mltp_Event(pDevContext, &pEvt);
		}
	}
	else {//发送MouseCollection
		mReport.button = Mouse_LButton_Status + (Mouse_RButton_Status << 1) + (Mouse_MButton_Status << 2);  //左中右键状态合成

		pEvt.collectionType = MOUSE_CollectionType;//
		pEvt.mReport = mReport;
		//发送鼠标事件
		mltp_Event(pDevContext, &pEvt);
	}
	
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


