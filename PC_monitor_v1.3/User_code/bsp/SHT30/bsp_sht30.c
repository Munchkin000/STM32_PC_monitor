#include "bsp_sht30.h"
#include <stdio.h>
/*============================================================
 * SHT30使用的I2C句柄
 *===========================================================*/
static I2C_HandleTypeDef *g_SHT30I2CHdle = &hi2c2;//根据配置的I2C句柄进行更改(仅硬件IIC使用)
/* 保存传感器数据结构体 */
BSP_SHT30_Data_t SHT30_data;
/*============================================================
 * 检测 SHT30 是否在线
 *===========================================================*/
BSP_SHT30_Status_t SHT30_IsReady(void)
{
    HAL_StatusTypeDef status;
    /* 尝试最多 3 次检测 SHT30 是否应答 */
    status = HAL_I2C_IsDeviceReady(g_SHT30I2CHdle,BSP_SHT30_I2C_ADDR,3,BSP_SHT30_I2C_TIMEOUT);
    if (status == HAL_OK)
    {
        return BSP_SHT30_OK;
    }
    return BSP_SHT30_ERROR;
}
/*============================================================
 * 向 SHT30 发送 16 位命令
 *===========================================================*/
static BSP_SHT30_Status_t SHT30_SendCommand(uint16_t command)
{
    uint8_t tx_data[2];
    /* SHT30 命令为 16 位数据，发送时先发送高 8 位，再发送低 8 位。*/
    tx_data[0] = (uint8_t)(command >> 8);
    tx_data[1] = (uint8_t)(command & 0xFFU);
    if (HAL_I2C_Master_Transmit(g_SHT30I2CHdle,BSP_SHT30_I2C_ADDR,tx_data,2,BSP_SHT30_I2C_TIMEOUT) != HAL_OK)
    {
        return BSP_SHT30_ERROR;
    }
    return BSP_SHT30_OK;
}
/*============================================================
 * 计算 CRC-8 校验值
 *
 * SHT30 数据手册规定：
 * 多项式：0x31
 * 初始值：0xFF
 * 输入不反转,输出不反转
 * 最终异或值：0x00
 *===========================================================*/
static uint8_t SHT30_CalculateCRC(const uint8_t *data,uint8_t length)
{
    uint8_t crc = 0xFFU;
    uint8_t i;
    uint8_t bit;

    for (i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (bit = 0; bit < 8; bit++)
        {
            if ((crc & 0x80U) != 0U)
            {
                crc = (uint8_t)((crc << 1) ^ 0x31U);
            }
            else
            {
                crc <<= 1;
            }
        }
    }
    return crc;
}
/*============================================================
 * SHT30 软件复位
 *===========================================================*/
BSP_SHT30_Status_t SHT30_SoftReset(void)
{
    /* 发送软件复位命令 0x30A2 */
    if (SHT30_SendCommand(SHT30_CMD_SOFT_RESET) != BSP_SHT30_OK)
    {
        return BSP_SHT30_ERROR;
    }
    /* 数据手册规定软件复位最长需要约 1 ms。此处等待 2 ms，留出一定时间余量。 */
    HAL_Delay(2);
    return BSP_SHT30_OK;
}
/*============================================================
 * 初始化 SHT30
 *===========================================================*/
BSP_SHT30_Status_t SHT30_Init(void)
{
    /* SHT30 上电后需要一定时间进入空闲状态。数据手册给出的最长上电时间约为 1 ms，此处等待 2 ms。*/
    HAL_Delay(2);
    /* 首先检测传感器是否能够正常应答。 */
    if (SHT30_IsReady() != BSP_SHT30_OK)
    {
        printf("[SHT30] SHT30 sensor ack failed.\r\n");
        return BSP_SHT30_ERROR;
    }
    /* 对传感器执行一次软件复位，使其进入确定的初始状态。 */
    if (SHT30_SoftReset() != BSP_SHT30_OK)
    {
        printf("[SHT30] SHT30 soft reset failed.\r\n");
        return BSP_SHT30_ERROR;
    }
    /* 软件复位完成后再次检测设备是否在线。*/
    if (SHT30_IsReady() != BSP_SHT30_OK)
    {
        printf("[SHT30] SHT30 sensor ack failed.\r\n");
        return BSP_SHT30_ERROR;
    }
    printf("[SHT30] SHT30 sensor init successed.\r\n");
    return BSP_SHT30_OK;
}
/*============================================================
 * 读取 SHT30 温湿度数据,单次读取返回 6 字节：
 * 字节 0：温度高 8 位
 * 字节 1：温度低 8 位
 * 字节 2：温度 CRC
 * 字节 3：湿度高 8 位
 * 字节 4：湿度低 8 位
 * 字节 5：湿度 CRC
 *===========================================================*/
BSP_SHT30_Status_t SHT30_ReadData(BSP_SHT30_Data_t *data)
{
    uint8_t rx_data[6];
    uint16_t raw_temperature;
    uint16_t raw_humidity;
    /* 检查输入指针是否合法。 */
    if (data == NULL)
    {
        return BSP_SHT30_PARAM_ERROR;
    }
    /*========================================================
     * 第一步：启动单次温湿度测量
     *=======================================================*/
    /* 使用命令 0x2400：高重复性,禁止时钟延展 */
    if (SHT30_SendCommand(SHT30_CMD_MEASURE_HIGH) != BSP_SHT30_OK)
    {
        return BSP_SHT30_ERROR;
    }
    /* 高重复性测量的最长测量时间约为 15 ms。 */
    HAL_Delay(20);
    /*========================================================
     * 第二步：读取 6 字节测量结果
     *=======================================================*/
    if (HAL_I2C_Master_Receive(g_SHT30I2CHdle,BSP_SHT30_I2C_ADDR,rx_data,sizeof(rx_data),
                               BSP_SHT30_I2C_TIMEOUT) != HAL_OK)
    {
        return BSP_SHT30_ERROR;
    }
    /*========================================================
     * 第三步：检查温度数据 CRC
     *=======================================================*/
    /* 温度原始数据由 rx_data[0] 和 rx_data[1] 组成，rx_data[2] 为对应的 CRC 校验值。*/
    if (SHT30_CalculateCRC(&rx_data[0], 2) != rx_data[2])
    {
        return BSP_SHT30_CRC_ERROR;
    }
    /*========================================================
     * 第四步：检查湿度数据 CRC
     *=======================================================*/
    /* 湿度原始数据由 rx_data[3] 和 rx_data[4] 组成，rx_data[5] 为对应的 CRC 校验值。*/
    if (SHT30_CalculateCRC(&rx_data[3], 2) != rx_data[5])
    {
        return BSP_SHT30_CRC_ERROR;
    }
    /*========================================================
     * 第五步：组合 16 位原始数据
     *=======================================================*/
    raw_temperature = ((uint16_t)rx_data[0] << 8) | ((uint16_t)rx_data[1]);
    raw_humidity = ((uint16_t)rx_data[3] << 8) | ((uint16_t)rx_data[4]);
    /* 保存传感器原始数据，方便后续调试时直接观察原始值。*/
    data->temperature_raw = raw_temperature;
    data->humidity_raw    = raw_humidity;
    /*========================================================
     * 第六步：将原始数据转换为实际温湿度
     *=======================================================*/
    /* 温度换算公式：T = -45 + 175 × ST / 65535,ST 为 16 位温度原始数据。*/
    data->temperature = -45.0f + 175.0f * ((float)raw_temperature / 65535.0f);
    /* 相对湿度换算公式：RH = 100 × SRH / 65535,RH 为 16 位湿度原始数据。*/
    data->humidity = 100.0f * ((float)raw_humidity / 65535.0f);
    return BSP_SHT30_OK;
}
/*============================================================
 * 简化的温湿度读取接口
 *===========================================================*/
BSP_SHT30_Status_t SHT30_Read(float *temperature,float *humidity)
{
    BSP_SHT30_Data_t data;
    BSP_SHT30_Status_t status;
    /* 检查输出指针是否合法。*/
    if ((temperature == NULL) || (humidity == NULL))
    {
        return BSP_SHT30_PARAM_ERROR;
    }
    /* 读取完整的 SHT30 数据。*/
    status = SHT30_ReadData(&data);
    if (status != BSP_SHT30_OK)
    {
        return status;
    }
    /* 将测量结果返回给调用者。*/
    *temperature = data.temperature;
    *humidity    = data.humidity;

    return BSP_SHT30_OK;
}
