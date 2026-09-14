#include <stdio.h>
#include "ui_task.h"
#include "task_manager.h"
#include "basic_task.h"
/* FreeRTOS头文件 */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
/* 其他头文件 */
#include "bsp_sht30.h"
#include "beep.h"
/* ============================================================
 * FreeRTOS对象
 * ============================================================ */
/* 1ms软件定时器 */
static TimerHandle_t s_basic_timer = NULL;
/* 按键事件Queue */
static QueueHandle_t s_key_event_queue = NULL;
//static volatile uint32_t s_basic_timer_cnt = 0;
///* 查询FreeRTOS各个任务剩余的内存 */
//static TickType_t last_memory_check = 0;

/* ============================================================
 * 发送按键事件
 *
 * 注意：
 * FreeRTOS Software Timer Callback 不属于硬件ISR，
 * 因此这里使用普通 xQueueSend()，
 * 不使用 xQueueSendFromISR()。
 * ============================================================ */
static void Basic_KeySendEvent(Basic_KeyId_t key,Basic_KeyEventType_t event)
{
    Basic_KeyEvent_t key_event;

    if (s_key_event_queue == NULL)
    {
        return;
    }

    key_event.key   = key;
    key_event.event = event;

    /* Timer Callback 中禁止长时间阻塞。因此等待时间设置为0。*/
    (void)xQueueSend(s_key_event_queue,&key_event,0);
}
/* ============================================================
 * 放在1ms定时回调函数里的按键扫描函数，将这里的函数改为自己使用的即可
 *
 * 注意：不要进行阻塞耗时操作
 * ============================================================ */
void Basic_KeyScan_1ms(void)
{
	uint8_t i;

    /* 原有按键状态机，每1ms调用一次 */
    KEY_scan();

    /* 检查所有按键产生的事件 */
    for (i = 0; i < KEY_NUM; i++)
    {
        /* 按下事件 */
        if (KEY_check_flag(i, KEY_DOWN))
        {
            Basic_KeySendEvent((KEY_name_t)i,KEY_DOWN);
        }
        /* 松开事件 */
        if (KEY_check_flag(i, KEY_UP))
        {
            Basic_KeySendEvent((KEY_name_t)i,KEY_UP);
        }
        /* 单击事件 */
        if (KEY_check_flag(i, KEY_SINGLE_PRESS))
        {
            Basic_KeySendEvent((KEY_name_t)i,KEY_SINGLE_PRESS);
        }
        /* 双击事件 */
        if (KEY_check_flag(i, KEY_DOUBLE_PRESS))
        {
            Basic_KeySendEvent((KEY_name_t)i,KEY_DOUBLE_PRESS);
        }
        /* 长按事件 */
        if (KEY_check_flag(i, KEY_LONG_PRESS))
        {
            Basic_KeySendEvent((KEY_name_t)i,KEY_LONG_PRESS);
        }
        /* 连续长按事件 */
        if (KEY_check_flag(i, KEY_REPEAT_PRESS))
        {
            Basic_KeySendEvent((KEY_name_t)i,KEY_REPEAT_PRESS);
        }
    }
}
/* ============================================================
 * FreeRTOS 1ms软件定时器回调
 * ============================================================ */
static void Basic_TimerCallback(TimerHandle_t xTimer)
{
    (void)xTimer;
	// s_basic_timer_cnt++;
    /*
     * 保持回调尽可能短。
     */
	beeping();
	beep_play_music(music);
    Basic_KeyScan_1ms();
}

/* ============================================================
 * Basic Task
 *
 * 该函数会覆盖 task_manager.c 中的 __WEAK Basic_Task()
 * ============================================================ */
void Basic_Task(void *pvParameters)
{
	(void)pvParameters;
	
    Basic_KeyEvent_t key_event;
	TickType_t last_monitor_time;
	TickType_t last_SHT30_update_time;	// 每隔一段时间更新温湿度数据
    
    printf("Basic_Task start\r\n");
    /* --------------------------------------------------------
     * 创建按键事件Queue
     *
     * 最多缓存8个事件。
     * -------------------------------------------------------- */
    s_key_event_queue = xQueueCreate(8,sizeof(Basic_KeyEvent_t));
    if (s_key_event_queue == NULL)
    {
        printf("Basic key queue create failed\r\n");
        vTaskDelete(NULL);
    }

    /* --------------------------------------------------------
     * 创建1ms周期软件定时器
     * -------------------------------------------------------- */
    s_basic_timer = xTimerCreate("BasicTimer",pdMS_TO_TICKS(BASIC_SCAN_PERIOD_MS),pdTRUE,NULL,Basic_TimerCallback);
	/* 定时器周期 自动重载  定时器ID */
	/* 创建失败 */
    if (s_basic_timer == NULL)
    {
        printf("Basic timer create failed\r\n");
        vQueueDelete(s_key_event_queue);
        s_key_event_queue = NULL;
        vTaskDelete(NULL);
    }

    /* --------------------------------------------------------
     * 启动软件定时器
     * -------------------------------------------------------- */
    if (xTimerStart(s_basic_timer,pdMS_TO_TICKS(10)/*等待时间*/) != pdPASS)
    {
        printf("Basic timer start failed\r\n");
        xTimerDelete(s_basic_timer,pdMS_TO_TICKS(10));
        vQueueDelete(s_key_event_queue);
        s_basic_timer = NULL;
        s_key_event_queue = NULL;
        vTaskDelete(NULL);
    }
    printf("Basic 1ms timer start OK\r\n");

	last_monitor_time = xTaskGetTickCount();	// 获取当前时间
	last_SHT30_update_time = xTaskGetTickCount();
    /* --------------------------------------------------------
     * Basic Task主循环
     *
     * 平时阻塞，不占CPU。
     * 收到按键事件才运行。
     * -------------------------------------------------------- */
    while (1)
    {
		/* 每隔一段时间执行一次RTOS各个任务内存检查 */
		#if RTOS_MEMORY_MONITOR_SWITCH
        if ((xTaskGetTickCount() - last_monitor_time) >= pdMS_TO_TICKS(RTOS_MEMORY_MONITOR_PERIOD_MS))
        {
            last_monitor_time = xTaskGetTickCount();
            Task_Manager_PrintMemoryInfo();
        }
		#endif
		/* 每隔一段时间更新温湿度传感器数据 */
		if ((xTaskGetTickCount() - last_SHT30_update_time) >= pdMS_TO_TICKS(SHT30DATA_UPDATE_PERIOD_MS))
        {
            last_SHT30_update_time = xTaskGetTickCount();
            SHT30_ReadData(&SHT30_data);	// 读取温湿度数据
        }
		
		/* 通过机械按键执行操作 */
		if (xQueueReceive(s_key_event_queue,&key_event,pdMS_TO_TICKS(1000)) == pdPASS)
        {
            if ((key_event.key == KEY_1) && (key_event.event == KEY_SINGLE_PRESS))
			{
//				HAL_GPIO_TogglePin(LED1_GPIO_Port,LED1_Pin);
				beep(50);
				// printf("KEY1 single press.\r\n");
				UI_NextPage();
			}
        }
		// vTaskDelay(1);
    }
}
