#include <stdio.h>
#include <string.h>
#include "bsp_timer.h"
#include "tim.h"	// cubeMX配置头文件
//#include "lptim.h"	// cubeMX配置头文件（老型号无）
#include "bsp_key.h"
//#include "bsp_ina226.h"

//uint16_t TIM4_cnt = 0;
/* 普通定时器中断回调函数 */
//void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
//{
////	if(htim->Instance==TIM6)
////	{
////		TIM6_PLL++;//1ms一次计数
////		if(TIM6_PLL>=1000)//1000ms(1s)一次
////		{
////			TIM6_PLL=0;
////			TIM6_Count++;//秒计数加一
////		}
////		
////	}
//	if(htim->Instance==TIM4)
//	{
//		TIM4_cnt++;
//		if(TIM4_cnt >= 500)
//		{
//			/* 灯交替闪烁指示程序运行 */
//			TIM4_cnt = 0;
//			HAL_GPIO_TogglePin(LED1_GPIO_Port,LED1_Pin);	
//		}
//		KEY_scan();
//	}
//}
/* 低功耗定时器中断回调函数(老型号无) */
//void HAL_LPTIM_AutoReloadMatchCallback(LPTIM_HandleTypeDef *hlptim)
//{
//    if (hlptim->Instance == LPTIM4)
//    {
//		KEY_scan();
//		INA_measure_cnt++;
//		if(INA_measure_cnt >= 2000)
//		{
//			INA_measure_cnt = 0;
//			// INA226_ApplicationRead();
//		}	
//		if(beep_cnt > 0)
//		{
//			beep_cnt--;
//			HAL_GPIO_WritePin(BEEP_GPIO_Port,BEEP_Pin,GPIO_PIN_RESET);
//		}
//		else
//		{
//			HAL_GPIO_WritePin(BEEP_GPIO_Port,BEEP_Pin,GPIO_PIN_SET);
//		}
//    }
//}
