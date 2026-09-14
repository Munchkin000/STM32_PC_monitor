#include "main.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "task_manager.h"
#include "start_task.h"

TaskHandle_t start_task_handle;

/* 开启FreeRTOS任务 */
void start_task(void *pvParameters)
{
    (void)pvParameters;

    printf("start_task running\r\n");

    /*
     * 初始化任务管理器。
     * 任务管理器本身也是一个 FreeRTOS 任务。
     */
    if (Task_Manager_Init() != pdPASS)
    {
        printf("Task_Manager_Init failed\r\n");
        vTaskDelete(NULL);
    }

    /*
     * 创建基础任务。
     * 如果暂时不用，可以注释掉。
     */
    Taskmanager_Ctrl(TASK_ID_BASIC,TASK_ACT_CREATE, TASK_FROM_THREAD);
    /* 创建UI界面显示任务。*/
    Taskmanager_Ctrl(TASK_ID_UI,TASK_ACT_CREATE, TASK_FROM_THREAD);
	 /*
     * 创建USB与上位机通信任务。
     */
	Taskmanager_Ctrl(TASK_ID_USB,TASK_ACT_CREATE, TASK_FROM_THREAD);
    /*
     * 根据需要创建其他任务。
     */
    // Taskmanager_Ctrl(TASK_ID_FATFS, TASK_ACT_CREATE, TASK_FROM_THREAD);

    /* start_task 只负责启动系统任务，完成后删除自身。*/
    vTaskDelete(NULL);
}

void freertos_start(void)
{
    BaseType_t ret;
	/* 创建启动任务 */
    ret = xTaskCreate(start_task,
                      "start_task",
                      START_TASK_STACK,
                      NULL,
                      START_TASK_PRIORITY,
                      &start_task_handle);

    if (ret != pdPASS)
    {
        printf("start_task create failed\r\n");
        return;
    }
	/* 正式启动 FreeRTOS 调度器，开始运行任务 */
    vTaskStartScheduler();
    /*
     * 正常情况下不会运行到这里。
     * 如果运行到这里，通常是 FreeRTOS heap 不够。
     */
    printf("vTaskStartScheduler failed\r\n");

    while (1)
    {
		
    }
}
