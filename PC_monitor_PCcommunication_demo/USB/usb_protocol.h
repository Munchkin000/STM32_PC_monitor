#ifndef __USB_PROTOCOL_H__
#define __USB_PROTOCOL_H__

#include "main.h"
#include <stdint.h>

/* ============================================================
 * 配置
 * ============================================================ */
/*
 * 一条完整协议指令的最大长度
 *
 * 当前CDC基础接收缓冲为64字节，因此这里也保持较小。
 */
#define USB_PROTOCOL_LINE_SIZE        80U

/*
 * USB CDC顶层协议初始化
 */
void USB_Protocol_Init(void);

/*
 * USB CDC协议轮询处理
 *
 * 裸机工程：
 * 放在while(1)中持续调用
 *
 * 后续FreeRTOS：
 * 可以直接放入USB_Task中调用
 */
void USB_Protocol_Process(void);

#endif
