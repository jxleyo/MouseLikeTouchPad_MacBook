#include "MouseLikeTouchPad.h"
#include "apple_tp_proto.h"
#include<math.h>
extern "C" int _fltused = 0;


#define MAXFINGER_CNT 10

struct MouseLikeTouchPad_state
{
	MOUSEEVENTCALLBACK evt_cbk;
	void* evt_param;
	ULONG tick_count;
};


struct FingerPosition
{
	short              pos_x;
	short              pos_y;
};


//保存追踪的手指
static struct FingerPosition lastfinger[MAXFINGER_CNT];
static struct FingerPosition currentfinger[MAXFINGER_CNT];
static int currentfinger_count=0;
static int lastfinger_count=0;

static int Mouse_Pointer_lastIndexID; //定义临时追踪鼠标指针触摸点坐标的数据索引号，-1为未定义
static int Mouse_Pointer_currentIndexID; //定义临时鼠标指针触摸点坐标的数据索引号，-1为未定义

static BOOLEAN Mouse_LButton_Status; //定义鼠标左键状态，0为释放，1为按下
static BOOLEAN Mouse_MButton_Status; //定义鼠标中键状态，0为释放，1为按下
static BOOLEAN Mouse_RButton_Status; //定义鼠标右键状态，0为释放，1为按下

static BOOLEAN Mouse_Wheel_mode; //定义鼠标滚轮状态，0为滚轮未激活，1为滚轮激活
static BOOLEAN TouchRoundStart; //触摸回合开始标志
static BOOLEAN Mouse_MButton_Enabled; //鼠标中键功能开启标志

static LARGE_INTEGER MousePointer_DefineTime;//鼠标指针定义时间，用于计算按键间隔时间来区分判定鼠标中间和滚轮操作
static float TouchPad_ReportInterval;//定义触摸板报告间隔时间
static float Jitter_Interval;//定义抖动修正间隔时间

static int Scroll_IntervalCount; //定义鼠标滚动间隔计数
static short evt_idle_repeat_count; //鼠标空载计数

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
		Mouse_Pointer_lastIndexID = -1; //定义鼠标指针触摸点坐标的数据索引号，-1为未定义
		Mouse_Pointer_currentIndexID = -1; //定义鼠标指针触摸点坐标的数据索引号，-1为未定义
		Mouse_LButton_Status = 0; //定义鼠标左键状态，0为释放，1为按下
		Mouse_MButton_Status = 0;//定义鼠标中键状态，0为释放，1为按下
		Mouse_RButton_Status = 0; //定义鼠标右键状态，0为释放，1为按下

		Mouse_Wheel_mode = 0;

		lastfinger_count = 0;
		currentfinger_count = 0;
		for (u8 i = 0; i < MAXFINGER_CNT; ++i) {
			lastfinger[i].pos_x = 0;
			lastfinger[i].pos_y = 0;
			currentfinger[i].pos_x = 0;
			currentfinger[i].pos_y = 0;
		}

		Jitter_Interval = 0;
		Scroll_IntervalCount = 0;
		evt_idle_repeat_count = 0;
		TouchRoundStart = TRUE;	
		Mouse_MButton_Enabled = FALSE;//默认关闭中键功能

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
		TouchRoundStart = TRUE;//下一轮触摸操作回合开始

		currentfinger_count = 0;
		/*for (int i = 0; i < MAXFINGER_CNT; ++i) {
			currentfinger[i].pos_x = 0;
			currentfinger[i].pos_y = 0;
		}*/
	}
	else {
		currentfinger_count = pr->finger_number;
		if (currentfinger_count > 0) {
			for (int i = 0; i < currentfinger_count; i++) {
				struct tp_finger* g = (struct tp_finger*)(data + TP_HEADER_SIZE + i * TP_FINGER_SIZE); //
				currentfinger[i].pos_x = g->x;
				currentfinger[i].pos_y = g->y;
			}
		}
	}

	//初始化鼠标事件
	evt.button = 0;
	evt.dx = 0;
	evt.dy = 0;
	evt.h_wheel = 0;
	evt.v_wheel = 0;
	Mouse_LButton_Status = 0; //定义鼠标左键状态，0为释放，1为按下，每次都需要重置确保后面逻辑
	Mouse_MButton_Status = 0; //定义鼠标中键状态，0为释放，1为按下，每次都需要重置确保后面逻辑
	Mouse_RButton_Status = 0; //定义鼠标右键状态，0为释放，1为按下，每次都需要重置确保后面逻辑

	if (currentfinger_count >3 && pr->clicked) {
		Mouse_MButton_Enabled = !Mouse_MButton_Enabled;//3指以上触摸并按压时可以随时切换开启/关闭鼠标中键功能
	}

	if (Mouse_Pointer_lastIndexID==-1 && TouchRoundStart) {//鼠标指针、左键、右键、中键都未定义
		if (currentfinger_count > 0) {
			Mouse_Pointer_currentIndexID = 0;  //首个触摸点作为指针
			MousePointer_DefineTime = current_ticktime;//定义当前指针起始时间
		}	
	}
	else if (Mouse_Pointer_lastIndexID != -1 && !Mouse_Wheel_mode && TouchRoundStart) {//指针已定义的非滚轮事件处理
		//查找指针
		BOOLEAN foundi = 0;
		for (int i = 0; i < currentfinger_count; i++) {
			float dx = (float)(currentfinger[i].pos_x - lastfinger[Mouse_Pointer_lastIndexID].pos_x);
			float dy = (float)(currentfinger[i].pos_y - lastfinger[Mouse_Pointer_lastIndexID].pos_y);
			float distance = sqrt(dx*dx + dy*dy);

			if (abs(distance) < FingerTracingMaxOffset) {	
				if (currentfinger_count != lastfinger_count) {//手指变化瞬间时电容不稳定指针坐标突发性漂移需要忽略
					Jitter_Interval = 0;//开始抖动修正计时
				}
				else {
					float rx = dx / thumb_scale;
					float ry = dy / thumb_scale;

					if (Jitter_Interval < Jitter_Time_MSEC) {//手指变化后续一段时间内指针位移做修正处理
						Jitter_Interval += TouchPad_ReportInterval;//累计抖动修正时间
						if (abs(rx) < Jitter_Offset) {//指针抖动修正，另外参考指针旁边的手指位移量修正指针位移
							rx = 0;
						}
						if (abs(ry) < Jitter_Offset) {//指针抖动修正，另外参考指针旁边的手指位移量修正指针位移
							ry = 0;
						}
					}
					else {//超过时间后无需修正
						Jitter_Interval = 0;//重置抖动修正计时	
					}

					evt.dx = (short)(rx / PointerSensitivity);
					evt.dy = -(short)(ry / PointerSensitivity);
						
				}
				
				Mouse_Pointer_currentIndexID = i; //最后为指针赋值新的索引
				foundi = 1;
				break;
			}
		}
		if (!foundi) {//指针消失则全部操作无效
			TouchRoundStart = FALSE;

			Mouse_Pointer_currentIndexID = -1;//指针消失
		}
		else {//指针存在的情况下查询是否有左键右键操作或者滚轮操作
			for (int i = 0; i < currentfinger_count; i++) {
				if (i == Mouse_Pointer_currentIndexID) {
					continue;
				}
				float dx = (float)(currentfinger[i].pos_x - currentfinger[Mouse_Pointer_lastIndexID].pos_x);
				float dy = (float)(currentfinger[i].pos_y - currentfinger[Mouse_Pointer_lastIndexID].pos_y);
				float distance = sqrt(dx * dx + dy * dy);

				//查找指针左侧或者右侧是否有并拢的手指作为滚轮模式或者按键模式，当指针左侧/右侧的手指按下时间与指针手指定义时间间隔小于设定阈值时判定为鼠标滚轮否则为鼠标按键，这一规则能有效区别按键与滚轮操作,但鼠标按键和滚轮不能一起使用
				LARGE_INTEGER MouseButton_Interval;
				MouseButton_Interval.QuadPart = (current_ticktime.QuadPart - MousePointer_DefineTime.QuadPart) * tp->tick_count / 10000;//单位ms毫秒
				float Mouse_Button_Interval = (float)MouseButton_Interval.LowPart;//指针左右侧的手指按下时间与指针定义起始时间的间隔ms

				if (!Mouse_MButton_Enabled) {//鼠标中键功能关闭时
					if (abs(distance) > FingerMinDistance && abs(distance) < FingerClosedThresholdDistance && Mouse_Button_Interval < MButton_Interval_MSEC) {//指针左右侧有并拢的手指按下并且与指针手指起始定义时间间隔小于阈值
						Mouse_Wheel_mode = TRUE;  //开启滚轮模式
						break;
					}
					if (abs(distance) > FingerMinDistance && abs(distance) < FingerMaxDistance && dx < (FingerMinDistance / 2)) {//指针左侧有手指按下，前面滚轮模式条件判断已经排除了所以不需要考虑与指针手指起始定义时间间隔，最后的参数修正左键有可能x轴略大于指针一点点的状况
						Mouse_LButton_Status = 1; //找到左键
						continue;  //继续找右键
					}
					if (abs(distance) > FingerMinDistance && abs(distance) < FingerMaxDistance && dx > 0) {//指针右侧有手指按下，前面滚轮模式条件判断已经排除了所以不需要考虑与指针手指起始定义时间间隔
						Mouse_RButton_Status = 1; //找到右键
						continue;  //继续找左键
					}
				}
				else {//鼠标中键功能开启时
					if (abs(distance) > FingerMinDistance && abs(distance) < FingerClosedThresholdDistance && Mouse_Button_Interval < MButton_Interval_MSEC) {//指针左右侧有并拢的手指按下并且与指针手指起始定义时间间隔小于阈值
						Mouse_Wheel_mode = TRUE;  //开启滚轮模式
						break;
					}
					else if (abs(distance) > FingerMinDistance && abs(distance) < FingerClosedThresholdDistance && Mouse_Button_Interval > MButton_Interval_MSEC && dx < 0) {//指针左侧有并拢的手指按下并且与指针手指起始定义时间间隔大于阈值
						Mouse_MButton_Status = 1; //找到中键
						continue;  //继续找右键，食指已经被中键占用所以原则上左键已经不可用
					}
					else if (abs(distance) > FingerMinDistance && abs(distance) < FingerMaxDistance && dx < (FingerMinDistance / 2)) {//指针左侧有手指按下，前面滚轮模式条件判断已经排除了所以不需要考虑与指针手指起始定义时间间隔，最后的参数修正左键有可能x轴略大于指针一点点的状况，
						Mouse_LButton_Status = 1; //找到左键
						continue;  //继续找右键
					}
					else if (abs(distance) > FingerMinDistance && abs(distance) < FingerMaxDistance && dx > 0) {//指针右侧有手指按下，前面滚轮模式条件判断已经排除了所以不需要考虑与指针手指起始定义时间间隔
						Mouse_RButton_Status = 1; //找到右键
						continue;  //继续找左键
					}
				}
					
			}
		}
	}
	else if (Mouse_Pointer_lastIndexID != -1 && Mouse_Wheel_mode && TouchRoundStart) {//滚轮操作模式
		// 查找指针
		BOOLEAN foundi = 0;
		for (int i = 0; i < currentfinger_count; i++) {
			float dx = (float)(currentfinger[i].pos_x - lastfinger[Mouse_Pointer_lastIndexID].pos_x);
			float dy = (float)(currentfinger[i].pos_y - lastfinger[Mouse_Pointer_lastIndexID].pos_y);
			float distance = sqrt(dx * dx + dy * dy);
			if (abs(distance) < FingerTracingMaxOffset) {
				if (currentfinger_count != lastfinger_count) {//手指变化瞬间时电容不稳定指针坐标突发性漂移需要忽略
					Jitter_Interval = 0;//开始抖动修正计时
				}
				else {
					float rx = dx / thumb_scale;
					float ry = dy / thumb_scale;

					if (Jitter_Interval < Jitter_Time_MSEC) {//手指变化后续一段时间内指针位移做修正处理
						Jitter_Interval += TouchPad_ReportInterval;//累计抖动修正时间
						if (abs(rx) < Jitter_Offset) {//指针抖动修正，另外参考指针旁边的手指位移量修正指针位移
							rx = 0;
						}
						if (abs(ry) < Jitter_Offset) {//指针抖动修正，另外参考指针旁边的手指位移量修正指针位移
							ry = 0;
						}
					}
					else {//超过时间后无需修正
						Jitter_Interval = 0;//重置抖动修正计时	
					}

					int direction_hscale = 1;//滚动方向缩放比例
					int direction_vscale = 1;//滚动方向缩放比例

					if (abs(rx) > abs(dy) / 4) {//滚动方向稳定性修正
						direction_hscale = 1;
						direction_vscale = 8;
					}
					if (abs(ry) > abs(dx) / 4) {//滚动方向稳定性修正
						direction_hscale = 8;
						direction_vscale = 1;
					}

					rx = rx / direction_hscale;
					ry = ry / direction_vscale;

					if (abs(rx) > 4 && abs(rx) < 32) {
						if (Scroll_IntervalCount == 0) {//先滚动一次再间隔计数
							evt.h_wheel = (char)(rx > 0 ? 1 : -1);
							Scroll_IntervalCount = 16;
						}
						else {
							Scroll_IntervalCount--;
						}
					}
					else if (abs(rx) >= 32) {
						evt.h_wheel = (char)(rx > 0 ? 1 : -1);
						Scroll_IntervalCount = 0;
					}
					if (abs(ry) > 4 && abs(ry) < 32) {
						if (Scroll_IntervalCount == 0) {
							evt.v_wheel = (char)(ry > 0 ? 1 : -1);
							Scroll_IntervalCount = 16;
						}
						else {
							Scroll_IntervalCount--;
						}
					}
					else if (abs(ry) >= 32) {
						evt.v_wheel = (char)(ry > 0 ? 1 : -1);
						Scroll_IntervalCount = 0;
					}
				}
				
				Mouse_Pointer_currentIndexID = i; //最后为指针赋值新的索引
				foundi = 1;//滚轮模式持续直到指针消失
				break;
			}
		}
		if (!foundi) {//指针消失则全部操作无效
			TouchRoundStart = FALSE;

			Mouse_Wheel_mode = FALSE;
			Mouse_Pointer_currentIndexID = -1;//指针消失

			Scroll_IntervalCount = 0;
		} 
		
	}
	else {
		//其他组合无效
	}

	evt.button = Mouse_LButton_Status + (Mouse_RButton_Status << 1) + (Mouse_MButton_Status << 2);  //左中右键状态合成
	tp->evt_cbk(&evt, tp->evt_param);//设置鼠标事件
	
	//保存下一轮初始坐标
	for (int i = 0; i < currentfinger_count; i++) {
		lastfinger[i] = currentfinger[i];
	}
	lastfinger_count = currentfinger_count;
	Mouse_Pointer_lastIndexID = Mouse_Pointer_currentIndexID;

}

