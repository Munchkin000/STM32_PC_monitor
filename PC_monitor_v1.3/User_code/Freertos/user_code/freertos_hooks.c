#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include <stdio.h>

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    printf("FreeRTOS stack overflow: %s\r\n", pcTaskName);

    taskDISABLE_INTERRUPTS();

    while (1)
    {
    }
}
/*
 * 当 configSUPPORT_STATIC_ALLOCATION = 1 时，
 * FreeRTOS 需要用户提供 Idle Task 的 TCB 和 Stack。
 */
#if (configSUPPORT_STATIC_ALLOCATION == 1)

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];

    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

#if (configUSE_TIMERS == 1)

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize)
{
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

#endif

#endif

void vApplicationMallocFailedHook(void)
{
    printf("\r\n[FreeRTOS] MALLOC FAILED!\r\n");

    taskDISABLE_INTERRUPTS();

    while (1)
    {
        /* FreeRTOS动态内存申请失败 */
    }
}
