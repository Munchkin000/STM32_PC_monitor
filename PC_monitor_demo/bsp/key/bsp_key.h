#ifndef __BSP_KEY_H__
#define __BSP_KEY_H__

#include "main.h"

#define KEY_SCAN_COUNT_TIME		20		//按键分频扫描周期（需确保定时器中断周期为1ms，数值为多少ms一次）

#define KEY_NUM					1		//按键个数

//按键编号(根据实际需求更改每个按键名称，但给定值不能超过KEY_NUM且从必须0开始编号)
typedef enum
{
	KEY_1=0,	//按键1
	KEY_2,		//按键2
	KEY_3,		//按键3
	KEY_4,		//按键4
}KEY_name_t;

//按键是否在某一时刻按下状态定义
typedef enum	
{
	UNPRESSED=0,	//抬起
	PRESSED,		//按下
}KEY_press_status_t;

//按键状态标志位掩码
typedef enum
{
	NONE=0x00,				//未按下
	KEY_HOLD=0x01,			//按住不放
	KEY_DOWN=0x02,			//按下
	KEY_UP=0x04,			//抬起
	KEY_SINGLE_PRESS=0x08,	//单击
	KEY_DOUBLE_PRESS=0x10,	//双击
	KEY_LONG_PRESS=0x20,	//长按
	KEY_REPEAT_PRESS=0x40,	//连续长按
}KEY_status_t;

//按键状态检测时间阈值（最好设置为KEY_SCAN_COUNT_TIME的整数倍）
typedef enum
{
	LONG_PRESS_TIME		=2000,	//长按时间阈值
	REAPEAT_PRESS_TIME	=100,	//连续长按时间阈值
	DOUBLE_PRESS_TIME	=200,	//双击间隔阈值
}KEY_press_time_t;

uint8_t KEY_check_flag(uint8_t key_num,uint8_t key_flag);
void KEY_clear_flag(uint8_t key_num);
void KEY_clear_all_flags(void);
void KEY_scan(void);

#endif 

