#include "beep.h"
#include "music.h"
#include "tim.h"

#define BUZZER_TIM_CHANNEL 						TIM_CHANNEL_1			//根据配置的定时器通道进行更改
static 	TIM_HandleTypeDef *Buzzer_handle = 		&htim1;	//根据配置的定时器句柄进行更改 

#define PLAY_SPEED		200		//音乐播放速度(数值越小速度越快)

static uint16_t beep_time;			//蜂鸣器响计时
static uint8_t play_music_flag;		//播放音乐标志位
const uint8_t* music;				//外部调用指针用于音乐列表选择
/**********************************************************************
 * 功能描述： 无源蜂鸣器控制函数
 * 输入参数： on - 1-响, 0-不响
 * 输出参数： 无
 * 返 回 值： 无
 ***********************************************************************/
void Buzzer_Control(uint8_t on)
{
    if (on)
    {
        HAL_TIM_PWM_Start(Buzzer_handle, BUZZER_TIM_CHANNEL);
    }        
    else
    {
        HAL_TIM_PWM_Stop(Buzzer_handle, BUZZER_TIM_CHANNEL);
    }
}
/**********************************************************************
 * 功能描述： 无源蜂鸣器控制函数: 设置频率和占空比
 * 输入参数： duty - 占空比
 * 输出参数： 无
 * 返 回 值： 无
 ***********************************************************************/
void Buzzer_set_duty(int duty)
{
	__HAL_TIM_SetCompare(Buzzer_handle,BUZZER_TIM_CHANNEL,duty);
}
/*****************************************************************************
***@breif	无源蜂鸣器设置频率和占空比
***@param	freq:频率
***@param	duty:占空比
***@retval	无
*******************************************************************************/
void Buzzer_set_freq_duty(int freq, int duty)
{
    HAL_TIM_PWM_Stop(Buzzer_handle, BUZZER_TIM_CHANNEL);//蜂鸣器停止
	
	Buzzer_handle->Init.Prescaler = 71;		//重新设置频率
    Buzzer_handle->Init.Period = 1000000 / freq - 1;  
    HAL_TIM_Base_Init(Buzzer_handle);
	
	uint16_t compare=(1000000 / freq - 1) * duty / 100;//重新设置占空比
	Buzzer_set_duty(compare);
	
    HAL_TIM_PWM_Start(Buzzer_handle, BUZZER_TIM_CHANNEL);//重新打开蜂鸣器
}
/*****************************************************************************
***@breif	蜂鸣器计时(放在1ms定时中断里)
***@param	无
***@retval	无
*******************************************************************************/
void beeping()
{
    if(beep_time!=0)//计时
    {
        beep_time--;
    }
    if(beep_time==1)//停止
	{
		Buzzer_set_duty(0);
	}
}
/*****************************************************************************
***@breif	蜂鸣器响，作为提示音(外部调用)
***@param	响的时间
***@retval	无
*******************************************************************************/
void beep(uint16_t time)
{    
	beep_time=time;	//计时
	Buzzer_set_freq_duty(yinjie[0],100);
}
/*****************************************************************************
***@breif	用于播放音乐中的蜂鸣器响(外部调用)
***@param	freq:频率
***@param	time:响的时间
***@retval	无
*******************************************************************************/
void beep_music(uint8_t freq,uint16_t time)
{
    if(freq==0)
	{
		return;
	}
	beep_time=time;//计时
	Buzzer_set_freq_duty(yinjie[freq],70);
}
/*****************************************************************************
***@breif	执行播放音乐操作（放在1ms定时中断里）
***@param	播放音乐的列表
***@retval	无
*******************************************************************************/
void beep_play_music(const uint8_t* music)
{
    static uint16_t num=0;	//播放快慢计数
	static uint16_t step=0;	//播放音节
    if(play_music_flag==1)	//播放
    {
        if(num==PLAY_SPEED)//分频
        {
            num=0;//清零
            if(music[step]==0)//结束标志
            {
                play_music_flag=0;//清空
                step=0;
                num=0;
                return;
            }
            beep_music(music[step],100);//蜂鸣器响
            step++;//下一音节
        }
		num++;//分频累加
    }
    else//停止
    {
        num=0;
        step=0;
    }
}
/*****************************************************************************
***@breif	播放音乐接口(外部调用)
***@param	播放的音乐对应数组列表
***@retval	无
*******************************************************************************/
void play_music(const uint8_t* music_list)
{
    play_music_flag=1;	//播放标志位置1
    music=music_list;	//选择要播放的列表
}


