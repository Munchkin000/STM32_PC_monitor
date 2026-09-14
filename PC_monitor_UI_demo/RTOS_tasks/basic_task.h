#ifndef __BASIC_TASK_H__
#define __BASIC_TASK_H__

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include <stdint.h>
#include "bsp_key.h"

/* ============================================================
 * Basic Task 配置
 * ============================================================ */
/* 软件定时器扫描周期 */
#define BASIC_SCAN_PERIOD_MS          1U

///* ============================================================
// * 按键编号
// * ============================================================ */
//typedef enum
//{
//    BASIC_KEY_0 = 0,
//    BASIC_KEY_1,

//    BASIC_KEY_MAX
//} Basic_KeyId_t;
#define	Basic_KeyId_t			KEY_name_t
#define Basic_KeyEventType_t	KEY_status_t
///* ============================================================
// * 按键事件
// * ============================================================ */
//typedef enum
//{
//    BASIC_KEY_EVENT_NONE = 0,

//    BASIC_KEY_EVENT_PRESS,        /* 按下 */
//    BASIC_KEY_EVENT_RELEASE,      /* 松开 */
//    BASIC_KEY_EVENT_LONG_PRESS    /* 长按 */

//} Basic_KeyEventType_t;


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
