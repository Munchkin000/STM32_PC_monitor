#include "task_manager.h"
#include <stdio.h>

/*
 * 如果编译器没有定义 __WEAK 关键字宏，这里补充定义 (兼容不同编译器，如 GCC, ARMCC)。
 * 作用：
 * 1. 允许本文件提供默认的、带死循环的空任务函数；
 * 2. 如果用户在工程的其他文件 (如 main.c) 中实现了同名任务函数，链接器会自动覆盖这里的弱定义，而不会报重定义错误。
 */
#ifndef __WEAK
#define __WEAK __attribute__((weak))
#endif

/*
 * 全局任务句柄 (TaskHandle_t)
 * 说明：
 * 1. 任务未创建时，句柄为 NULL。
 * 2. 创建成功后，由 xTaskCreate() 底层分配并写入。
 * 3. 任务管理器在执行删除任务动作后，会负责将其重新置为 NULL。
 */
static TaskHandle_t task_manager_handle = NULL;
static TaskHandle_t basic_task_handle   = NULL;
static TaskHandle_t ui_task_handle    	= NULL;
static TaskHandle_t usb_task_handle     = NULL;

/*
 * 任务动作缓存数组和任务状态记录数组
 * 说明：
 * 1. 使用 volatile 修饰是因为这些变量会在中断/其他任务 (写入) 和 Task_Manager 任务 (读取) 之间并发访问。
 * 2. g_task_action[] 由外部通过 Taskmanager_Ctrl() 写入请求动作；
 * 3. Task_Manager 任务被唤醒后，会扫描 g_task_action[] 并执行对应动作，执行完清空。
 * 4. g_task_state[] 由 Task_Manager 维护，记录当前任务是 NULL/RUNNING/STOP。
 */
static volatile task_action_t g_task_action[TASK_ID_MAX];
static volatile task_state_t  g_task_state[TASK_ID_MAX];

/*
 * 任务管理器专属的同步二值信号量
 * 说明：
 * 1. 外部调用 Taskmanager_Ctrl() 请求某个操作后，会释放 (Give) 该信号量；
 * 2. Task_Manager 守护任务平时阻塞 (Take) 等待此信号量，被唤醒后统一集中处理任务的创建、挂起、恢复、删除，
 *    避免多任务直接交叉创建/删除造成的竞态条件或内存碎片。
 */
static SemaphoreHandle_t task_manager_sem = NULL;

/*
 * 任务静态注册表
 * 注意：
 * 1. 数组下标的宏 (如 TASK_ID_BASIC) 必须与 task_manager.h 中 task_id_t 枚举顺序/数值完全一致。
 * 2. 这里的 Task_Registry_t 结构体假定已在其他包含的头文件中定义。
 *    包含了：任务入口函数、任务名称字符串、栈大小、优先级、对应句柄的指针。
 */
static const Task_Registry_t Task_List[TASK_ID_MAX] =
{
    [TASK_ID_BASIC] =
    {
        Basic_Task,             /* 任务入口函数指针 */
        "Basic_Task",           /* 任务名 */
        BASIC_TASK_STACK_SIZE,  /* 栈深度 (字) */
        BASIC_TASK_PRIO,        /* 任务优先级 */
        &basic_task_handle      /* 接收创建后句柄的指针的地址 */
    },

    [TASK_ID_UI] =
    {
        UI_task,
        "UI_Task",
        UI_TASK_STACK_SIZE,
        UI_TASK_PRIO,
        &ui_task_handle
    },
	
    [TASK_ID_USB] =
    {
        USB_Task,
        "USB_Task",
        USB_TASK_STACK_SIZE,
        USB_TASK_PRIO,
        &usb_task_handle
    },
};

/*
 * @brief  初始化任务管理器核心
 * @return BaseType_t 返回 pdPASS(成功) 或 pdFAIL(失败)
 */
BaseType_t Task_Manager_Init(void)
{
    BaseType_t ret;

    /* 1. 初始化全部任务的待执行动作和初始状态为"无"和"不存在" */
    for (uint8_t i = 0; i < TASK_ID_MAX; i++)
    {
        g_task_action[i] = TASK_ACT_NONE;
        g_task_state[i]  = TASK_STATE_NULL;
    }

	/* 2. 创建用于触发任务管理器的二值信号量 */
    task_manager_sem = xSemaphoreCreateBinary();

    if (task_manager_sem == NULL)
    {
        printf("Task manager semaphore create failed\r\n");
        return pdFAIL;
    }
    
	/* 3. 创建后台守护任务 Task_Manager */
    ret = xTaskCreate(Task_Manager,             /* 任务函数主体 */
                      "Task_Manager",           /* 调试用任务名 */
                      TASK_MANAGER_STACK_SIZE,  /* 任务栈深度 */
                      NULL,                     /* 传递给任务的参数 */
                      TASK_MANAGER_PRIO,        /* 任务优先级 */
                      &task_manager_handle);    /* 传回任务句柄 */

    if (ret != pdPASS)
    {
        printf("Task_Manager create failed\r\n");
        return pdFAIL;
    }

    printf("Task_Manager init OK\r\n");

    return pdPASS;
}

/*
 * @brief  任务控制函数 (向管理器发送调度请求)
 */
BaseType_t Taskmanager_Ctrl(task_id_t task_id, task_action_t task_action, uint8_t is_from_isr)
{
    /* 边界检查：防止传入非法的任务 ID 导致数组越界 */
    if (task_id >= TASK_ID_MAX)
    {
        return pdFAIL;
    }
    /* 检查任务管理器是否已完成初始化 (信号量是否创建) */
    if (task_manager_sem == NULL)
    {
        return pdFAIL;
    }

    /* 将请求的动作登记到缓存数组中，由 Task_Manager 去异步执行 */
    g_task_action[task_id] = task_action;

    /* 根据调用环境，调用不同的 FreeRTOS API 释放信号量唤醒 Task_Manager */
    if (is_from_isr)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        /* 在中断中释放信号量 */
        xSemaphoreGiveFromISR(task_manager_sem, &xHigherPriorityTaskWoken);
        /* 如果唤醒的任务优先级比当前被中断的上下文高，则请求一次上下文切换 */
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

        return pdPASS;
    }
    else
    {
        /* 在普通任务中释放信号量 */
        return xSemaphoreGive(task_manager_sem);
    }
}

/*
 * @brief  获取指定 ID 的任务的底层 FreeRTOS 句柄
 */
TaskHandle_t Task_Manager_GetHandle(task_id_t task_id)
{
    if (task_id >= TASK_ID_MAX)
    {
        return NULL;
    }

    /* 从静态注册表解引用获取真正的 task handle (可能为 NULL) */
    return *(Task_List[task_id].handle_ptr);
}

/*
 * @brief  获取指定 ID 任务在管理器中登记的业务状态
 */
task_state_t Task_Manager_GetState(task_id_t task_id)
{
    if (task_id >= TASK_ID_MAX)
    {
        return TASK_STATE_NULL;
    }

    return g_task_state[task_id];
}

/*
 * ============================================================
 * FreeRTOS运行时内存监控
 *
 * Stack High Water Mark:
 * 表示任务运行以来栈空间最少剩余过多少个word。
 *
 * STM32F103 Cortex-M3：
 *
 * 1 StackType_t = 4 Bytes
 *
 * HighWaterMark越小，说明任务越接近栈耗尽。
 * ============================================================
 */
void Task_Manager_PrintMemoryInfo(void)
{
    uint8_t i;

    UBaseType_t hwm;
    uint32_t total_words;
    uint32_t used_words;
	uint32_t stack_usage;

    printf("\r\n");
    printf("========== FreeRTOS Memory Monitor ==========\r\n");
    /* ========================================================
     * Basic / UI / USB任务
     * ======================================================== */
    for (i = 0; i < TASK_ID_MAX; i++)
    {
        TaskHandle_t handle;
        handle = *(Task_List[i].handle_ptr);

        /*
         * 任务还没有创建则跳过
         */
        if (handle == NULL)
        {
            printf("%-12s : NOT CREATED\r\n",
                   Task_List[i].task_name);

            continue;
        }
        /*
         * 获取历史最小剩余栈
         *
         * 单位：word
         */
        hwm = uxTaskGetStackHighWaterMark(handle);
        total_words = Task_List[i].stack_size;
        /*
         * 防止异常值造成无符号减法溢出
         */
        if (hwm <= total_words)
        {
            used_words = total_words - hwm;
        }
        else
        {
            used_words = 0;
        }
		/*
         * 计算栈的使用率
         */
		if (total_words > 0)
		{
			stack_usage = used_words * 100UL / total_words;
		}
		else
		{
			stack_usage = 0;
		}
		/* 打印信息 */
        printf("%-12s : ""stack=%4lu W, ""max_used=%4lu W, ""min_free=%4lu W, ""usage=%3lu%%\r\n",
			Task_List[i].task_name,(unsigned long)total_words,(unsigned long)used_words,(unsigned long)hwm,(unsigned long)stack_usage);
    }

    /* ========================================================
     * Task_Manager自身
     *
     * 它不在Task_List[]中，所以单独统计。
     * ======================================================== */
    if (task_manager_handle != NULL)
    {
        hwm = uxTaskGetStackHighWaterMark(task_manager_handle);
        total_words = TASK_MANAGER_STACK_SIZE;

        if (hwm <= total_words)
        {
            used_words = total_words - hwm;
        }
        else
        {
            used_words = 0;
        }
		/*
         * 计算栈的使用率
         */
		if (total_words > 0)
		{
			stack_usage = used_words * 100UL / total_words;
		}
		else
		{
			stack_usage = 0;
		}
		/* 打印信息 */
        printf("%-12s : "
			   "stack=%4lu W, "
			   "max_used=%4lu W, "
			   "min_free=%4lu W, "
			   "usage=%3lu%%\r\n",

			   "Task_Manager",

			   (unsigned long)total_words,
			   (unsigned long)used_words,
			   (unsigned long)hwm,
			   (unsigned long)stack_usage);
    }

    /* ========================================================
     * FreeRTOS Heap
     * ======================================================== */
    printf("---------------------------------------------\r\n");

    printf("Heap Current Free : %lu Bytes\r\n",
           (unsigned long)xPortGetFreeHeapSize());

    printf("Heap Minimum Free : %lu Bytes\r\n",
           (unsigned long)xPortGetMinimumEverFreeHeapSize());

    printf("=============================================\r\n");
}

/*
 * @brief  任务管理器主守护任务
 * @note   以最高优先级 (TASK_MANAGER_PRIO) 在后台运行，确保任务调度指令能被第一时间处理
 */
void Task_Manager(void *pvParameters)
{
    (void)pvParameters;

    while (1)
    {
        /*
         * 阻塞等待任务控制命令。
         * 外部调用 Taskmanager_Ctrl() 发送指令并 Give 信号量后，这里会被立即唤醒。
         * portMAX_DELAY 表示死等，不消耗 CPU 时间片。
         */
        xSemaphoreTake(task_manager_sem, portMAX_DELAY);

        /*
         * 唤醒后，遍历整个任务注册表，检查每个任务是否有待执行的操作动作。
         */
        for (uint8_t i = 0; i < TASK_ID_MAX; i++)
        {
            task_action_t current_action;
            TaskHandle_t *pHandle;

            /* 获取当前遍历到的任务动作和句柄存放地址 */
            current_action = g_task_action[i];
            pHandle = Task_List[i].handle_ptr;

            /* 根据缓存的操作命令分发处理 */
            switch (current_action)
            {
                case TASK_ACT_CREATE:   /* 处理：创建任务请求 */
                {
                    /* 防错：如果句柄为空，说明任务确实未创建，才执行创建 */
                    if (*pHandle == NULL)
                    {
                        BaseType_t ret;

                        /* 依据静态注册表 Task_List 提供的信息调用底层 API 创建任务 */					
						printf("[HEAP] Before %-12s: free=%u, min=%u\r\n",
							   Task_List[i].task_name,
							   (unsigned int)xPortGetFreeHeapSize(),
							   (unsigned int)xPortGetMinimumEverFreeHeapSize());

						ret = xTaskCreate(Task_List[i].task_func,Task_List[i].task_name,Task_List[i].stack_size,NULL,
										  Task_List[i].priority,pHandle);

						if (ret == pdPASS)
						{
							printf("%s create OK\r\n", Task_List[i].task_name);

							printf("[HEAP] After  %-12s: free=%u, min=%u\r\n",
								   Task_List[i].task_name,
								   (unsigned int)xPortGetFreeHeapSize(),
								   (unsigned int)xPortGetMinimumEverFreeHeapSize());

							g_task_state[i] = TASK_STATE_RUNNING;
						}
						else
						{
							printf("%s create failed\r\n", Task_List[i].task_name);
							g_task_state[i] = TASK_STATE_NULL;
						}
                    }
                    else
                    {
                        /* 如果任务句柄已存在，直接修正记录状态，不重复创建 */
                        g_task_state[i] = TASK_STATE_RUNNING;
                    }

                    /* 动作执行完毕，清除标记 */
                    g_task_action[i] = TASK_ACT_NONE;
                    break;
                }

                case TASK_ACT_SUSPEND:  /* 处理：挂起任务请求 */
                {
                    /* 仅当任务有效时执行挂起操作 */
                    if (*pHandle != NULL)
                    {
                        vTaskSuspend(*pHandle);             /* 调用底层 API 暂停调度该任务 */
                        g_task_state[i] = TASK_STATE_STOP;  /* 更新内部状态标记为已停止 */
                        printf("%s suspend OK\r\n", Task_List[i].task_name);
                    }

                    g_task_action[i] = TASK_ACT_NONE;
                    break;
                }

                case TASK_ACT_RESUME:   /* 处理：恢复任务请求 */
                {
                    /* 仅当任务有效时执行恢复操作 */
                    if (*pHandle != NULL)
                    {
                        vTaskResume(*pHandle);                  /* 调用底层 API 恢复该任务进入就绪态 */
                        g_task_state[i] = TASK_STATE_RUNNING;   /* 更新内部状态标记为运行中 */
                        printf("%s resume OK\r\n", Task_List[i].task_name);
                    }

                    g_task_action[i] = TASK_ACT_NONE;
                    break;
                }

                case TASK_ACT_DELETE:   /* 处理：删除任务请求 */
                {
                    /* 仅当任务有效时执行删除操作 */
                    if (*pHandle != NULL)
                    {
                        vTaskDelete(*pHandle);  /* 底层 API：杀死目标任务，回收栈与控制块资源 */
                        *pHandle = NULL;        /* 手动清空句柄指针，防止野指针误用，并供下一次 CREATE 检查 */

                        g_task_state[i] = TASK_STATE_NULL; /* 更新内部状态标记为不存在 */
                        printf("%s delete OK\r\n", Task_List[i].task_name);
                    }

                    g_task_action[i] = TASK_ACT_NONE;
                    break;
                }

                default:    /* 如果是 TASK_ACT_NONE 或其他非法值，直接跳过 */
                {
                    break;
                }
            }
        }
    }
}

/*
 * 默认基础任务 (弱定义)
 * 如果用户工程没有在其它地方实现 Basic_Task()，编译器会默认使用这里的函数体。
 */
__WEAK void Basic_Task(void *pvParameters)
{
    (void)pvParameters; /* 消除未使用参数的编译警告 */

    while (1)
    {
//        printf("Basic_Task running\r\n");
        /* 基础任务默认每 1000 毫秒(1秒)执行一次调度延时 */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
/*
 * 默认 Ui显示 任务 (弱定义)
 * 如果用户工程已经有真实的 UI_task()，会覆盖这里的弱定义。
 */
__WEAK void UI_task(void *pvParameters)
{
    (void)pvParameters;

    while (1)
    {
        /* UI 任务通常刷新率较高，默认给予 5 毫秒阻塞延时释放 CPU */
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
/*
 * 默认 USB 任务 (弱定义)
 */
__WEAK void USB_Task(void *pvParameters)
{
    (void)pvParameters;

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

