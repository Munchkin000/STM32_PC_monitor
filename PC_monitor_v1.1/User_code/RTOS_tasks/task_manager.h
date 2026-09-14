#ifndef __TASK_MANAGER_H__
#define __TASK_MANAGER_H__
/* 包含必要的系统和 FreeRTOS 头文件 */
#include "main.h"
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

/*
 * 各任务的栈大小和优先级宏定义
 * 注意：
 * 1. FreeRTOS 中栈大小的单位是字 (word)，不是字节 (byte)。
 * 2. 在 Cortex-M7 (32位) 架构下，1 word = 4 bytes。
 *    例如 512 意味着分配 512 * 4 = 2048 字节的栈空间。
 * 3. 优先级数字越大，优先级越高 (与 uCOS 相反)。
 */
#define TASK_MANAGER_STACK_SIZE     256     /* 任务管理器自身的栈大小 */
#define TASK_MANAGER_PRIO           7       /* 任务管理器拥有极高的优先级，确保控制命令能被立即响应 */

#define BASIC_TASK_STACK_SIZE       192
#define BASIC_TASK_PRIO             2       /* 基础任务优先级较低 */

#define UI_TASK_STACK_SIZE        	256    /* UI 刷新通常需要较大栈空间 */
#define UI_TASK_PRIO              	3

#define USB_TASK_STACK_SIZE         256
#define USB_TASK_PRIO               4

/*
 * 任务编号枚举
 * 说明：
 * 1. 这里的编号用于作为索引，传递给 Taskmanager_Ctrl() 以控制指定的任务。
 * 2. 后续增加新任务时，只需要在这里增加 ID，并在 task_manager.c 的任务注册表 (Task_List) 中补充对应的配置即可。
 * 3. 注意：实际 task_manager.c 中使用了 TASK_ID_UI，请确保枚举定义与 c 文件中的调用一致。
 */
typedef enum
{
    TASK_ID_BASIC = 0,      /* 基础后台任务，例如LED闪烁、状态打印、按键扫描等 */
    TASK_ID_UI,           	/* UI界面任务 (注：对应 task_manager.c 中的 UI_task) */
    TASK_ID_USB,            /* USB通信或挂载任务，可选 */
    TASK_ID_MAX             /* 任务总数上限，用于数组边界安全校验 */
} task_id_t;

/*
 * 任务控制动作枚举
 * 说明：
 * 用于告诉任务管理器 (Task_Manager) 下一步对目标任务执行什么操作。
 * TASK_ACT_CREATE  ：创建任务并分配内存/句柄
 * TASK_ACT_SUSPEND ：挂起任务 (暂停运行)
 * TASK_ACT_RESUME  ：恢复任务 (从挂起状态恢复运行)
 * TASK_ACT_DELETE  ：删除任务并释放对应的栈和 TCB 内存
 */
typedef enum
{
    TASK_ACT_NONE    = 0,   /* 无动作 (空闲状态) */
    TASK_ACT_CREATE  = 1,   /* 请求创建任务 */
    TASK_ACT_SUSPEND = 3,   /* 请求挂起任务 */
    TASK_ACT_RESUME  = 5,   /* 请求恢复任务 */
    TASK_ACT_DELETE  = 6    /* 请求删除任务 */
} task_action_t;
/*
 * 任务注册表结构体
 * 说明：
 * 每一个需要被任务管理器控制的任务，都在这里注册。
 */
typedef struct
{
    TaskFunction_t      task_func;      /* 任务函数 */
    const char         *task_name;      /* 任务名称 */
    configSTACK_DEPTH_TYPE stack_size;  /* 任务栈大小，单位 word */
    UBaseType_t         priority;       /* 任务优先级 */
    TaskHandle_t       *handle_ptr;     /* 任务句柄指针 */
} Task_Registry_t;
/*
 * 任务当前状态枚举
 * 说明：
 * 这是任务管理器内部记录的自定义业务状态，不等同于 FreeRTOS 原生的 eTaskState (如 eRunning, eSuspended 等)。
 * 主要用于逻辑防错，例如防止重复创建、重复删除等。
 */
typedef enum
{
    TASK_STATE_NULL = 0,    /* 任务不存在 (未创建或已被删除) */
    TASK_STATE_RUNNING,     /* 任务已创建且正在运行或处于就绪队列 */
    TASK_STATE_STOP         /* 任务已被挂起 (Suspend) */
} task_state_t;

/*
 * 控制命令来源宏定义
 * 用于 Taskmanager_Ctrl() 的 is_from_isr 参数，区分调用环境以使用正确的 FreeRTOS API。
 */
#define TASK_FROM_THREAD    0   /* 从普通任务/线程中调用 */
#define TASK_FROM_ISR       1   /* 从中断服务函数(ISR)中调用 */

/*
 * @brief 任务管理器初始化
 * 作用：
 * 1. 初始化全局状态/动作数组；
 * 2. 创建任务管理器用于阻塞等待命令的二值信号量；
 * 3. 创建核心守护任务 (Task_Manager 任务本身)。
 * @return BaseType_t pdPASS 表示成功，pdFAIL 表示失败
 */
BaseType_t Task_Manager_Init(void);

/*
 * @brief 任务管理器主任务 (守护任务)
 * 说明：一般不需要用户直接调用，由 Task_Manager_Init() 内部自动通过 xTaskCreate 创建。
 * 它会在后台死循环等待信号量，一旦收到请求，就统筹执行其他任务的创建、删除等动作。
 */
void Task_Manager(void *pvParameters);

/*
 * @brief 任务控制函数 (向任务管理器发送控制指令)
 * 参数：
 * @param task_id     ：要控制的目标任务编号，使用 task_id_t 枚举
 * @param task_action ：要执行的任务动作，使用 task_action_t 枚举
 * @param is_from_isr ：调用上下文环境，0=普通任务中调用(TASK_FROM_THREAD)，1=中断中调用(TASK_FROM_ISR)
 * @return BaseType_t pdPASS 表示指令下发成功，pdFAIL 表示参数错误或信号量未就绪
 */
BaseType_t Taskmanager_Ctrl(task_id_t task_id, task_action_t task_action, uint8_t is_from_isr);

/*
 * 获取任务句柄和任务状态的接口函数
 */
TaskHandle_t Task_Manager_GetHandle(task_id_t task_id); /* 获取底层 FreeRTOS 任务句柄 */
task_state_t Task_Manager_GetState(task_id_t task_id);  /* 获取管理器记录的当前业务状态 */

/*
 * 打印FreeRTOS运行时内存状态
 *
 * 包括：
 * 1. Basic/UI/USB任务Stack High Water Mark
 * 2. Task_Manager自身Stack High Water Mark
 * 3. 当前FreeRTOS Heap剩余
 * 4. 历史最低FreeRTOS Heap
 *
 * 注意：
 * 不建议高频调用，调试阶段每10~30秒调用一次即可。
 */
void Task_Manager_PrintMemoryInfo(void);

/*
 * 需要由用户工程提供的具体任务处理函数声明
 * 说明：
 * 如果某些任务暂时不用，可以不通过 Taskmanager_Ctrl() 去创建。
 * task_manager.c 中提供了带有 __WEAK 的默认空实现，用户在其他文件重写即可覆盖。
 */
void Basic_Task(void *pvParameters);
void UI_task(void *pvParameters);
void USB_Task(void *pvParameters);

#endif
