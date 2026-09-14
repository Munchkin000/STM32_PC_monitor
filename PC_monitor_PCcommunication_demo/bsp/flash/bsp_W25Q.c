#include "bsp_W25Q.h"

xW25Q_TypeDef  xW25Qxx;      // 声明全局结构体, 用于记录w25qxx信息
uint8_t W25Qxx_buffer[4096];                         // 开辟一段内存空间

/******************************************************************************
 * 函  数： delay_ms
 * 功  能： ms 延时函数
 * 备  注： 1、系统时钟168MHz
 *          2、打勾：Options/ c++ / One ELF Section per Function
            3、编译优化级别：Level 3(-O3)
 * 参  数： uint32_t  ms  毫秒值
 * 返回值： 无
 ******************************************************************************/
static volatile uint32_t ulTimesMS;    // 使用volatile声明，防止变量被编译器优化
static void delay_ms(uint16_t ms)
{
    ulTimesMS = ms * 16500;
    while (ulTimesMS)
    {
        ulTimesMS--;                   // 操作外部变量，防止空循环被编译器优化掉
    }       
}
// 5_1 发送1字节,返回1字节
// SPI通信,只一个动作:向DR写入从设命令值,同步读出数据!写读组合,按从设时序图来. 作为主设,
//因为收发同步,连接收发送中断也不用开,未验证其它中断对其工作的影响.
static uint8_t  sendByte(uint8_t d)
{
    uint8_t rxData = 0;
    uint32_t retry = 0;
    HAL_StatusTypeDef status;

    // 等待发送缓冲区空（TXE标志）
    while (__HAL_SPI_GET_FLAG(&W25Qxx_SPI, SPI_FLAG_TXE) == RESET)
    {
        retry++;
        if (retry > 1000) 
        {
            return 0;
        }
    }

    // 使用全双工传输，同时发送和接收
    status = HAL_SPI_TransmitReceive(&W25Qxx_SPI, &d, &rxData, 1, HAL_MAX_DELAY);
    if (status != HAL_OK)
    {
        return 0;
    }
    retry = 0;
    
    // 等待发送缓冲区空（TXE标志）
    while (__HAL_SPI_GET_FLAG(&W25Qxx_SPI, SPI_FLAG_TXE) == RESET)
    {
        retry++;
        if (retry > 1000) 
        {
            return 0;
        }
    }
    return rxData;
}
// 5_2 写使能
static void writeEnable()
{
    W25Qxx_CS_LOW ;
    sendByte(0x6);                           // 命令: Write Enable : 06h
    W25Qxx_CS_HIGH ;
}
// 5_3 等待空闲
static void WaitReady()
{
    W25Qxx_CS_LOW ;
    sendByte(0x05);    // 命令: Read Status Register : 05h
    while (sendByte(0xFF) & 1) {}
    // 只要发送读状态寄存器指令，芯片就会持续向主机发送最新的状态寄存器内容 ，直到收到通信的停止信号。
    W25Qxx_CS_HIGH ;
}
// 5_4 擦除一个扇区, 每扇区>150ms
static void eraseSector(uint32_t addr)
{
    if (xW25Qxx.FlagInit == 0) 
    {
        return;      // 如果W25Qxx初始化失败，则跳过检测，防止卡死
    }

    addr = addr * 4096;                      // 从第几扇区开始

    writeEnable();
    WaitReady();
    // 命令
    W25Qxx_CS_LOW ;
    sendByte(0x20);                          // 命令: Sector Erase(4K) : 20h
    sendByte((uint8_t)(addr >> 16));
    sendByte((uint8_t)(addr >> 8));
    sendByte((uint8_t)addr);
    W25Qxx_CS_HIGH ;

    WaitReady();
}
// 5_5 写扇区. 要分页写入
static void writeSector(uint32_t addr, uint8_t *p, uint16_t num)
{
    if (xW25Qxx.FlagInit == 0) 
    {
        return;           // 如果W25Qxx初始化失败，则跳过检测，防止卡死
    }

    uint16_t pageRemain = 256;                    // 重要，重要，重要：W25Qxx每个页命令最大写入字节数:256字节;

    for (char i = 0; i < 16; i++)                 // 扇区:4096bytes, 缓存页:256bytes, 写扇区要分16次页命令写入
    {
        writeEnable();                            // 写使能
        WaitReady();                              // 等待空闲

        W25Qxx_CS_LOW ;                          // 低电平,开始
        sendByte(0x02);                           // 命令: page program : 02h , 每个写页命令最大缓存256字节
        sendByte((uint8_t)(addr >> 16));          // 地址
        sendByte((uint8_t)(addr >> 8));
        sendByte((uint8_t)addr);
        for (uint16_t i = 0; i < pageRemain; i++) // 发送写入的数据
        {
            sendByte(p[i]);// 高电平, 结束
        }                      
        W25Qxx_CS_HIGH ;

        WaitReady();                              // 等待空闲

        p = p + pageRemain;                       // 缓存指针增加一页字节数
        addr = addr + pageRemain ;                // 写地址增加一页字节数
    }
}
// 读取芯片型号
static uint32_t readID(void)
{
    uint16_t Temp = 0;
    W25Qxx_CS_LOW;
    sendByte(0x90);//发送读取ID命令
    sendByte(0x00);
    sendByte(0x00);
    sendByte(0x00);
    Temp |= sendByte(0xFF) << 8;
    Temp |= sendByte(0xFF);
    W25Qxx_CS_HIGH;

    xW25Qxx.FlagInit  = 1;
    switch (Temp)
    {
        case W25Q16:
            sprintf((char *)xW25Qxx.type, "%s", "W25Q16");
            break;
        case W25Q32:
            sprintf((char *)xW25Qxx.type, "%s", "W25Q32");
            break;
        case W25Q64:
            sprintf((char *)xW25Qxx.type, "%s", "W25Q64");
            break;
        case W25Q128:
            sprintf((char *)xW25Qxx.type, "%s", "W25Q128");
            break;
        case W25Q256:
            sprintf((char *)xW25Qxx.type, "%s", "W25Q256");  // 注意:W25Q256的地址是4字节
            break;
        default:
        {
            sprintf((char *)xW25Qxx.type, "%s", "Flash设备失败 !!!");
            xW25Qxx.FlagInit = 0;
            printf("[W25Q] 读取到的错误型号数据：%d\r\n", Temp);
            break;
        }
    }

    if (xW25Qxx.FlagInit  == 1)
    {
        printf("[W25Q] Flash存储 检测...        型号:%s\r", xW25Qxx.type);
    }
    else
    {
        printf("[W25Q] 数据存储检测：           型号读取错误，设备不可用!\r");
    }

    return Temp;
}
//// 检查字库样本的正确性
//static void checkFlagGBKStorage(void)
//{
//    if (xW25Q128 .FlagInit == 0) 
//    {
//        return;                  // 如果W25Qxx初始化失败，则跳过检测，防止卡死
//    }

//    printf("GBK字库 测试...          ");
//    uint8_t sub = 0;
//    uint8_t f = 0 ;

//    for (uint32_t i = 0; i < 6128640; i = i + 1000000)
//    {
//        W25Q128_ReadData(GBK_STORAGE_ADDR + i, &f, 1);
//        sub = sub + f;                                    // 80 , 0, 98, 79, 0, 1, 0
//    }
//    xW25Q128.FlagGBKStorage = (sub == 146 ? 1 : 0);       // 判断是否有字库,打开地址写保护, 防止字库被错误写入履盖

//    if (xW25Q128.FlagGBKStorage == 1)
//    {
//        printf("字库可用\r");                             // 标记字库可用
//    }
//    else
//    {
//        printf(" 错误，字库不可用!\r");
//    }
//}
/******************************************************************************
 * 函  数： W25qx_Init
 * 功  能： 初始化W25Q128所需引脚、SPI
 * 参  数：
 * 返回值： 初始化结果，0:失败、1:成功
 ******************************************************************************/
uint8_t W25Qxx_Init(void)
{  
    delay_ms(300);       // 重要！！ 上电后，要稍延时300ms，以避免烧录时多次复位导致擦写过程错误。
    W25Qxx_CS_HIGH;                                                 // CS引脚拉高：停止信号
    // 配置SPI工作模式
//     SPI1 -> CR1  = 0x1 << 0; // CPHA:时钟相位,0x1=在第2个时钟边沿进行数据采样
//     SPI1 -> CR1 |= 0x1 << 1; // CPOL:时钟极性,0x1=空闲状态时，SCK保持高电平
//     SPI1 -> CR1 |= 0x1 << 2; // 主从模式:         1 = 主配置
//     SPI1 -> CR1 |= 0x0 << 3; // 波特率控制[5:3]:  0 = fPCLK /2
//	 SPI1 -> CR1 |= 0x0 << 7; // 帧格式:           0 = 先发送MSB
//     SPI1 -> CR1 |= 0x1 << 9; // 软件从器件管理 :  1 = 使能软件从器件管理(软件NSS)
//     SPI1 -> CR1 |= 0x1 << 8; // 内部从器件选择,根据9位设置(失能内部NSS)
//     SPI1 -> CR1 |= 0x0 << 11;// 数据帧格式,       0 = 8位
     //SPI1 -> CR1 |= 0x1 << 6; // SPI使能           1 = 使能外设

    readID();                                                 // 读取芯片型号,判断通讯是否正常
//    checkFlagGBKStorage();                                    // 检查字库

    if (xW25Qxx.FlagInit)
    {
        return 1;                                             // 初始化成功，返回:1
    }       
    return 0;                                                 // 初始化失败，返回:0
}
/******************************************************************************
 * 函  数： W25Q_ReadData
 * 功  能： 读取数据
 * 参  数： uint32_t addr  数据在W25Qxx内的地址
 *          uint8_t *pData 数据缓存地址
 *          uint16_t num   连续读取的字节数
 * 返回值： 无
 ******************************************************************************/
void W25Qxx_ReadData(uint32_t addr, uint8_t *pData, uint16_t num)
{
    if (xW25Qxx .FlagInit == 0) 
    {
        return;  // 如果W25Qxx初始化失败，则跳过检测，防止卡死
    }

    W25Qxx_CS_LOW ;
    sendByte(0x03);                       // 发送读取命令 03h
    sendByte((uint8_t)(addr >> 16));
    sendByte((uint8_t)(addr >> 8));
    sendByte((uint8_t)addr);

    for (uint32_t i = 0; i < num; i++)
    {
        pData[i] = sendByte(0xFF);
    }

    W25Qxx_CS_HIGH ;
}
/******************************************************************************
 * 函数名： W25Q_WriteData
 * 功  能： 从addr处起，读取num个字节，存放到缓存p
 * 参  数： uint32_t  addr   写入地址         (W25Q128 只用3字节, W25Q256用4字节)
 *          uint8_t  *pData  要写入的数据存储区
 *          uint16_t  num    写入的字节数
 * 返  回： 无
 * 备  注： 最后更新_2020年12月15日
 ******************************************************************************/
void W25Qxx_WriteData(uint32_t addr, uint8_t *pData, uint16_t num)
{
    if (xW25Qxx.FlagInit == 0) 
    {
        return ;              // 如果w25qxx设备初始化失败，则跳过本函数，防止卡死
    }

    // 字库段写保护, 防止字库被错误写入履盖
    if (((addr + num) > 0x00A00000) && (xW25Qxx.FlagGBKStorage == 1))
    {
        printf("[W25Q] 要写入的数据在字库数据存储区内，已跳过本次操作!!\r");
        return;
    }

    uint32_t  secPos      = addr / 4096;              // 扇区地址,第几个扇区
    uint16_t  secOff      = addr % 4096;              // 开始地始偏移字节数: 数据在扇区的第几字节存放
    uint16_t  secRemain   = 4096 - secOff;            // 扇区剩余空间字节数 ,用于判断够不够存放余下的数据
    uint8_t  *buf = W25Qxx_buffer;                   // 原子哥代码,为什么不直接使用所声明的数组.

    if (num <= secRemain) 
    {
        secRemain = num;
    }
    while (1)
    {
        W25Qxx_ReadData(secPos * 4096, buf, 4096);   // 读取扇区内容到缓存

        eraseSector(secPos);                          // 擦扇区
        for (uint16_t i = 0; i < secRemain ; i++)     // 原始数据写入缓存
        {
            buf[secOff + i] = pData[i];
        }
        writeSector(secPos * 4096, buf, 4096);        // 缓存数据写入设备

        if (secRemain == num)                         // 已全部写入
        {
            break;
        }
        else
        {
            // 未写完
            pData = pData + secRemain ;               // 原始数据指针偏移
            secPos ++;                                // 新扇区
            secOff = 0;                               // 新偏移位,扇区内数据起始地址
            num = num - secRemain ;                   // 剩余未写字节数
            secRemain = (num > 4096) ? 4096 : num;    // 计算新扇区写入字节数
        }
    }
}
/******************************************************************************
 * 函数名： W25Q_ReadFontData
 * 功  能： 从w25Q的字库中读取出汉字字模数据
 * 参  数： uint8_t *pFont     汉字
 *          uint8_t  size      字体大小 12/16/24/32
 *          uint8_t *fontData  读取到的字模点阵数据
 * 返  回： 无
 * 备  注： 最后更新_2024年02月05日
 ******************************************************************************/
void W25Q_ReadFontData(uint8_t *pFont, uint8_t size, uint8_t *fontData)
{
    uint8_t qh, ql;
    uint32_t foffset;
    uint8_t csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size);                           // 计算汉字点阵大小，单位字节数

    qh = *pFont;                                                                          // 汉字GBK的第一个字节
    ql = *(++pFont);                                                                      // 汉字GBK的第二个字节

    if (qh < 0x81 || ql < 0x40 || ql == 0xff || qh == 0xff)                               // 不在字库内的汉字，将用填充显示整个位置
    {
        for (uint8_t i = 0; i < csize; i++) 
        {
            *fontData++ = 0x00;                           // 填充满格
        }
        return;                                                                           // 返回
    }

    if (ql < 0x7f)  
    {
         ql -= 0x40;// 计算要提取的汉字在字库中的偏移位置
    }                                                                      
    else
    {
        ql -= 0x41;
    }

    qh -= 0x81;
    foffset = ((unsigned long)190 * qh + ql) * csize; // 得到汉字在字库中的偏移位置

    switch (size)  // 按字体的不同，在不同字库读取字体点阵
    {
        case 12:
            W25Qxx_ReadData(foffset + GBK_STORAGE_ADDR + 0x00000000, fontData, csize);     // 12号字体
            break;
        case 16:
            W25Qxx_ReadData(foffset + GBK_STORAGE_ADDR + 0x0008c460, fontData, csize);   // 16号字体
            break;
        case 24:
            W25Qxx_ReadData(foffset + GBK_STORAGE_ADDR + 0x001474E0, fontData, csize);   // 24号字体
            break;
        case 32:
            W25Qxx_ReadData(foffset + GBK_STORAGE_ADDR + 0x002EC200, fontData, csize);   // 32号字体
            break;
    }
}

/******************************************************************************
 * 函  数： W25Qxx_Test
 * 功  能： W25Qxx Flash 简单读写测试
 * 测试流程：
 *          1. 初始化W25Qxx
 *          2. 向指定地址写入64字节测试数据
 *          3. 从相同地址读取64字节
 *          4. 比较写入和读取的数据
 * 返  回： 1-测试成功
 *          0-测试失败
 ******************************************************************************/
uint8_t W25Qxx_Test(void)
{
    #define W25Q_TEST_ADDR    0x00010000U
    #define W25Q_TEST_SIZE    64U

    uint8_t tx_buf[W25Q_TEST_SIZE];
    uint8_t rx_buf[W25Q_TEST_SIZE];
    uint16_t i;

    printf("\r\n");
    printf("========================================\r\n");
    printf("        W25Qxx Read/Write Test\r\n");
    printf("========================================\r\n");

    /* 1. 初始化W25Qxx */
    if (W25Qxx_Init() == 0)
    {
        printf("[W25Q] Init FAILED!\r\n");
        printf("========================================\r\n");
        return 0;
    }

    printf("[W25Q] Init OK\r\n");
    printf("[W25Q] Device: %s\r\n", xW25Qxx.type);

    /* 2. 生成测试数据 */
    for (i = 0; i < W25Q_TEST_SIZE; i++)
    {
        tx_buf[i] = (uint8_t)(i + 0x10);
        rx_buf[i] = 0;
    }

    printf("[W25Q] Test Address : 0x%06lX\r\n",
           (unsigned long)W25Q_TEST_ADDR);

    printf("[W25Q] Test Size    : %d Bytes\r\n",
           W25Q_TEST_SIZE);

    /* 3. 写入数据 */
    printf("[W25Q] Writing...\r\n");

    W25Qxx_WriteData(
        W25Q_TEST_ADDR,
        tx_buf,
        W25Q_TEST_SIZE
    );

    printf("[W25Q] Write finished\r\n");

    /* 4. 读取数据 */
    printf("[W25Q] Reading...\r\n");

    W25Qxx_ReadData(
        W25Q_TEST_ADDR,
        rx_buf,
        W25Q_TEST_SIZE
    );

    printf("[W25Q] Read finished\r\n");

    /* 5. 比较数据 */
    for (i = 0; i < W25Q_TEST_SIZE; i++)
    {
        if (tx_buf[i] != rx_buf[i])
        {
            printf("[W25Q] Verify FAILED!\r\n");

            printf("[W25Q] Error index : %d\r\n", i);

            printf("[W25Q] Write data  : 0x%02X\r\n",
                   tx_buf[i]);

            printf("[W25Q] Read data   : 0x%02X\r\n",
                   rx_buf[i]);

            printf("========================================\r\n");

            return 0;
        }
    }

    printf("[W25Q] Verify OK!\r\n");

    /* 6. 打印读回数据 */
    printf("[W25Q] Read Data:\r\n");

    for (i = 0; i < W25Q_TEST_SIZE; i++)
    {
        printf("%02X ", rx_buf[i]);

        if ((i + 1) % 16 == 0)
        {
            printf("\r\n");
        }
    }

    printf("\r\n");
    printf("[W25Q] TEST PASS!\r\n");
    printf("========================================\r\n");

    return 1;
}
