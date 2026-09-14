#ifndef __BASIC_TASK_H__
#define __BASIC_TASK_H__

#include "main.h"
#include "FreeRTOS.h"
#include <stdint.h>
#include "bsp_key.h"

/* ============================================================
 * Basic Task 配置
 * ============================================================ */
/* 软件定时器扫描周期 */
#define BASIC_SCAN_PERIOD_MS          1U
/* RTOS任务内存使用监测信息打印周期 */
#define RTOS_MEMORY_MONITOR_PERIOD_MS    60000U
/* RTOS任务内存检测开关 0：不检测 1：打印检测信息*/
#define RTOS_MEMORY_MONITOR_SWITCH    1
/* 更新温湿度传感器数据周期 */
#define SHT30DATA_UPDATE_PERIOD_MS    3000U

#define	Basic_KeyId_t			KEY_name_t
#define Basic_KeyEventType_t	KEY_status_t

/* ============================================================
 * 按键事件数据
 * ============================================================ */
typedef struct
{
    Basic_KeyId_t        key;
    Basic_KeyEventType_t event;
} Basic_KeyEvent_t;

/* ============================================================
 * 对外接口
 * ============================================================ */
/*
 * Basic Task
 * 由现有 Task_Manager 创建。
 */
void Basic_Task(void *pvParameters);

/*
 * 获取当前按键稳定状态
 *
 * 返回：
 * 0：未按下
 * 1：按下
 */
uint8_t Basic_KeyIsPressed(Basic_KeyId_t key);

#endif
