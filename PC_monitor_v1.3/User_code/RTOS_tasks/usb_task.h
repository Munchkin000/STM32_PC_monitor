#ifndef __USB_TASK_H__
#define __USB_TASK_H__

#include "main.h"
#include <stdint.h>
#include <stdio.h>

/* ============================================================
 * 放在RTOS工程里使用，进行与上位机的通信
 * ============================================================ */
void USB_Task(void *pvParameters);

#endif
