#include "usb_protocol.h"
#include "usb_task.h"
/* 包含必要的FreeRTOS 头文件 */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* ============================================================
 * 放在RTOS工程里使用，进行与上位机的通信
 * ============================================================ */
void USB_Task(void *pvParameters)
{
    (void)pvParameters;

    while (1)
    {
        /* 与上位机通信更改UI内容 */
        USB_Protocol_Process();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

