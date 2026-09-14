 #ifndef	_UART_BSP_H_
 #define	_UART_BSP_H_

 #include "main.h"

 // 串口开关
 #define UART1_EN     1          // 串口1，0=关、1=启用;  倘若没用到UART1, 置0，就不会开辟UART1发送缓存、接收缓存，省一点资源;
 #define UART2_EN     0          // 串口2，0=关、1=启用;  同上;
 #define UART3_EN     0          // 串口3，0=关、1=启用;  同上;
 #define UART4_EN     0          // 串口4，0=关、1=启用;  同上;
 #define UART5_EN     0          // 串口5，0=关、1=启用;  同上;
 #define UART6_EN     0          // 串口5，0=关、1=启用;  同上;

 typedef struct	// 声明一个结构体，方便管理变量
 {
     uint16_t  ReceiveNum;        // 接收字节数; 在中断回调函数里被自动赋值; 只要字节数>0，即为接受到新一帧的数据
     uint8_t   ReceiveData[512];  // 接收到的数据
     uint8_t   BuffTemp[512];  
 	// 接收缓存; 注意：这个数组，只是一个缓存，用于DMA逐个字节接收，当接收完一帧后，数据在回调函数中，
 	//会自动转存到 ReceivedData[ ] 存放。即：双缓冲，有效减少单缓冲的接收过程新数据覆盖旧数据
 } xUATR_TypeDef;

 // 声明串口的结构体; 为了方便使用，一次过全声明了
 #if UART1_EN 
 	extern xUATR_TypeDef xUART1 ; /* 定义串口1的数据接收结构体，方便管理变量。*/ 
 #endif
 #if UART2_EN 
 extern xUATR_TypeDef xUART2 ;          // 定义串口2的数据接收结构体，方便管理变量。
 #endif
 #if UART3_EN 
 extern xUATR_TypeDef xUART3 ;          // 定义串口3的数据接收结构体，方便管理变量。
 #endif
 #if UART4_EN 
 extern xUATR_TypeDef xUART4 ;          // 定义串口4的数据接收结构体，方便管理变量。
 #endif
 #if UART5_EN 
 extern xUATR_TypeDef xUART5 ;          // 定义串口5的数据接收结构体，方便管理变量。
 #endif
 #if UART6_EN 
 extern xUATR_TypeDef xUART6 ;          // 定义串口6的数据接收结构体，方便管理变量。
 #endif

void UART_Send_IT(UART_HandleTypeDef *huart,uint8_t * str,uint32_t strlen);
void UART_Send_DMA(UART_HandleTypeDef *huart,uint8_t * str,uint32_t strlen);
void USART_printf(UART_HandleTypeDef *huart, const char *format, ...);

 #endif
