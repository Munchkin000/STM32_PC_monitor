#ifndef __UI_DATA_H__
#define __UI_DATA_H__

#include <stdint.h>

/*
 * PC运行状态数据
 *
 * 当前裸机调试阶段：
 * 使用固定测试值
 *
 * 后续FreeRTOS工程：
 * 由USB_Task接收上位机数据并更新
 */
typedef struct
{
    uint8_t cpu_usage;      /* CPU使用率 0~100 % */
    uint8_t ram_usage;      /* RAM使用率 0~100 % */
    int16_t cpu_temp;       /* CPU温度 ℃ */

    uint8_t gpu_usage;      /* GPU使用率 0~100 % */
    uint8_t vram_usage;     /* 显存使用率 0~100 % */
    int16_t gpu_temp;       /* GPU温度 ℃ */
	
	uint16_t rtc_year;      	/* rtc时钟时间年份 */
    uint8_t rtc_month;      /* rtc时钟时间月份 */
    uint8_t rtc_day;      	/* rtc时钟时间日期 */
	
	uint8_t rtc_hour;      	/* rtc时钟时间小时 */
    uint8_t rtc_minute;     /* rtc时钟时间分钟 */
    uint8_t rtc_second;     /* rtc时钟时间秒 */
	
	uint8_t env_temp;      	/* 环境温度 */
    uint8_t env_humid;      /* 环境湿度 */

} UI_PC_Status_t;

#endif
