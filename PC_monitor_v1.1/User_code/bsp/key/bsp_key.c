#include "bsp_key.h"

uint8_t key_status_flag[KEY_NUM];	// 按位存储按键状态标志位
/*********************************************************************
*	位	名称		释义			功能描述
*********************************************************************************************************
*	0	HOLD		按住不放		按键按住不放时置1，按键松开时置0
*	1	DOWN		按下时刻		按键按下的时刻置1
*	2	UP			松开时刻		按键松开的时刻置1
*	3	SINGLE		单击			按键按下松开后，没有再次按下，超过双击时间阈值的时刻置1
*	4	DOUBLE		双击			按键按下松开后，在双击时间阈值内再次按下的时刻置1
*	5	LONG		长按			按键按住不放，超过长按时间阈值的时刻置1
*	6	REPEAT		重复长按		按键长按后，每隔重复时间阈值置一次1，直到按键松开
*	7	RESERVE		保留			无功能，后续添加可使用
********************************************************************************************************
说明：HOLD、DOWN、UP，在任何时刻，只要检测到对应的事件，就会置标志位
SINGLE、DOUBLE、LONG/REPEAT，三者互斥，一次完整的按键流程，只会置其中一类标志位
HOLD自动置1和清0，其余标志位在检测到指定事件的时刻置1，读后清0
********************************************************************************************************/

/*****************************************************************************
***@breif	检测按键是否按下
***@param	key_num：按键编号
***@retval	UNPRESSED：未按下 
***@retval	PRESSED：按下
*******************************************************************************/
KEY_press_status_t KEY_get_state(uint8_t key_num)
{	
	switch(key_num)//读取当前按键引脚的电平(根据实际电路修改)
	{
		case KEY_1://按键1
		{
			if(HAL_GPIO_ReadPin(KEY1_GPIO_Port,KEY1_Pin)==GPIO_PIN_SET)	//按下
			{
				return PRESSED;
			}
			break;
		}
		case KEY_2://按键2
		{
//			if(HAL_GPIO_ReadPin(KEY2_GPIO_Port,KEY2_Pin)==GPIO_PIN_RESET)	//按下
//			{
//				return PRESSED;
//			}
			break;
		}
		case KEY_3://按键3
		{
//			if(HAL_GPIO_ReadPin(KEY3_GPIO_Port,KEY3_Pin)==GPIO_PIN_SET)	//按下
//			{
//				return PRESSED;
//			}
			break;
		}
		case KEY_4://按键4
		{
			break;
		}
		default:break;
	}
	return UNPRESSED;//未按下
}
/*****************************************************************************
***@breif	按键状态标志位查询，读取用户对按键的操作
***@param	key_num:按键编号
***@param	key_flag：要查询的按键状态
***@retval	0:不处于当前状态
***@retval	1:处于当前状态
*******************************************************************************/
uint8_t KEY_check_flag(uint8_t key_num,KEY_status_t key_flag)
{
	if(key_status_flag[key_num] & key_flag)	//要查询的标志位为1
	{
		if(key_flag!=KEY_HOLD)	//非HOLD标志位每次查询完清零
		{
			key_status_flag[key_num] &= ~key_flag;//清零
		}		
		return 1;
	}
	return 0;
}
/*****************************************************************************
***@breif	清除一按键的状态标志位
***@param	key_num:按键编号
***@retval	无
*******************************************************************************/
void KEY_clear_flag(uint8_t key_num)
{
	key_status_flag[key_num]=NONE;
}
/*****************************************************************************
***@breif	清除所有按键的状态标志位
***@param	无
***@retval	无
*******************************************************************************/
void KEY_clear_all_flags()
{
	for(uint8_t i=0;i<KEY_NUM;i++)
	{
		key_status_flag[i]=NONE;
	}	
}
/*****************************************************************************
***@breif	放在在定时中断里周期扫描按键状态函数
***@param	无
***@retval	无
*******************************************************************************/
void KEY_scan()
{
	static uint8_t key_scan_count=0;	//分频扫描按键
	static uint8_t key_loop_count=0;	//多个按键循环扫描
	static uint8_t key_current_state[KEY_NUM]={0};	//当前按键状态
	static uint8_t key_previous_state[KEY_NUM]={0};	//上一次按键状态
	static uint8_t status[KEY_NUM] ={0};			//按键状态标志位，用于高级按键状态机判断
	static int key_press_time_count[KEY_NUM]={0};	//持续按下按键时间统计，用于高级按键状态机判断
	
	for(key_loop_count=0;key_loop_count<KEY_NUM;key_loop_count++)	//多个按键循环分别计时
	{
		if(key_press_time_count[key_loop_count]>0)	//大于0则进入按键状态机判断(单位1ms)
		{
			key_press_time_count[key_loop_count]--;//减到0为止
		}
	}	
	
	key_scan_count++;//分频值累加
	
	if(key_scan_count>=KEY_SCAN_COUNT_TIME)//到达设定周期
	{
		key_scan_count=0;	//按键分频计数清零
		
		for(key_loop_count=0;key_loop_count<KEY_NUM;key_loop_count++)	//多个按键循环分别检查状态
		{
			/*读取各按键当前和上一次状态*/
			key_previous_state[key_loop_count]=key_current_state[key_loop_count];
			key_current_state[key_loop_count]=KEY_get_state(key_loop_count);
			
			/*基础按键状态判断*/
			if(key_current_state[key_loop_count]==PRESSED)//按键按住不放时HOLD位置1
			{
				key_status_flag[key_loop_count] |= KEY_HOLD;
			}
			else //按键松开时置0
			{
				//KEY_status =NONE;
				key_status_flag[key_loop_count] &= ~KEY_HOLD;
			}	
			if(key_current_state[key_loop_count]==PRESSED&&key_previous_state[key_loop_count]==UNPRESSED)
			{//按键按下的时刻DOWN位置1
				key_status_flag[key_loop_count] |= KEY_DOWN;			
			}
			else if(key_current_state[key_loop_count]==UNPRESSED&&key_previous_state[key_loop_count]==PRESSED)	
			{//按键松开的时刻UP位置1
				key_status_flag[key_loop_count] |= KEY_UP;
			}
				
			/*高级按键状态判断状态机*/
			switch(status[key_loop_count])
			{
				case 0:	//按键状态0
				{
					if(key_current_state[key_loop_count]==PRESSED) //检测到按下
					{
						key_press_time_count[key_loop_count]=LONG_PRESS_TIME;//设定长按按键检测时间
						status[key_loop_count]=1;	//切换状态
					}
					break;
				}
				case 1:	//按键状态1
				{
					if(key_current_state[key_loop_count]==UNPRESSED)//检测到抬起
					{
						key_press_time_count[key_loop_count]=DOUBLE_PRESS_TIME;//设定双击按键间隔检测时间
						status[key_loop_count]=2;	//切换状态
					}
					else if(key_press_time_count[key_loop_count]<=0)	//达到长按时间阈值
					{
						key_status_flag[key_loop_count] |= KEY_LONG_PRESS;		//长按标志位置1
						key_press_time_count[key_loop_count]=REAPEAT_PRESS_TIME;//设定持续长按时间
						status[key_loop_count]=4;	//切换状态
					}
					break;
				}
				case 2:	//按键状态2
				{
					if(key_current_state[key_loop_count]==PRESSED) //检测到按下
					{
						key_status_flag[key_loop_count] |= KEY_DOUBLE_PRESS; 	//双击标志位置1
						status[key_loop_count]=3;	//切换状态
					}
					else if(key_press_time_count[key_loop_count]<=0) //达到双击间隔时间阈值
					{
						key_status_flag[key_loop_count] |= KEY_SINGLE_PRESS;	//单击标志位置1
						status[key_loop_count]=0;	//切换状态
					}
					break;
				}
				case 3:	//按键状态3
				{
					if(key_current_state[key_loop_count]==UNPRESSED)//检测到抬起
					{
						status[key_loop_count]=0;	//切换状态
					}
					break;
				}
				case 4:	//按键状态4
				{				
					if(key_current_state[key_loop_count]==UNPRESSED)	//检测到抬起
					{
						key_press_time_count[key_loop_count]=0;
						status[key_loop_count]=0;	//切换状态
					}
					else if(key_press_time_count[key_loop_count]<=0)	//达到持续长按时间阈值
					{
						key_status_flag[key_loop_count] |= KEY_REPEAT_PRESS;	//持续长按标志位置1
						key_press_time_count[key_loop_count]=REAPEAT_PRESS_TIME;//设定持续长按时间
					}
					break;
				}
				default:break;
			}
		}	
	}
}

