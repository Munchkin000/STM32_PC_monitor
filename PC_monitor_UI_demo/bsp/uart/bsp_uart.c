 #include "bsp_uart.h"
 //#include "bsp_esp8266.h" // 在 bsp_uart.c 顶部添加此头文件，以识别 strEsp8266_Fram_Record
 #include <stdio.h>
 #include <string.h>
 #include <stdarg.h>
 #include "usart.h"

 // 定义串口的结构体; 为了方便使用，一次过全定义了
 xUATR_TypeDef xUART1 = {0};          // 定义结构体，用于UART1。也可以不用结构体，用单独的变量
 xUATR_TypeDef xUART2 = {0};          // 定义结构体，用于UART2。也可以不用结构体，用单独的变量
 xUATR_TypeDef xUART3 = {0};          // 定义结构体，用于UART3。也可以不用结构体，用单独的变量
 xUATR_TypeDef xUART4 = {0};          // 定义结构体，用于UART4。也可以不用结构体，用单独的变量
 xUATR_TypeDef xUART5 = {0};          // 定义结构体，用于UART5。也可以不用结构体，用单独的变量
 xUATR_TypeDef xUART6 = {0};          // 定义结构体，用于UART6。也可以不用结构体，用单独的变量

 /******************************************************************************
  * 函  数： fputc
  * 功  能： 使printf的输出由UART1实现
  * 参  数： 
  * 返回值： 无
  * 备  注： 1.注意，不能使用HAL_UART_Transmit_IT(), 机制上会冲突; 因为调用中断发送函数后，如果上次发送还在进行，就会直接返回！
  *			它不会继续等待，也不会数据填入队列排队发送
  *			2.使用HAL_UART_Transmit，相等于USART1->DR = ch, 函数内部加了简单的超时判断(ms)，防止卡死
 ******************************************************************************/
 int fputc(int ch, FILE *f)                                 
 {
     HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);   
     return ch;
 }
 /**
 * @brief UART2 接收到一帧数据的弱回调函数
 * @note  这是一个弱定义(__weak)，具体的外设驱动（如 ESP8266）可以在自己的文件里重写它
 */
__weak void UART2_RxEvent_Callback(uint8_t *pData, uint16_t Size)
{
    // 默认空实现，如果外面没人重写，就什么都不做
}
 /******************************************************************************
  * 函  数： UART_Send_IT
  * 功  能： 利用中断发送，非阻塞式，大大减少资源占用
  * 参  数： UART_HandleTypeDef *huart:要发送的串口句柄
  * 参  数： uint8_t * str：要发送的数组
  * 参  数：	uint32_t strlen：发送的字节数
  * 返回值： 无
  * 备  注： 当上次的调用还没完成发送，下次的调用会直接返回(放弃)，所以，要想连接发送，两行调用间，
  *			要么判断串口结构体gState的值，要么调用延时HAL_Delay(ms),ms值要大于前一帧发送用时, 
  *			用时计算：1/(波特率*11*前一帧字节数)
 ******************************************************************************/
 void UART_Send_IT(UART_HandleTypeDef *huart,uint8_t * str,uint32_t strlen)
 {
 	HAL_UART_Transmit_IT(huart,str, strlen);
 }
 /******************************************************************************
  * 函  数： UART_Send_DMA
  * 功  能： 利用DMA发送，非阻塞式，最大限度减少资源占用
  * 参  数： UART_HandleTypeDef *huart:要发送的串口句柄
  * 参  数： uint8_t * str：要发送的数组
  * 参  数：	uint32_t strlen：发送的字节数
  * 返回值： 无
  * 备  注： 当上次的调用还没完成发送，下次的调用会直接返回(放弃); 所以，要想连接发送，
  *			两行调用间，要么判断串口结构体gState的值，要么调用延时HAL_Delay(ms), ms值要大于前一帧发送用时，
  *			用时计算：1/(波特率*11*前一帧字节数)
 ******************************************************************************/
 void UART_Send_DMA(UART_HandleTypeDef *huart,uint8_t * str,uint32_t strlen)
 {
 	while (huart->gState != HAL_UART_STATE_READY);// 等待上条发送结束; 也可以用HAL_Delay延时法，但就要计算发送用时间
 	//程序暂时卡死不会往下运行
 	HAL_UART_Transmit_DMA(huart,str, strlen);
 }	
 /******************************************************************************
 * @brief  自定义的串口格式化打印函数
 * @param  huart: 指定要发送的串口句柄的指针 (例如 &huart1, &huart2)
 * @param  format: 格式化字符串，用法和标准 printf 完全一致
 * @param  ... : 可变参数列表
 ******************************************************************************/
void USART_printf(UART_HandleTypeDef *huart, const char *format, ...)
{
    // 定义一个足够大的局部缓冲区，256字节通常足够常规打印了
    // 如果你的内存非常紧张，可以适当调小；如果要打印超长字符串，可以调大
    char TX_Buffer[256]; 
    int len;

    va_list ap;
    va_start(ap, format);
    
    // 使用标准库的 vsnprintf 将格式化后的完整字符串放入 TX_Buffer
    // vsnprintf 是安全的，它会自动防止数组越界
    len = vsnprintf(TX_Buffer, sizeof(TX_Buffer), format, ap);
    
    va_end(ap);

    // 如果格式化成功，调用 HAL 库一次性将整个缓冲区发送出去
    if(len > 0)
    {
        HAL_UART_Transmit(huart, (uint8_t *)TX_Buffer, len, HAL_MAX_DELAY);
    }
}
 /******************************************************************************
  * 函  数： HAL_UARTEx_RxEventCallback
  * 功  能： DMA+空闲中断回调函数
  * 参  数： UART_HandleTypeDef  *huart   // 触发的串口
  *          uint16_t             Size    // 接收字节
  * 返回值： 无
  * 备  注： 1：这个是回调函数，不是中断服务函数。技巧：使用CubeMX生成的工程中，中断服务函数已被CubeMX安排妥当，
  *			我们只管重写回调函数
  *          2：触发条件：当DMA接收到指定字节数时，或产生空闲中断时，硬件就会自动调用本回调函数，无需进行人工调用;
  *          3：必须使用这个函数名称，因为它在CubeMX生成时，已被写好了各种函数调用、函数弱定义(在stm32xx_hal_uart.c的底部); 
  *			不要在原弱定义中增添代码，而是重写本函数
  *          4：无需进行中断标志的清理，它在被调用前，已有清中断的操作;
  *          5：生成的所有DMA+空闲中断服务函数，都会统一调用这个函数，以引脚编号作参数
  *          6：判断参数传进来的引脚编号，即可知道是哪个串口接收收了多少字节
 ******************************************************************************/
 void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
 {
 #if UART1_EN
     // 串口UART1的DMA+空闲中断回调函数
     if (huart == &huart1) // 判断串口
     {
 		// 解锁串口状态
         __HAL_UNLOCK(huart);  
 		// 接收完成标志                                                               
         xUART1.ReceiveNum  = Size;
 		// 清0前一帧的接收数据                                                         
         memset(xUART1.ReceiveData, 0, sizeof(xUART1.ReceiveData));                          
         // 把新数据，从临时缓存中，复制到xUART1.ReceivedData[], 以备使用
 		memcpy(xUART1.ReceiveData, xUART1.BuffTemp, Size); 
 		// 再次开启DMA空闲中断; 每当接收完指定长度，或者产生空闲中断时，就会来到这个
         HAL_UARTEx_ReceiveToIdle_DMA(&huart1, xUART1.BuffTemp, sizeof(xUART1.BuffTemp));
     }
 #endif
    
 #if UART2_EN
     // 串口UART2的DMA+空闲中断回调函数
     if (huart == &huart2) // 判断串口
     {
 		// 解锁串口状态
         __HAL_UNLOCK(huart);
 		// 接收完成标志
         xUART2.ReceiveNum  = Size;
 		// 清0前一帧的接收数据                                                          
         memset(xUART2.ReceiveData, 0, sizeof(xUART2.ReceiveData)); 
 		// 把新数据，从临时缓存中，复制到xUART2.ReceivedData[], 以备使用                         
         memcpy(xUART2.ReceiveData, xUART2.BuffTemp, Size);
		 /* 底层只负责抛出数据，不关心谁来处理 */
        UART2_RxEvent_Callback(xUART2.BuffTemp, Size);
 		// 再次开启DMA空闲中断; 每当接收完指定长度，或者产生空闲中断时，就会来到这个
         HAL_UARTEx_ReceiveToIdle_DMA(&huart2, xUART2.BuffTemp, sizeof(xUART2.BuffTemp));
     }
 #endif

 #if UART3_EN
     // 串口UART3的DMA+空闲中断回调函数
     if (huart == &huart3) // 判断串口
     {
 		// 解锁串口状态
         __HAL_UNLOCK(huart);
 		// 接收完成标志
         xUART3.ReceiveNum  = Size;
 		// 清0前一帧的接收数据                                                          
         memset(xUART3.ReceiveData, 0, sizeof(xUART3.ReceiveData)); 
 		// 把新数据，从临时缓存中，复制到xUART3.ReceivedData[], 以备使用                         
         memcpy(xUART3.ReceiveData, xUART3.BuffTemp, Size);
 		// 再次开启DMA空闲中断; 每当接收完指定长度，或者产生空闲中断时，就会来到这个
         HAL_UARTEx_ReceiveToIdle_DMA(&huart3, xUART3.BuffTemp, sizeof(xUART3.BuffTemp));
     }
 #endif

 #if UART4_EN
     // 串口UART4的DMA+空闲中断回调函数
     if (huart == &huart4)// 判断串口
     {
 		// 解锁串口状态
         __HAL_UNLOCK(huart);
 		// 接收完成标志
         xUART4.ReceiveNum  = Size;
 		// 清0前一帧的接收数据                                                          
         memset(xUART4.ReceiveData, 0, sizeof(xUART4.ReceiveData)); 
 		// 把新数据，从临时缓存中，复制到xUART4.ReceivedData[], 以备使用                         
         memcpy(xUART4.ReceiveData, xUART4.BuffTemp, Size);
 		// 再次开启DMA空闲中断; 每当接收完指定长度，或者产生空闲中断时，就会来到这个
         HAL_UARTEx_ReceiveToIdle_DMA(&huart4, xUART4.BuffTemp, sizeof(xUART4.BuffTemp));   
 	}
 #endif

 #if UART5_EN
     // 串口UART5的DMA+空闲中断回调函数
     if (huart == &huart5)// 判断串口
     {
 		// 解锁串口状态
         __HAL_UNLOCK(huart);
 		// 接收完成标志
         xUART3.ReceiveNum  = Size;
 		// 清0前一帧的接收数据                                                          
         memset(xUART5.ReceiveData, 0, sizeof(xUART5.ReceiveData)); 
 		// 把新数据，从临时缓存中，复制到xUART5.ReceivedData[], 以备使用                         
         memcpy(xUART5.ReceiveData, xUART5.BuffTemp, Size);
 		// 再次开启DMA空闲中断; 每当接收完指定长度，或者产生空闲中断时，就会来到这个
         HAL_UARTEx_ReceiveToIdle_DMA(&huart5, xUART5.BuffTemp, sizeof(xUART5.BuffTemp));    
 	}
 #endif

 #if UART6_EN
     // 串口UART6的DMA+空闲中断回调函数
     if (huart == &huart6) // 判断串口
     {
         // 解锁串口状态
         __HAL_UNLOCK(huart);
 		// 接收完成标志
         xUART6.ReceiveNum  = Size;
 		// 清0前一帧的接收数据                                                          
         memset(xUART6.ReceiveData, 0, sizeof(xUART6.ReceiveData)); 
 		// 把新数据，从临时缓存中，复制到xUART6.ReceivedData[], 以备使用                         
         memcpy(xUART6.ReceiveData, xUART6.BuffTemp, Size);
 		// 再次开启DMA空闲中断; 每当接收完指定长度，或者产生空闲中断时，就会来到这个
         HAL_UARTEx_ReceiveToIdle_DMA(&huart6, xUART6.BuffTemp, sizeof(xUART6.BuffTemp));
     }
 #endif
 }
 
