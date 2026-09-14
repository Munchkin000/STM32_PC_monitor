#include "usb_protocol.h"

#include "usbd_cdc_if.h"
#include "ui_task.h"
#include "ui_data.h"

#include <string.h>

/* ============================================================
 * 协议内部变量
 * ============================================================ */
/*
 * 行协议缓存
 *
 * 用于解决一条协议可能被拆成多个USB数据包的问题。
 */
static char s_protocol_line[USB_PROTOCOL_LINE_SIZE];

/* 当前已经缓存的字符数 */
static uint16_t s_protocol_line_len = 0;

/* ============================================================
 * 内部函数声明
 * ============================================================ */
static void USB_Protocol_HandleLine(char *line);

static uint8_t USB_Protocol_GetValue(
    const char *line,
    const char *key,
    int32_t *value
);

static void USB_Protocol_SendString(const char *str);

static void USB_Protocol_HandlePC(char *line);

static void USB_Protocol_HandlePage(char *line);

/* ============================================================
 * 发送字符串
 * ============================================================ */
static void USB_Protocol_SendString(const char *str)
{
    if (str == NULL)
    {
        return;
    }
    CDC_Transmit_FS((uint8_t *)str,(uint16_t)strlen(str));
}

/* ============================================================
 * 从字符串中查找指定字段
 *
 * 例如：
 *
 * line:
 * $PC,CPU=68,CT=52,RAM=54
 *
 * key:
 * "CPU="
 *
 * 返回：
 * value = 68
 *
 * 支持负数，例如TEMP=-5
 * ============================================================ */

static uint8_t USB_Protocol_GetValue(
    const char *line,
    const char *key,
    int32_t *value
)
{
    const char *p;

    int32_t result = 0;
    int8_t sign = 1;

    uint8_t has_digit = 0;

    if ((line == NULL) ||
        (key == NULL) ||
        (value == NULL))
    {
        return 0;
    }

    /* 查找字段 */
    p = strstr(line, key);

    if (p == NULL)
    {
        return 0;
    }

    /* 跳过字段名 */
    p += strlen(key);

    /* 处理负号 */
    if (*p == '-')
    {
        sign = -1;
        p++;
    }

    /* 读取数字 */
    while ((*p >= '0') && (*p <= '9'))
    {
        has_digit = 1;

        result =
            result * 10 +
            (*p - '0');

        p++;
    }
    /* 至少必须存在一个数字 */
    if (has_digit == 0)
    {
        return 0;
    }

    /* 字段后只允许：或字符串结束 */
    if ((*p != ',') && (*p != '\0'))
    {
        return 0;
    }

    *value = result * sign;

    return 1;
}

/* ============================================================
 * 处理PC状态数据
 *
 * 协议：
 *
 * $PC,CPU=68,CT=52,RAM=54,GPU=72,GT=61,VRAM=45
 * ============================================================ */
static void USB_Protocol_HandlePC(char *line)
{
    UI_PC_Status_t status;
    /* 读取的数据 */
    int32_t cpu;
    int32_t cpu_temp;
    int32_t ram;
    int32_t gpu;
    int32_t gpu_temp;
    int32_t vram;

    /* --------------------------------------------------------
     * 提取全部字段
     * -------------------------------------------------------- */
    if (!USB_Protocol_GetValue(line,"CPU=",&cpu))
    {
        USB_Protocol_SendString("$ERR,FORMAT\r\n");
        return;
    }
    if (!USB_Protocol_GetValue(line,"CT=",&cpu_temp))
    {
        USB_Protocol_SendString("$ERR,FORMAT\r\n");
        return;
    }
    if (!USB_Protocol_GetValue(line,"RAM=",&ram))
    {
        USB_Protocol_SendString("$ERR,FORMAT\r\n");
        return;
    }
    if (!USB_Protocol_GetValue(line,"GPU=",&gpu))
    {
        USB_Protocol_SendString("$ERR,FORMAT\r\n");
        return;
    }
    if (!USB_Protocol_GetValue(line,"GT=",&gpu_temp))
    {
        USB_Protocol_SendString("$ERR,FORMAT\r\n");
        return;
    }
    if (!USB_Protocol_GetValue(line,"VRAM=",&vram))
    {
        USB_Protocol_SendString("$ERR,FORMAT\r\n");
        return;
    }

    /* --------------------------------------------------------
     * 参数范围检查
     * -------------------------------------------------------- */
    if ((cpu < 0) || (cpu > 100) ||
        (ram < 0) || (ram > 100) ||
        (gpu < 0) || (gpu > 100) ||
        (vram < 0) || (vram > 100))
    {
        USB_Protocol_SendString("$ERR,RANGE\r\n");
        return;
    }
    /* 温度范围留得稍宽一些。 */
    if ((cpu_temp < -40) || (cpu_temp > 150) ||
        (gpu_temp < -40) || (gpu_temp > 150))
    {
        USB_Protocol_SendString("$ERR,RANGE\r\n" );
        return;
    }

    /* --------------------------------------------------------
     * 转换成UI数据结构
     * -------------------------------------------------------- */
    status.cpu_usage = (uint8_t)cpu;
    status.ram_usage = (uint8_t)ram;
    status.cpu_temp  = (int16_t)cpu_temp;

    status.gpu_usage = (uint8_t)gpu;
    status.vram_usage = (uint8_t)vram;
    status.gpu_temp  = (int16_t)gpu_temp;

    /* --------------------------------------------------------
     * 提交给UI
     * -------------------------------------------------------- */
    UI_SetStatus(&status);

    /* --------------------------------------------------------
     * 返回ACK
     * -------------------------------------------------------- */
    USB_Protocol_SendString("$ACK,PC\r\n");
}

/* ============================================================
 * 页面控制
 *
 * $PAGE,NEXT   // 下一页
 * $PAGE,CPU    // 翻到CPU界面
 * $PAGE,GPU    // 翻到GPU界面
 * ============================================================ */
static void USB_Protocol_HandlePage(char *line)
{
    if (strcmp(line,"$PAGE,NEXT") == 0)
    {
        UI_NextPage();
        USB_Protocol_SendString("$ACK,PAGE\r\n");
        return;
    }

    if (strcmp(line,"$PAGE,CPU") == 0)
    {
        UI_SetPage(UI_PAGE_CPU);
        USB_Protocol_SendString("$ACK,PAGE\r\n");
        return;
    }

    if (strcmp(line,"$PAGE,GPU") == 0)
    {
        UI_SetPage(UI_PAGE_GPU);
        USB_Protocol_SendString("$ACK,PAGE\r\n");
        return;
    }

    USB_Protocol_SendString("$ERR,CMD\r\n");
}

/* ============================================================
 * 一条完整协议处理
 * ============================================================ */
static void USB_Protocol_HandleLine(char *line)
{
    if ((line == NULL) || (line[0] == '\0'))
    {
        return;
    }

    /* --------------------------------------------------------
     * 通信测试
     * -------------------------------------------------------- */
    if (strcmp(line,"$PING") == 0)
    {
        USB_Protocol_SendString("$ACK,PONG\r\n");
        return;
    }
    /* --------------------------------------------------------
     * PC状态数据
     * -------------------------------------------------------- */
    if (strncmp(line,"$PC,", 4) == 0)
    {
        USB_Protocol_HandlePC(line);
        return;
    }
    /* --------------------------------------------------------
     * 页面控制
     * -------------------------------------------------------- */
    if (strncmp(line,"$PAGE,",6) == 0)
    {
        USB_Protocol_HandlePage(line);
        return;
    }
    /* --------------------------------------------------------
     * 未识别命令
     * -------------------------------------------------------- */
    USB_Protocol_SendString("$ERR,CMD\r\n");
}

/* ============================================================
 * 初始化
 * ============================================================ */
void USB_Protocol_Init(void)
{
    memset(s_protocol_line,0,sizeof(s_protocol_line));

    s_protocol_line_len = 0;

    /*
     * 清除CDC底层可能残留的数据标志
     */
    CDC_ClearRXNum();
}

/* ============================================================
 * USB协议轮询
 *
 * main while(1)持续调用。
 *
 * 处理流程：
 *
 * CDC底层接收
 *      ↓
 * 获取本次USB数据块
 *      ↓
 * 拼接到行缓存
 *      ↓
 * 遇到\n
 *      ↓
 * 得到完整协议
 *      ↓
 * USB_Protocol_HandleLine()
 * ============================================================ */

void USB_Protocol_Process(void)
{
    uint16_t rx_num;
    uint8_t *rx_data;
    uint16_t i;

    /* 查询CDC底层是否接收到数据 */
    rx_num = CDC_GetRXNum();
    if (rx_num == 0)
    {
        return;
    }

    /* 获取CDC接收缓存地址 */
    rx_data = CDC_GetRXData();
    if (rx_data == NULL)
    {
        CDC_ClearRXNum();
        return;
    }

    /* 逐字节解析 */
    for (i = 0; i < rx_num; i++)
    {
        char ch;
        ch = (char)rx_data[i];

        /* 忽略 \r */
        if (ch == '\r')
        {
            continue;
        }

        /*
         * 收到 \n：
         *
         * 表示一帧完整协议结束。
         */
        if (ch == '\n')
        {
            /* 添加字符串结束符 */
            s_protocol_line[s_protocol_line_len] = '\0';

            /* 处理这一帧 */
            if (s_protocol_line_len > 0)
            {
                USB_Protocol_HandleLine(s_protocol_line);
            }

            /*  为下一帧重新开始 */
            s_protocol_line_len = 0;
            continue;
        }

        /*
         * 普通字符加入缓存
         */
        if (s_protocol_line_len < (USB_PROTOCOL_LINE_SIZE - 1))
        {
            s_protocol_line[s_protocol_line_len] = ch;
            s_protocol_line_len++;
        }
        else
        {
            /*
             * 数据帧过长：
             * 丢弃当前帧。
             */
            s_protocol_line_len = 0;

            USB_Protocol_SendString("$ERR,TOOLONG\r\n"
            );
        }
    }

    /* 本次CDC数据已经处理完。*/
    CDC_ClearRXNum();
}
