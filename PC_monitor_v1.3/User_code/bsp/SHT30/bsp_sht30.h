#ifndef BSP_SHT30_H
#define BSP_SHT30_H

#include "main.h"
#include <stdint.h>
#include "i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================
 * SHT30 I2C 地址
 * 数据手册中给出的地址为 7 位地址： 
 * ADDR 接 GND：0x44
 * ADDR 接 VDD：0x45
 * STM32 HAL I2C 函数使用左移 1 位后的设备地址，因此实际传入 HAL 函数的地址为 0x88。
 *===========================================================*/
#define BSP_SHT30_ADDR_7BIT          0x44U
#define BSP_SHT30_I2C_ADDR           (BSP_SHT30_ADDR_7BIT << 1)
/* I2C 通信超时时间，单位为毫秒 */
#define BSP_SHT30_I2C_TIMEOUT        100U
/*============================================================
 * SHT30 命令定义
 *===========================================================*/
/* 单次测量命令：高重复性,禁止时钟延展,命令：0x2400 */
#define SHT30_CMD_MEASURE_HIGH       0x2400U
/* 软件复位命令,命令：0x30A2 */
#define SHT30_CMD_SOFT_RESET         0x30A2U
/*============================================================
 * SHT30 驱动返回状态
 *===========================================================*/
typedef enum
{
    BSP_SHT30_OK = 0,          /* 操作成功 */
    BSP_SHT30_ERROR,           /* I2C 通信错误 */
    BSP_SHT30_CRC_ERROR,       /* 数据 CRC 校验错误 */
    BSP_SHT30_PARAM_ERROR      /* 输入参数错误 */
} BSP_SHT30_Status_t;
/*============================================================
 * SHT30 温湿度数据结构体
 *===========================================================*/
typedef struct
{
    float temperature;          /* 温度，单位：摄氏度 */
    float humidity;             /* 相对湿度，单位：%RH */
    uint16_t temperature_raw;   /* 温度原始数据 */
    uint16_t humidity_raw;      /* 湿度原始数据 */
} BSP_SHT30_Data_t;
/*============================================================
 * 全局变量
 *===========================================================*/
/* 保存传感器数据结构体 */
extern BSP_SHT30_Data_t SHT30_data;

/*============================================================
 * 函数声明
 *===========================================================*/
/**
 * @brief 初始化 SHT30
 *
 * @return BSP_SHT30_Status_t
 *         BSP_SHT30_OK    初始化成功
 *         BSP_SHT30_ERROR 初始化失败
 */
BSP_SHT30_Status_t SHT30_Init(void);
/**
 * @brief 检测 SHT30 是否存在于 I2C 总线上
 *
 * @return BSP_SHT30_Status_t
 */
BSP_SHT30_Status_t SHT30_IsReady(void);
/**
 * @brief 对 SHT30 执行软件复位
 *
 * @return BSP_SHT30_Status_t
 */
BSP_SHT30_Status_t SHT30_SoftReset(void);
/**
 * @brief 读取完整的 SHT30 温湿度数据
 *
 * @param data 温湿度数据结构体指针
 *
 * @return BSP_SHT30_Status_t
 */
BSP_SHT30_Status_t SHT30_ReadData(BSP_SHT30_Data_t *data);
/**
 * @brief 读取 SHT30 温度和湿度
 *
 * @param temperature 温度输出指针
 * @param humidity    湿度输出指针
 *
 * @return BSP_SHT30_Status_t
 */
BSP_SHT30_Status_t SHT30_Read(float *temperature,float *humidity);

#ifdef __cplusplus
}
#endif

#endif /* BSP_SHT30_H */
