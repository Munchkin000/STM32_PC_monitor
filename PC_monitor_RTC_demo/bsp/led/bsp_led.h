#ifndef _BSP_LED_H__
#define _BSP_LED_H__

#include "main.h"

/* LED开启关闭电平设置 */
#define LED_ON_STATE 		GPIO_PIN_RESET
#define LED_OFF_STATE 		GPIO_PIN_SET

//LED编号
typedef enum
{
	LED1=0,				//LED1
	LED2,				//LED2
	LED_ALL,			//所有LED
}LED_NUM_t;

/* 开灯 */
void led_on(uint8_t LED);
/* 关灯 */
void led_off(uint8_t LED);
/* 反转灯电平 */
void led_toggle(uint8_t LED);
/* 开所有灯 */
void led_on_all(void);
/* 关所有灯 */
void led_off_all(void);

#endif
