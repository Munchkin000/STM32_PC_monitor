#ifndef __UI_COMMON_H__
#define __UI_COMMON_H__

#include "OLED.h"
#include <stdint.h>

/*
 * 显示无前导0整数
 *
 * 返回显示字符数量。
 */
uint8_t UI_DrawUInt(int16_t x,int16_t y,uint16_t value,uint8_t font_size);

/*
 * 显示百分比
 * 例如：
 *
 * 68%
 */
void UI_DrawPercent(int16_t x,int16_t y,uint8_t value,uint8_t font_size);

/*
 * 显示百分比进度条
 */
void UI_DrawProgressBar(int16_t x,int16_t y,uint8_t width,uint8_t height,uint8_t percent);

#endif
