#include "MouseLikeTouchPad.h"
#include "apple_tp_proto.h"
#include<math.h>
extern "C" int _fltused = 0;


#define MAXFINGER_CNT 10

#define MouseButtonPointer_IntervalThreshold         12   // 鼠标左中右键与指针操作间隔计数阈值，

#define Jitter_Offset         2    // 修正触摸点轻微抖动的位移阈值
#define Exception_Offset         2    // 修正触摸点漂移异常的位移阈值


struct MouseLikeTouchPad_state
{
	MOUSEEVENTCALLBACK mEvt_cbk;
	void* mEvt_param;
	ULONG tick_count;
};


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
static BOOLEAN Mouse_MButton_Enabled; //鼠标中键功能开启标志

static BYTE JitterFixStep;         // 修正触摸点抖动修正步骤状态
static BYTE MouseButtonPointer_IntervalCount;//指针左右侧的手指按下时间与指针定义起始时间的间隔计数，结合鼠标报告频率估算出时间间隔

static BYTE Scroll_IntervalCount; //定义鼠标滚动间隔计数

static mouse_event_t mEvt;//定义鼠标事件结构体
static struct SPI_TRACKPAD_PACKET* PtpReport;//定义触摸板报告数据指针
static struct SPI_TRACKPAD_FINGER* pFinger_data;//定义触摸板报告手指触摸点数据指针

//临时变量定义成静态的防止存储堆栈溢出
static float dx;
static float dy;
static float distance;
static float px;
static float py;
static float lx;
static float ly;
static float rx;
static float ry;
static float mx;
static float my;
static float wx;
static float wy;
static int direction_hscale;//滚动方向缩放比例
static int direction_vscale;//滚动方向缩放比例


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

/////////////////

VOID RegDebug(WCHAR* strValueName, PVOID dataValue, ULONG datasizeValue)//RegDebug(L"Run debug here",pBuffer,pBufferSize);//RegDebug(L"Run debug here",NULL,0x12345678);
{
	//初始化注册表项
	UNICODE_STRING stringKey;
	RtlInitUnicodeString(&stringKey, L"\\Registry\\Machine\\Software\\RegDebug");

	//初始化OBJECT_ATTRIBUTES结构
	OBJECT_ATTRIBUTES  ObjectAttributes;
	InitializeObjectAttributes(&ObjectAttributes, &stringKey, OBJ_CASE_INSENSITIVE, NULL, NULL);

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



void MouseLikeTouchPad_init(MOUSEEVENTCALLBACK cbk, void* param)
{
	RtlZeroMemory(tp, sizeof(MouseLikeTouchPad_state));

	tp->tick_count = KeQueryTimeIncrement(); ///
	tp->mEvt_cbk = cbk;
	tp->mEvt_param = param;
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
		
		MouseButtonPointer_IntervalCount = 0;
		JitterFixStep = 0;

		Scroll_IntervalCount = 0;

		Mouse_MButton_Enabled = FALSE;//默认关闭中键功能

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

	//保存当前手指坐标
	if (!PtpReport->IsFinger) {//is_finger参数判断手指全部离开，不能用pr->state & 0x80)、pr->state 判断因为多点触摸时任意一个手指离开都会产生该信号，也不能用pr->finger_number判断因为该值不会为0
		currentfinger_count = 0;
	}
	else {
		currentfinger_count = PtpReport->NumOfFingers;
		if (currentfinger_count > 0) {
			for (char i = 0; i < currentfinger_count; i++) {
				pFinger_data = (struct SPI_TRACKPAD_FINGER*)(data + TP_HEADER_SIZE + i * TP_FINGER_SIZE); //
				currentfinger[i] = *pFinger_data;
			}
		}
	}

	//初始化鼠标事件
	mEvt.button = 0;
	mEvt.dx = 0;
	mEvt.dy = 0;
	mEvt.h_wheel = 0;
	mEvt.v_wheel = 0;
	Mouse_LButton_Status = 0; //定义临时鼠标左键状态，0为释放，1为按下，每次都需要重置确保后面逻辑
	Mouse_MButton_Status = 0; //定义临时鼠标中键状态，0为释放，1为按下，每次都需要重置确保后面逻辑
	Mouse_RButton_Status = 0; //定义临时鼠标右键状态，0为释放，1为按下，每次都需要重置确保后面逻辑


	//所有手指触摸点跟踪
	//初始化当前触摸点索引号，跟踪后未再赋值的表示不存在了
	Mouse_Pointer_CurrentIndexID = -1;
	Mouse_LButton_CurrentIndexID = -1;
	Mouse_RButton_CurrentIndexID = -1;
	Mouse_MButton_CurrentIndexID = -1;
	Mouse_Wheel_CurrentIndexID = -1;

	//对已经存在的指针、按键触摸点进行跟踪
	for (char i = 0; i < currentfinger_count; i++) {
		if (i== Mouse_Pointer_CurrentIndexID  || i == Mouse_LButton_CurrentIndexID ||i == Mouse_RButton_CurrentIndexID || i == Mouse_MButton_CurrentIndexID || i == Mouse_Wheel_CurrentIndexID){//跳过已经追踪的触摸点,i是正值所以不需要判断指针按键索引是否存在
			continue;
		}
		if (Mouse_Pointer_LastIndexID !=-1 && currentfinger[i].OriginalX == lastfinger[Mouse_Pointer_LastIndexID].OriginalX && currentfinger[i].OriginalY == lastfinger[Mouse_Pointer_LastIndexID].OriginalY) {//跟踪查找指针
			Mouse_Pointer_CurrentIndexID = i;
			continue;
		}

		if (Mouse_LButton_LastIndexID != -1 && currentfinger[i].OriginalX == lastfinger[Mouse_LButton_LastIndexID].OriginalX && currentfinger[i].OriginalY == lastfinger[Mouse_LButton_LastIndexID].OriginalY) {//跟踪查找左键
			Mouse_LButton_Status = 1; //找到左键，
			Mouse_LButton_CurrentIndexID = i;
			continue;
		}

		if (Mouse_RButton_LastIndexID != -1 && currentfinger[i].OriginalX == lastfinger[Mouse_RButton_LastIndexID].OriginalX && currentfinger[i].OriginalY == lastfinger[Mouse_RButton_LastIndexID].OriginalY) {//跟踪查找右键
			Mouse_RButton_Status = 1; //找到左键，
			Mouse_RButton_CurrentIndexID = i;
			continue;
		}

		if (Mouse_MButton_LastIndexID != -1 && currentfinger[i].OriginalX == lastfinger[Mouse_MButton_LastIndexID].OriginalX && currentfinger[i].OriginalY == lastfinger[Mouse_MButton_LastIndexID].OriginalY) {//跟踪查找中键
			Mouse_MButton_Status = 1; //找到左键，
			Mouse_MButton_CurrentIndexID = i;
			continue;
		}

		if (Mouse_Wheel_LastIndexID != -1 && currentfinger[i].OriginalX == lastfinger[Mouse_Wheel_LastIndexID].OriginalX && currentfinger[i].OriginalY == lastfinger[Mouse_Wheel_LastIndexID].OriginalY) {//跟踪查找滚轮辅助键
			Mouse_Wheel_CurrentIndexID = i;
			continue;
		}
	}

	//开始鼠标事件逻辑判定
	if (currentfinger_count >3 && PtpReport->ClickOccurred) {
		Mouse_MButton_Enabled = !Mouse_MButton_Enabled;//3指以上触摸并按压时可以随时切换开启/关闭鼠标中键功能
	}

	if (Mouse_Pointer_LastIndexID==-1 && currentfinger_count ==1) {//鼠标指针、左键、右键、中键都未定义
			Mouse_Pointer_CurrentIndexID = 0;  //首个触摸点作为指针
			MouseButtonPointer_IntervalCount = 0;//指针左右侧的手指按下时间与指针定义起始时间的间隔计数
	}	
	else if (Mouse_Pointer_CurrentIndexID == -1 && Mouse_Pointer_LastIndexID != -1) {//指针消失
		Mouse_Wheel_mode = FALSE;//结束滚轮模式
		//清除按键功能定义
		Mouse_LButton_CurrentIndexID = -1;
		Mouse_RButton_CurrentIndexID = -1;
		Mouse_MButton_CurrentIndexID = -1;
		Mouse_Wheel_CurrentIndexID = -1;

		MouseButtonPointer_IntervalCount = 0;
	}
	else if (Mouse_Pointer_CurrentIndexID != -1 && !Mouse_Wheel_mode) {//指针已定义的非滚轮事件处理
		//查找指针左侧或者右侧是否有并拢的手指作为滚轮模式或者按键模式，当指针左侧/右侧的手指按下时间与指针手指定义时间间隔小于设定阈值时判定为鼠标滚轮否则为鼠标按键，这一规则能有效区别按键与滚轮操作,但鼠标按键和滚轮不能一起使用
		if (MouseButtonPointer_IntervalCount<127) {
			MouseButtonPointer_IntervalCount++;//指针左右侧的手指按下时间与指针定义起始时间的间隔计数，结合鼠标报告频率估算出时间间隔
		}
		
		if (currentfinger_count > lastfinger_count) {//触摸点增加时查找按键或滚轮事件
			for (char i = 0; i < currentfinger_count; i++) {
				if (i == Mouse_Pointer_CurrentIndexID || i == Mouse_LButton_CurrentIndexID || i == Mouse_RButton_CurrentIndexID || i == Mouse_MButton_CurrentIndexID) {//跳过已经追踪的触摸点,i是正值所以不需要判断指针按键索引是否存在
					continue;
				}

				dx = (float)(currentfinger[i].X - currentfinger[Mouse_Pointer_CurrentIndexID].X);
				dy = (float)(currentfinger[i].Y - currentfinger[Mouse_Pointer_CurrentIndexID].Y);
				distance = sqrt(dx * dx + dy * dy);//触摸点与指针的距离

				if (!Mouse_MButton_Enabled) {//鼠标中键功能关闭时
					//左键、中键、右键按下的情况下不能转变为滚轮模式需要排除后再判断
					if ((Mouse_LButton_CurrentIndexID == -1 && Mouse_LButton_CurrentIndexID == -1 && Mouse_LButton_CurrentIndexID == -1) && \
						abs(distance) > FingerMinDistance && abs(distance) < FingerClosedThresholdDistance && MouseButtonPointer_IntervalCount < MouseButtonPointer_IntervalThreshold) {//指针左右侧有并拢的手指按下并且与指针手指起始定义时间间隔小于阈值

						Mouse_Wheel_mode = TRUE;  //开启滚轮模式
						Mouse_Wheel_CurrentIndexID = i;//滚轮辅助参考手指索引值
						MouseButtonPointer_IntervalCount = 0;
						break;
					}
					else if (Mouse_LButton_CurrentIndexID == -1 && abs(distance) > FingerMinDistance && abs(distance) < FingerMaxDistance && dx < (FingerMinDistance / 2)) {//指针左侧有手指按下，前面滚轮模式条件判断已经排除了所以不需要考虑与指针手指起始定义时间间隔，最后的参数修正左键有可能x轴略大于指针一点点的状况
						Mouse_LButton_Status = 1; //找到左键，
						Mouse_LButton_CurrentIndexID = i;
						continue;  //继续找其他键
					}
					else if (Mouse_RButton_CurrentIndexID == -1 && abs(distance) > FingerMinDistance && abs(distance) < FingerMaxDistance && dx > 0) {//指针右侧有手指按下，前面滚轮模式条件判断已经排除了所以不需要考虑与指针手指起始定义时间间隔
						Mouse_RButton_Status = 1; //找到右键
						Mouse_RButton_CurrentIndexID = i;
						continue;  //继续找其他键
					}
				}
				else {//鼠标中键功能开启时
					//左键、中键、右键按下的情况下不能转变为滚轮模式需要排除后再判断
					if ((Mouse_LButton_CurrentIndexID == -1 && Mouse_LButton_CurrentIndexID == -1 && Mouse_LButton_CurrentIndexID == -1) && \
						abs(distance) > FingerMinDistance && abs(distance) < FingerClosedThresholdDistance && MouseButtonPointer_IntervalCount < MouseButtonPointer_IntervalThreshold) {//指针左右侧有并拢的手指按下并且与指针手指起始定义时间间隔小于阈值
						Mouse_Wheel_mode = TRUE;  //开启滚轮模式
						Mouse_Wheel_CurrentIndexID = i;//滚轮辅助参考手指索引值
						MouseButtonPointer_IntervalCount = 0;
						break;
					}
					else if (Mouse_MButton_CurrentIndexID == -1 && abs(distance) > FingerMinDistance && abs(distance) < FingerClosedThresholdDistance && MouseButtonPointer_IntervalCount > MouseButtonPointer_IntervalThreshold && dx < 0) {//指针左侧有并拢的手指按下并且与指针手指起始定义时间间隔大于阈值
						Mouse_MButton_Status = 1; //找到中键
						Mouse_MButton_CurrentIndexID = i;
						MouseButtonPointer_IntervalCount = 0;
						continue;  //继续找其他键，食指已经被中键占用所以原则上左键已经不可用
					}
					else if (Mouse_LButton_CurrentIndexID == -1 && abs(distance) > FingerMinDistance && abs(distance) < FingerMaxDistance && dx < (FingerMinDistance / 2)) {//指针左侧有手指按下，前面滚轮模式条件判断已经排除了所以不需要考虑与指针手指起始定义时间间隔，最后的参数修正左键有可能x轴略大于指针一点点的状况，
						Mouse_LButton_Status = 1; //找到左键
						Mouse_LButton_CurrentIndexID = i;
						continue;  //继续找其他键
					}
					else if (Mouse_RButton_CurrentIndexID == -1 && abs(distance) > FingerMinDistance && abs(distance) < FingerMaxDistance && dx > 0) {//指针右侧有手指按下，前面滚轮模式条件判断已经排除了所以不需要考虑与指针手指起始定义时间间隔
						Mouse_RButton_Status = 1; //找到右键
						Mouse_RButton_CurrentIndexID = i;
						continue;  //继续找其他键
					}
				}
			}
		}

		if (!Mouse_Wheel_mode) {//非滚轮模式下鼠标指针位移设置
			if (currentfinger_count != lastfinger_count) {//手指变化瞬间时电容可能不稳定指针坐标突发性漂移需要忽略
				JitterFixStep = 1;
			}
			else {
				px = (float)(currentfinger[Mouse_Pointer_CurrentIndexID].X - lastfinger[Mouse_Pointer_LastIndexID].X) / thumb_scale;
				py = (float)(currentfinger[Mouse_Pointer_CurrentIndexID].Y - lastfinger[Mouse_Pointer_LastIndexID].Y) / thumb_scale;

				if (JitterFixStep == 1) {//指针抖动修正，另外参考指针旁边的手指位移量修正指针位移
					JitterFixStep=0;
					px = 0;
					py = 0;
				}
				else {
					if (JitterFixStep > 0 && JitterFixStep < 10) {
						JitterFixStep++;
					}

					if (abs(px) <= Jitter_Offset) {//指针轻微抖动修正
						px = 0;
					}
					if (abs(py) <= Jitter_Offset) {//指针轻微抖动修正
						py = 0;
					}
					//其他手指非常紧贴中指时作参考位移修正处理	
					if (Mouse_LButton_CurrentIndexID != -1) {
						lx = (float)(currentfinger[Mouse_LButton_CurrentIndexID].X - lastfinger[Mouse_LButton_LastIndexID].X) / thumb_scale;
						if (abs(lx - px) > Exception_Offset || (lx > 0 && px < 0) || (lx < 0 && px > 0)) {//参考触摸点位移与指针位移差异过大或者位移方向相反
							px = 0;
							py = 0;
						}
					}
					if (Mouse_RButton_CurrentIndexID != -1) {
						rx = (float)(currentfinger[Mouse_RButton_CurrentIndexID].X - lastfinger[Mouse_RButton_LastIndexID].X) / thumb_scale;
						if (abs(rx - px) > Exception_Offset || (rx > 0 && px < 0) || (rx < 0 && px > 0)) {//参考触摸点位移与指针位移差异过大或者位移方向相反
							px = 0;
							py = 0;
						}
					}
					if (Mouse_MButton_CurrentIndexID != -1) {
						mx = (float)(currentfinger[Mouse_MButton_CurrentIndexID].X - lastfinger[Mouse_MButton_LastIndexID].X) / thumb_scale;
						if (abs(mx - px) > Exception_Offset || (mx > 0 && px < 0) || (mx < 0 && px > 0)) {//参考触摸点位移与指针位移差异过大或者位移方向相反
							px = 0;
							py = 0;
						}
					}

					if (Mouse_LButton_CurrentIndexID != -1) {
						ly = (float)(currentfinger[Mouse_LButton_CurrentIndexID].Y - lastfinger[Mouse_LButton_LastIndexID].Y) / thumb_scale;
						if (abs(ly - py) > Exception_Offset || (ly > 0 && py < 0) || (ly < 0 && py > 0)) {//参考触摸点位移与指针位移差异过大或者位移方向相反
							px = 0;
							py = 0;
						}
					}
					if (Mouse_RButton_CurrentIndexID != -1) {
						ry = (float)(currentfinger[Mouse_RButton_CurrentIndexID].Y - lastfinger[Mouse_RButton_LastIndexID].Y) / thumb_scale;
						if (abs(ry - py) > Exception_Offset || (ry > 0 && py < 0) || (ry < 0 && py > 0)) {//参考触摸点位移与指针位移差异过大或者位移方向相反
							px = 0;
							py = 0;
						}
					}
					if (Mouse_MButton_CurrentIndexID != -1) {
						my = (float)(currentfinger[Mouse_MButton_CurrentIndexID].Y - lastfinger[Mouse_MButton_LastIndexID].Y) / thumb_scale;
						if (abs(my - py) > Exception_Offset || (my > 0 && py < 0) || (my < 0 && py > 0)) {//参考触摸点位移与指针位移差异过大或者位移方向相反
							px = 0;
							py = 0;
						}
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
			JitterFixStep = 1;
		}
		else {
			px = (float)(currentfinger[Mouse_Pointer_CurrentIndexID].X - lastfinger[Mouse_Pointer_LastIndexID].X) / thumb_scale;
			py = (float)(currentfinger[Mouse_Pointer_CurrentIndexID].Y - lastfinger[Mouse_Pointer_LastIndexID].Y) / thumb_scale;

			if (JitterFixStep == 1) {//指针抖动修正，另外参考指针旁边的手指位移量修正指针位移
				JitterFixStep = 0;
				px = 0;
				py = 0;
			}
			else {
				if (JitterFixStep >0 && JitterFixStep <10) {
					JitterFixStep++;
				}

				if (abs(px) <= Jitter_Offset) {//指针轻微抖动修正
					px = 0;
				}
				if (abs(py) <= Jitter_Offset) {//指针轻微抖动修正
					py = 0;
				}
				//其他手指非常紧贴中指时作参考位移修正处理	
				if (Mouse_Wheel_CurrentIndexID != -1) {
					wx = (float)(currentfinger[Mouse_Wheel_CurrentIndexID].X - lastfinger[Mouse_Wheel_LastIndexID].X) / thumb_scale;
					if (abs(wx - px) > Exception_Offset || (wx > 0 && px < 0) || (wx < 0 && px > 0)) {//参考触摸点位移与指针位移差异过大或者位移方向相反
						px = 0;
						py = 0;
					}
				}
				if (Mouse_Wheel_CurrentIndexID != -1) {
					wy = (float)(currentfinger[Mouse_Wheel_CurrentIndexID].Y - lastfinger[Mouse_Wheel_LastIndexID].Y) / thumb_scale;
					if (abs(wy - py) > Exception_Offset || (wy > 0 && py < 0) || (wy < 0 && py > 0)) {//参考触摸点位移与指针位移差异过大或者位移方向相反
						px = 0;
						py = 0;
					}
				}
			}

			direction_hscale = 1;//滚动方向缩放比例
			direction_vscale = 1;//滚动方向缩放比例

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
					mEvt.h_wheel = (char)(px > 0 ? 1 : -1);
					Scroll_IntervalCount = 16;
				}
				else {
					Scroll_IntervalCount--;
				}
			}
			else if (abs(px) >= 32) {
				mEvt.h_wheel = (char)(px > 0 ? 1 : -1);
				Scroll_IntervalCount = 0;
			}
			if (abs(py) > 4 && abs(py) < 32) {
				if (Scroll_IntervalCount == 0) {
					mEvt.v_wheel = (char)(py > 0 ? 1 : -1);
					Scroll_IntervalCount = 16;
				}
				else {
					Scroll_IntervalCount--;
				}
			}
			else if (abs(py) >= 32) {
				mEvt.v_wheel = (char)(py > 0 ? 1 : -1);
				Scroll_IntervalCount = 0;
			}
		}
	}
	else {
		//其他组合无效
	}
		
	mEvt.button = Mouse_LButton_Status + (Mouse_RButton_Status << 1) + (Mouse_MButton_Status << 2);  //左中右键状态合成
	tp->mEvt_cbk(&mEvt, tp->mEvt_param);//设置鼠标事件

	
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

