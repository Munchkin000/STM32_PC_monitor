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

} UI_PC_Status_t;

#endif
