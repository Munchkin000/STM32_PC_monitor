#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>

/*
 * FreeRTOS运行时间统计计数器
 *
 * 说明：
 * 1. 该变量被 portGET_RUN_TIME_COUNTER_VALUE() 读取；
 * 2. 当前先使用 LPTIM4 1ms 中断递增；
 * 3. 统计精度为 1ms，适合初步验证任务监控页面；
 * 4. 后续如需更高精度，可改用 TIM2/TIM5 32位定时器。
 */
volatile uint32_t FreeRTOSRunTimeTicks = 0;

/*
 * @brief FreeRTOS运行时间统计初始化函数
 *
 * @note FreeRTOS内核在启动调度器时会调用
 *       portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()，
 *       也就是调用本函数。
 */
void ConfigureTimeForRunTimeStats(void)
{
    FreeRTOSRunTimeTicks = 0;
}

/*
 * @brief 运行时间统计计数递增函数
 *
 * @note 在 1ms 定时中断中调用。
 */
void FreeRTOS_RunTimeStatsTick(void)
{
    FreeRTOSRunTimeTicks++;
}
