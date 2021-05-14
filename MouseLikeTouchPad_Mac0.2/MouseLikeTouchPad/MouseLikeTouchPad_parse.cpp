#include "MouseLikeTouchPad.h"
#include "apple_tp_proto.h"
#include<math.h>
extern "C" int _fltused = 0;


#define MAXFINGER_CNT 10

#define STABLE_INTERVAL_MSEC         100   // 手指按到触摸板的稳定时间间隔 

#define MouseReport_INTERVAL_MSEC     8   // 鼠标报告间隔时间ms，以频率125hz为基准
#define MButton_Interval_MSEC         200   // 鼠标左中右键与指针操作间隔时间ms，

#define Jitter_Offset         5    // 修正触摸点轻微抖动的位移阈值
#define Exception_Offset         2    // 修正触摸点漂移异常的位移阈值

#define undefine_dot  0 //触摸点功能定义为指针
#define Mouse_Pointer_dot  1 //触摸点功能定义为指针
#define Mouse_LButton_dot  2 //触摸点功能定义为左键
#define Mouse_RButton_dot  3 //触摸点功能定义为右键
#define Mouse_MButton_dot  4 //触摸点功能定义为右键
#define Mouse_Wheel_dot  5 //触摸点功能定义为滚轮模式下指针外的手指触摸点


struct MouseLikeTouchPad_state
{
	MOUSEEVENTCALLBACK evt_cbk;
	void* evt_param;
	ULONG tick_count;
};


struct FingerPosition
{
	short          pos_x;
	short          pos_y;
	BYTE           func_Def;  //触摸点功能定义
};


//保存追踪的手指
static struct FingerPosition lastfinger[MAXFINGER_CNT];
static struct FingerPosition currentfinger[MAXFINGER_CNT];
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
static BOOLEAN Mouse_MButton_Enabled; //鼠标中键功能开启标志
static BOOLEAN Jitter;         // 修正触摸点抖动修正状态

static LARGE_INTEGER MousePointer_DefineTime;//鼠标指针定义时间，用于计算按键间隔时间来区分判定鼠标中间和滚轮操作
static float TouchPad_ReportInterval;//定义触摸板报告间隔时间

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
		for (u8 i = 0; i < MAXFINGER_CNT; ++i) {
			lastfinger[i].pos_x = 0;
			lastfinger[i].pos_y = 0;
			lastfinger[i].func_Def = 0;
			currentfinger[i].pos_x = 0;
			currentfinger[i].pos_y = 0;
			currentfinger[i].func_Def = 0;
		}

		Scroll_IntervalCount = 0;

		Mouse_MButton_Enabled = FALSE;//默认关闭中键功能
		Jitter = FALSE;

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
		PointerSensitivity = TouchPad_DPI / 40;
}

void MouseLikeTouchPad_parse(u8* data, LONG length)
{
	mouse_event_t evt;
	struct tp_protocol* pr = (struct tp_protocol*)data;

	if (length < TP_HEADER_SIZE || length < TP_HEADER_SIZE + TP_FINGER_SIZE * pr->finger_number || pr->finger_number >= MAXFINGER_CNT) return; //

	//计算报告频率和时间间隔
	KeQueryTickCount(&current_ticktime);
	ticktime_Interval.QuadPart = (current_ticktime.QuadPart - last_ticktime.QuadPart) * tp->tick_count / 10000;//单位ms毫秒
	TouchPad_ReportInterval = (float)ticktime_Interval.LowPart;//触摸板报告间隔时间ms
	last_ticktime = current_ticktime;

	//保存当前手指坐标
	if (!pr->is_finger) {//is_finger参数判断手指全部离开，不能用pr->state & 0x80)、pr->state 判断因为多点触摸时任意一个手指离开都会产生该信号，也不能用pr->finger_number判断因为该值不会为0
		currentfinger_count = 0;
	}
	else {
		currentfinger_count = pr->finger_number;
		if (currentfinger_count > 0) {
			for (char i = 0; i < currentfinger_count; i++) {
				struct tp_finger* g = (struct tp_finger*)(data + TP_HEADER_SIZE + i * TP_FINGER_SIZE); //
				currentfinger[i].pos_x = g->x;
				currentfinger[i].pos_y = g->y;
				currentfinger[i].func_Def = undefine_dot;
			}
		}
	}

	//所有手指触摸点跟踪
	//初始化当前触摸点索引号，跟踪后未再赋值的表示不存在了
	Mouse_Pointer_CurrentIndexID = -1;
	Mouse_LButton_CurrentIndexID = -1;
	Mouse_RButton_CurrentIndexID = -1;
	Mouse_MButton_CurrentIndexID = -1;
	Mouse_Wheel_CurrentIndexID = -1;


	for (char i = 0; i < lastfinger_count; i++) {
		for (char j = 0; j < currentfinger_count; j++) {
			if (currentfinger[j].func_Def) {
				continue;// 已经定义的跳过
			}
			short dx = lastfinger[i].pos_x - currentfinger[j].pos_x;
			short dy = lastfinger[i].pos_y - currentfinger[j].pos_y;
			if ((abs(dx) < FingerTracingMaxOffset) && (abs(dy) < FingerTracingMaxOffset))
			{
				currentfinger[j].func_Def = lastfinger[i].func_Def;//找到了移动的点并同步功能定义值
				//判断功能并保存索引号方便后续快速使用,上次的功能索引号不需要再赋值
				if (currentfinger[j].func_Def == Mouse_Pointer_dot) {
					Mouse_Pointer_CurrentIndexID = j;
				}
				else if (currentfinger[j].func_Def == Mouse_LButton_dot) {
					Mouse_LButton_CurrentIndexID = j;
				}
				else if (currentfinger[j].func_Def == Mouse_RButton_dot) {
					Mouse_RButton_CurrentIndexID = j;
				}
				else if (currentfinger[j].func_Def == Mouse_MButton_dot) {
					Mouse_MButton_CurrentIndexID = j;
				}
				else if (currentfinger[j].func_Def == Mouse_Wheel_dot) {
					Mouse_Wheel_CurrentIndexID = j;
				}

				break;
			}
		}
	}


	//初始化鼠标事件
	evt.button = 0;
	evt.dx = 0;
	evt.dy = 0;
	evt.h_wheel = 0;
	evt.v_wheel = 0;
	Mouse_LButton_Status = 0; //定义临时鼠标左键状态，0为释放，1为按下，每次都需要重置确保后面逻辑
	Mouse_MButton_Status = 0; //定义临时鼠标中键状态，0为释放，1为按下，每次都需要重置确保后面逻辑
	Mouse_RButton_Status = 0; //定义临时鼠标右键状态，0为释放，1为按下，每次都需要重置确保后面逻辑

	if (currentfinger_count >3 && pr->clicked) {
		Mouse_MButton_Enabled = !Mouse_MButton_Enabled;//3指以上触摸并按压时可以随时切换开启/关闭鼠标中键功能
	}

	if (Mouse_Pointer_CurrentIndexID ==-1 && Mouse_Pointer_LastIndexID==-1 && currentfinger_count ==1) {//鼠标指针、左键、右键、中键都未定义
			Mouse_Pointer_CurrentIndexID = 0;  //首个触摸点作为指针
			currentfinger[0].func_Def = Mouse_Pointer_dot;//指针触摸点定义功能
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
	else if (Mouse_Pointer_CurrentIndexID != -1 && !Mouse_Wheel_mode) {//指针已定义的非滚轮事件处理
		//查找指针左侧或者右侧是否有并拢的手指作为滚轮模式或者按键模式，当指针左侧/右侧的手指按下时间与指针手指定义时间间隔小于设定阈值时判定为鼠标滚轮否则为鼠标按键，这一规则能有效区别按键与滚轮操作,但鼠标按键和滚轮不能一起使用
		LARGE_INTEGER MouseButton_Interval;
		MouseButton_Interval.QuadPart = (current_ticktime.QuadPart - MousePointer_DefineTime.QuadPart) * tp->tick_count / 10000;//单位ms毫秒
		float Mouse_Button_Interval = (float)MouseButton_Interval.LowPart;//指针左右侧的手指按下时间与指针定义起始时间的间隔ms

		char foundIndex_LB = -1;
		char foundIndex_RB = -1;
		char foundIndex_MB = -1;
		char foundIndex_WH = -1;
		for (char i = 0; i < currentfinger_count; i++) {
			if (currentfinger[i].func_Def == Mouse_Pointer_dot || currentfinger[i].func_Def == Mouse_LButton_dot || currentfinger[i].func_Def == Mouse_RButton_dot || currentfinger[i].func_Def == Mouse_MButton_dot) {
				continue;  //左键/中键/右键按下的情况下不能转变为滚轮模式所以可以跳过，关键一点注意查找到左键中键右键的触摸点后不能马上赋值新的按键触摸点数组索引号，因为后面会优先采用已存在追踪到的按键触摸点作验证和标准
			}
			float dx = (float)(currentfinger[i].pos_x - currentfinger[Mouse_Pointer_CurrentIndexID].pos_x);
			float dy = (float)(currentfinger[i].pos_y - currentfinger[Mouse_Pointer_CurrentIndexID].pos_y);
			float distance = sqrt(dx * dx + dy * dy);//触摸点与指针的距离

			if (!Mouse_MButton_Enabled) {//鼠标中键功能关闭时
				if (abs(distance) > FingerMinDistance && abs(distance) < FingerClosedThresholdDistance && Mouse_Button_Interval < MButton_Interval_MSEC) {//指针左右侧有并拢的手指按下并且与指针手指起始定义时间间隔小于阈值
					Mouse_Wheel_mode = TRUE;  //开启滚轮模式
					Mouse_Wheel_CurrentIndexID = i;//滚轮辅助参考手指索引值
					foundIndex_WH = i;

					Mouse_LButton_CurrentIndexID = -1;
					Mouse_RButton_CurrentIndexID = -1;
					Mouse_MButton_CurrentIndexID = -1;
					break;
				}
				else if (Mouse_LButton_CurrentIndexID == -1 && abs(distance) > FingerMinDistance && abs(distance) < FingerMaxDistance && dx < (FingerMinDistance / 2)) {//指针左侧有手指按下，前面滚轮模式条件判断已经排除了所以不需要考虑与指针手指起始定义时间间隔，最后的参数修正左键有可能x轴略大于指针一点点的状况
					Mouse_LButton_Status = 1; //找到左键，
					foundIndex_LB = i;
					continue;  //继续找右键
				}
				else if (Mouse_RButton_CurrentIndexID == -1 && abs(distance) > FingerMinDistance && abs(distance) < FingerMaxDistance && dx > 0) {//指针右侧有手指按下，前面滚轮模式条件判断已经排除了所以不需要考虑与指针手指起始定义时间间隔
					Mouse_RButton_Status = 1; //找到右键
					foundIndex_RB = i;
					continue;  //继续找左键
				}
			}
			else {//鼠标中键功能开启时
				if (abs(distance) > FingerMinDistance && abs(distance) < FingerClosedThresholdDistance && Mouse_Button_Interval < MButton_Interval_MSEC) {//指针左右侧有并拢的手指按下并且与指针手指起始定义时间间隔小于阈值
					Mouse_Wheel_mode = TRUE;  //开启滚轮模式
					Mouse_Wheel_CurrentIndexID = i;//滚轮辅助参考手指索引值
					foundIndex_WH = i;

					Mouse_LButton_CurrentIndexID = -1;
					Mouse_RButton_CurrentIndexID = -1;
					Mouse_MButton_CurrentIndexID = -1;
					break;
				}
				else if (Mouse_MButton_CurrentIndexID == -1 && abs(distance) > FingerMinDistance && abs(distance) < FingerClosedThresholdDistance && Mouse_Button_Interval > MButton_Interval_MSEC && dx < 0) {//指针左侧有并拢的手指按下并且与指针手指起始定义时间间隔大于阈值
					foundIndex_MB = i;
					Mouse_MButton_Status = 1; //找到中键
					continue;  //继续找右键，食指已经被中键占用所以原则上左键已经不可用
				}
				else if (Mouse_LButton_CurrentIndexID == -1 && abs(distance) > FingerMinDistance && abs(distance) < FingerMaxDistance && dx < (FingerMinDistance / 2)) {//指针左侧有手指按下，前面滚轮模式条件判断已经排除了所以不需要考虑与指针手指起始定义时间间隔，最后的参数修正左键有可能x轴略大于指针一点点的状况，
					Mouse_LButton_Status = 1; //找到左键
					foundIndex_LB = i;
					continue;  //继续找右键
				}
				else if (Mouse_RButton_CurrentIndexID == -1 && abs(distance) > FingerMinDistance && abs(distance) < FingerMaxDistance && dx > 0) {//指针右侧有手指按下，前面滚轮模式条件判断已经排除了所以不需要考虑与指针手指起始定义时间间隔
					Mouse_RButton_Status = 1; //找到右键
					foundIndex_RB = i;
					continue;  //继续找左键
				}
			}
		}

		//赋值新索引及功能定义，逻辑预设为左键和中键不能滑动食指互相切换需要抬起食指后进行改变
		if (Mouse_Wheel_CurrentIndexID == -1 && foundIndex_WH) {//新触摸点作为滚轮辅助参考手指
			Mouse_Wheel_CurrentIndexID = foundIndex_WH;
			currentfinger[foundIndex_WH].func_Def = Mouse_Wheel_dot;
		}
		if (Mouse_LButton_CurrentIndexID != -1) {//已经存在左键
			Mouse_LButton_Status = 1; 
		}
		else if (Mouse_LButton_CurrentIndexID == -1 && foundIndex_LB) {//新触摸点作为左键按下
			Mouse_LButton_CurrentIndexID = foundIndex_LB;
			currentfinger[foundIndex_LB].func_Def = Mouse_LButton_dot;
		}
		if (Mouse_RButton_CurrentIndexID != -1) {//已经存在右键
			Mouse_RButton_Status = 1;
		}
		else if (Mouse_RButton_CurrentIndexID == -1 && foundIndex_RB) {//新触摸点作为右键按下
			Mouse_RButton_CurrentIndexID = foundIndex_RB;
			currentfinger[foundIndex_RB].func_Def = Mouse_RButton_dot;
		}
		if (Mouse_MButton_CurrentIndexID != -1) {//已经存在中键
			Mouse_MButton_Status = 1;
		}
		else if (Mouse_MButton_CurrentIndexID == -1 && foundIndex_MB) {//新触摸点作为中键按下
			Mouse_MButton_CurrentIndexID = foundIndex_MB;
			currentfinger[foundIndex_MB].func_Def = Mouse_MButton_dot;
		}

		//鼠标指针位移设置
		if (currentfinger_count != lastfinger_count) {//手指变化瞬间时电容可能不稳定指针坐标突发性漂移需要忽略
			Jitter = TRUE;
		}
		else{
			float px = (float)(currentfinger[Mouse_Pointer_CurrentIndexID].pos_x - lastfinger[Mouse_Pointer_LastIndexID].pos_x) / thumb_scale;
			float py = (float)(currentfinger[Mouse_Pointer_CurrentIndexID].pos_y - lastfinger[Mouse_Pointer_LastIndexID].pos_y) / thumb_scale;

			if (Jitter) {//指针抖动修正，另外参考指针旁边的手指位移量修正指针位移
				Jitter = FALSE;
				px =0;
				py= 0;
			}
			else {
				if (abs(px) <=Jitter_Offset) {//指针轻微抖动修正
					px = 0;
				}
				if (abs(py) <= Jitter_Offset) {//指针轻微抖动修正
					py = 0;
				}
					//其他手指非常紧贴中指时作参考位移修正处理	
				if (Mouse_LButton_CurrentIndexID != -1) {
						float lx = (float)(currentfinger[Mouse_LButton_CurrentIndexID].pos_x - lastfinger[Mouse_LButton_LastIndexID].pos_x) / thumb_scale;
						if (abs(lx - px) > Exception_Offset|| (lx>0 && px<0) || (lx < 0 && px > 0)) {//参考触摸点位移与指针位移差异过大或者位移方向相反
							px = 0;
							py = 0;
						}
				}
				if (Mouse_RButton_CurrentIndexID != -1) {
					float rx = (float)(currentfinger[Mouse_RButton_CurrentIndexID].pos_x - lastfinger[Mouse_RButton_LastIndexID].pos_x) / thumb_scale;
					if (abs(rx - px) > Exception_Offset || (rx > 0 && px < 0) || (rx < 0 && px > 0)) {//参考触摸点位移与指针位移差异过大或者位移方向相反
						px = 0;
						py = 0;
					}
				}
				if (Mouse_MButton_CurrentIndexID != -1) {
					float mx = (float)(currentfinger[Mouse_MButton_CurrentIndexID].pos_x - lastfinger[Mouse_MButton_LastIndexID].pos_x) / thumb_scale;
					if (abs(mx - px) > Exception_Offset || (mx > 0 && px < 0) || (mx < 0 && px > 0)) {//参考触摸点位移与指针位移差异过大或者位移方向相反
						px = 0;
						py = 0;
					}
				}
				
				if (Mouse_LButton_CurrentIndexID != -1) {
						float ly = (float)(currentfinger[Mouse_LButton_CurrentIndexID].pos_y - lastfinger[Mouse_LButton_LastIndexID].pos_y) / thumb_scale;
						if (abs(ly - py) > Exception_Offset || (ly > 0 && py < 0) || (ly < 0 && py > 0)) {//参考触摸点位移与指针位移差异过大或者位移方向相反
							px = 0;
							py = 0;
						}
				}
				if (Mouse_RButton_CurrentIndexID != -1) {
					float ry = (float)(currentfinger[Mouse_RButton_CurrentIndexID].pos_y - lastfinger[Mouse_RButton_LastIndexID].pos_y) / thumb_scale;
					if (abs(ry - py) > Exception_Offset || (ry > 0 && py < 0) || (ry < 0 && py > 0)) {//参考触摸点位移与指针位移差异过大或者位移方向相反
						px = 0;
						py = 0;
					}
				}
				if (Mouse_MButton_CurrentIndexID != -1) {
					float my = (float)(currentfinger[Mouse_MButton_CurrentIndexID].pos_y - lastfinger[Mouse_MButton_LastIndexID].pos_y) / thumb_scale;
					if (abs(my - py) > Exception_Offset || (my > 0 && py < 0) || (my < 0 && py > 0)) {//参考触摸点位移与指针位移差异过大或者位移方向相反
						px = 0;
						py = 0;
					}
				}
			}
			evt.dx = (short)(px / PointerSensitivity);
			evt.dy = -(short)(py / PointerSensitivity);
		}
	}
	else if (Mouse_Pointer_CurrentIndexID != -1 && Mouse_Wheel_mode) {//滚轮操作模式
		//鼠标指针位移设置
		if (currentfinger_count != lastfinger_count) {//手指变化瞬间时电容可能不稳定指针坐标突发性漂移需要忽略
			Jitter = TRUE;
		}
		else {
			float px = (float)(currentfinger[Mouse_Pointer_CurrentIndexID].pos_x - lastfinger[Mouse_Pointer_LastIndexID].pos_x) / thumb_scale;
			float py = (float)(currentfinger[Mouse_Pointer_CurrentIndexID].pos_y - lastfinger[Mouse_Pointer_LastIndexID].pos_y) / thumb_scale;

			if (Jitter) {//指针抖动修正，另外参考指针旁边的手指位移量修正指针位移
				Jitter = FALSE;
				px = 0;
				py = 0;
			}
			else {
				if (abs(px) <= Jitter_Offset) {//指针轻微抖动修正
					px = 0;
				}
				if (abs(py) <= Jitter_Offset) {//指针轻微抖动修正
					py = 0;
				}
				//其他手指非常紧贴中指时作参考位移修正处理	
				if (Mouse_Wheel_CurrentIndexID != -1) {
					float wx = (float)(currentfinger[Mouse_Wheel_CurrentIndexID].pos_x - lastfinger[Mouse_Wheel_LastIndexID].pos_x) / thumb_scale;
					if (abs(wx - px) > Exception_Offset || (wx > 0 && px < 0) || (wx < 0 && px > 0)) {//参考触摸点位移与指针位移差异过大或者位移方向相反
						px = 0;
						py = 0;
					}
				}
				if (Mouse_Wheel_CurrentIndexID != -1) {
					float wy = (float)(currentfinger[Mouse_Wheel_CurrentIndexID].pos_y - lastfinger[Mouse_Wheel_LastIndexID].pos_y) / thumb_scale;
					if (abs(wy - py) > Exception_Offset || (wy > 0 && py < 0) || (wy < 0 && py > 0)) {//参考触摸点位移与指针位移差异过大或者位移方向相反
						px = 0;
						py = 0;
					}
				}
			}

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

			if (abs(px) > 4 && abs(px) < 32) {
				if (Scroll_IntervalCount == 0) {//先滚动一次再间隔计数
					evt.h_wheel = (char)(px > 0 ? 1 : -1);
					Scroll_IntervalCount = 16;
				}
				else {
					Scroll_IntervalCount--;
				}
			}
			else if (abs(px) >= 32) {
				evt.h_wheel = (char)(px > 0 ? 1 : -1);
				Scroll_IntervalCount = 0;
			}
			if (abs(py) > 4 && abs(py) < 32) {
				if (Scroll_IntervalCount == 0) {
					evt.v_wheel = (char)(py > 0 ? 1 : -1);
					Scroll_IntervalCount = 16;
				}
				else {
					Scroll_IntervalCount--;
				}
			}
			else if (abs(py) >= 32) {
				evt.v_wheel = (char)(py > 0 ? 1 : -1);
				Scroll_IntervalCount = 0;
			}
		}
	}
	else {
		//其他组合无效
	}
		
	evt.button = Mouse_LButton_Status + (Mouse_RButton_Status << 1) + (Mouse_MButton_Status << 2);  //左中右键状态合成
	tp->evt_cbk(&evt, tp->evt_param);//设置鼠标事件
	
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

