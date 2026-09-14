#include "bsp_led.h"

/*****************************************************************************
***@breif	打开某个LED
***@param	LED：LED编号
***@retval	无 
*******************************************************************************/
void led_on(uint8_t LED)
{
	switch(LED)
	{
		case LED1:HAL_GPIO_WritePin(LED1_GPIO_Port,LED1_Pin,LED_ON_STATE);break;
//		case LED2:HAL_GPIO_WritePin(LED2_GPIO_Port,LED2_Pin,LED_ON_STATE);break;
		default:break;
	}
}
/*****************************************************************************
***@breif	关闭某个LED
***@param	LED：LED编号
***@retval	无 
*******************************************************************************/
void led_off(uint8_t LED)
{
	switch(LED)
	{
		case LED1:HAL_GPIO_WritePin(LED1_GPIO_Port,LED1_Pin,LED_OFF_STATE);break;
//		case LED2:HAL_GPIO_WritePin(LED2_GPIO_Port,LED2_Pin,LED_OFF_STATE);break;
		default:break;
	}
}
/*****************************************************************************
***@breif	反转某个LED电平
***@param	LED：LED编号
***@retval	无 
*******************************************************************************/
void led_toggle(uint8_t LED)
{
	switch(LED)
	{
		case LED1:HAL_GPIO_TogglePin(LED1_GPIO_Port,LED1_Pin);break;
//		case LED2:HAL_GPIO_TogglePin(LED2_GPIO_Port,LED2_Pin);break;
		default:break;
	}
}
/*****************************************************************************
***@breif	打开所有LED
***@param	无
***@retval	无 
*******************************************************************************/
void led_on_all()
{
	HAL_GPIO_WritePin(LED1_GPIO_Port,LED1_Pin,LED_ON_STATE);
//	HAL_GPIO_WritePin(LED2_GPIO_Port,LED2_Pin,LED_ON_STATE);
}
/*****************************************************************************
***@breif	关闭所有LED
***@param	无
***@retval	无 
*******************************************************************************/
void led_off_all()
{
	HAL_GPIO_WritePin(LED1_GPIO_Port,LED1_Pin,LED_OFF_STATE);
//	HAL_GPIO_WritePin(LED2_GPIO_Port,LED2_Pin,LED_OFF_STATE);
}
/*****************************************************************************
***@breif	反转所有LED
***@param	无
***@retval	无 
*******************************************************************************/
void led_toggle_all()
{
	HAL_GPIO_TogglePin(LED1_GPIO_Port,LED1_Pin);
//	HAL_GPIO_TogglePin(LED2_GPIO_Port,LED2_Pin);
}
