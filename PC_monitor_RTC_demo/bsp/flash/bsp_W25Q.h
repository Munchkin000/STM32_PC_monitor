#ifndef __BSP__W25Q_H
#define __BSP__W25Q_H
/***********************************************************************************************************************************
 ** 【代码使用】  所用GPIO引脚和SPI端口，可以在 "bsp_W25Q128.h＂中修改;
 **               整个W25Qxx的读写操作，包括中文字库数据，已封装成4个全局函数，只用这4个函数，即可完成对其存取操作
 **               初始化  ：  W25Qxx_Init()　
 **               读取数据：  W25Qxx_ReadData (uint32_t addr, uint8_t *pData, uint16_t num)　      // 读取数据：数据地址，缓存指针，字节数
 **               写入数据：  W25Qxx_WriteData(uint32_t addr, uint8_t *pData, uint16_t num)        // 写入数据：数据地址，缓存指针，字节数
 **               字模读取：  W25Qxx_ReadGBK  (uint8_t *typeface, uint8_t size, uint8_t *dataBuf); // 读取字模：从w25qxx的字库中读取出字模数据
 **
 ** 【划 重 点】 1_ 本代码，适用W25Q16、W25Q32、W25Q65、W25Q128，不适用于W25Q256;
 **              2_ 地址的范围取值，如W25Q128，容量：128/8=16M字节，则字节地址范围：0~16777216，用16进制表示为：0x00~0x01000000
 **              3_ 如果使用的是魔女开发板上的W25Q128，存储数据时，请使用前10M空间地址0X0~0x9FFFFF; 芯片后6M空间,已存储汉字字模数据，地址为0x00A00000~0x01000000;
 **              4_ 代码已经多次优化，直接使用即可，而无需过于纠结SPI通信原理、芯片的存储原理，而浪费了时间;
 ** 
 ** 【字库使用】  特别地注意，请慎重使用芯片擦除，魔女开发板的w25q128，在存储区尾部已烧录宋体4种字号大小汉字GBK字模数据
 **               字库存放地址：0x00A00000 - 0x01000000   尽量不要写操作此区域地址
 **               具体的读取操作，可参考c文件中函数
 **
***********************************************************************************************************************************/
#include <stdio.h>
#include "main.h"
#include "spi.h"

/*****************************************************************************
 ** 引脚定义
 ** 移植时，如果使用SPI1,只需要修改这个区域
****************************************************************************/
#define  GBK_STORAGE_ADDR               0x00A00000       // 汉字GBK字库起始地址,魔女开发板的W25Q128已保存宋体12、16、24、32号字体

#define  W25Qxx_SPI                    hspi1             // SPI端口

// 引脚复用功能(在main.h中定义)
#define  W25Qxx_SPI_AFx                GPIO_AF5_SPI1    // 引脚复用编号
// SCK
#define  W25Qxx_SCK_GPIO               W25Q_CLK_GPIO_Port  // SCK引脚; 时钟同步引脚
#define  W25Qxx_SCK_PIN                W25Q_CLK_Pin    
// MISO                              
#define  W25Qxx_MISO_GPIO              W25Q_MISO_GPIO_Port  // MISO引脚; 主机输入从机输出
#define  W25Qxx_MISO_PIN               W25Q_MISO_Pin
// MOSI                                  
#define  W25Qxx_MOSI_GPIO              W25Q_MOSI_GPIO_Port  // MOSI引脚; 主机输出从机输入
#define  W25Qxx_MOSI_PIN               W25Q_MOSI_Pin                
// CS
#define  W25Qxx_CS_GPIO                W25Q_CS_GPIO_Port  // 片选引脚; SPI总线中，主机拉低哪个设备的CS线，那个设备就接受通信
#define  W25Qxx_CS_PIN                 W25Q_CS_Pin      

//W25Q系列芯片型号返回值
#define    W25Q80            0XEF13
#define    W25Q16            0XEF14
#define    W25Q32            0XEF15
#define    W25Q64            0XEF16
#define    W25Q128           0XEF17
#define    W25Q256           0XEF18
//#define  W25Qxx    65519    // 很多时候重新下载后读出的都是65519

// #define    W25Q128_CS_HIGH    (W25Q128_CS_GPIO -> BSRR =  W25Q128_CS_PIN)
// #define    W25Q128_CS_LOW     (W25Q128_CS_GPIO -> BSRR =  W25Q128_CS_PIN << 16)
#define    W25Qxx_CS_HIGH    HAL_GPIO_WritePin(W25Qxx_CS_GPIO, W25Qxx_CS_PIN, GPIO_PIN_SET)
#define    W25Qxx_CS_LOW     HAL_GPIO_WritePin(W25Qxx_CS_GPIO, W25Qxx_CS_PIN, GPIO_PIN_RESET)
/*****************************************************************************
 ** 声明全局变量
****************************************************************************/
typedef struct
{
    uint8_t   FlagInit;      // 初始化状态   0:失败, 1:成功
    uint8_t   FlagGBKStorage;// GBK字库标志; 0=没有, 1=可用; 作用: 用于判断地址段的写保护, 防止字库被错误写履盖; 并可作LCD的中文输出判断
    char      type[20];      // 型号
    uint16_t  StartupTimes;  // 记录启动次数
} xW25Q_TypeDef;

// 声明全局结构体, 用于记录w25qxx信息
extern xW25Q_TypeDef  xW25Qxx;

/*****************************************************************************
 ** 声明全局函数
 ** 为统一代码以方便移植，已封装成4个对外函数，可完成对其所有存取操作
****************************************************************************/
/* 初始化 */
uint8_t W25Qxx_Init(void);                                                    
// 读数据：addr-地址，*pData-读取后数据缓存，num-要读取的字节数
void    W25Qxx_ReadData(uint32_t addr, uint8_t *pData, uint16_t num);  
// 写数据：addr-地址，*pData-待写的数据缓存，num-要写入的字节数
void    W25Qxx_WriteData(uint32_t addr, uint8_t *pData, uint16_t num);        
// 读取字模：*pFont-汉字，size-字号，*fontData-读取到的字模点阵数据
void    W25Qxx_ReadFontData(uint8_t *pFont, uint8_t size, uint8_t *fontData); 
/* FLASH读写测试 */
uint8_t W25Qxx_Test(void);
#endif




