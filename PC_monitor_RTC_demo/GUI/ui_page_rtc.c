#include "ui_page_rtc.h"
#include "OLED.h"
#include "rtc.h"
#include "ui_data.h"

#include <stdint.h>

/*
 * 将0~99的数转换为两个ASCII字符
 *
 * 例如：
 * value = 8
 *
 * buf[0] = '0'
 * buf[1] = '8'
 */
static void UI_RTC_U8To2Char(char *buf, uint8_t value)
{
    buf[0] = (char)('0' + value / 10);
    buf[1] = (char)('0' + value % 10);
}

/*
 * 生成时间字符串
 *
 * 格式：
 * HH:MM:SS
 *
 * 例如：
 * 23:55:08
 */
static void UI_RTC_MakeTimeString(char *buf,const RTC_TimeTypeDef *time)
{
    UI_RTC_U8To2Char(&buf[0], time->Hours);
    buf[2] = ':';

    UI_RTC_U8To2Char(&buf[3], time->Minutes);
    buf[5] = ':';

    UI_RTC_U8To2Char(&buf[6], time->Seconds);
    buf[8] = '\0';
}

/*
 * 生成日期字符串
 *
 * 格式：
 * YYYY-MM-DD
 *
 * STM32 RTC中Year范围为0~99，
 * 这里按照2000~2099处理。
 *
 * 例如：
 * 2026-08-31
 */
static void UI_RTC_MakeDateString(char *buf,const RTC_DateTypeDef *date)
{
    buf[0] = '2';
    buf[1] = '0';

    UI_RTC_U8To2Char(&buf[2], date->Year);
    buf[4] = '-';

    UI_RTC_U8To2Char(&buf[5], date->Month);
    buf[7] = '-';

    UI_RTC_U8To2Char(&buf[8], date->Date);
    buf[10] = '\0';
}

/*
 * RTC / 环境数据显示页面
 *
 * 页面布局：
 *
 *  +-----------------------------+
 *  | RTC / ENV              3/3  |
 *  +-----------------------------+
 *  |                             |
 *  |      23:55:08               |
 *  |                             |
 *  |       2026-08-31            |
 *  | TEMP --C       HUM --%      |
 *  +-----------------------------+
 *
 * 当前温湿度传感器尚未安装，
 * TEMP和HUM区域只预留数据显示位置。
 */
void UI_PageRTC_Draw(void)
{
    RTC_TimeTypeDef rtc_time = {0};
    RTC_DateTypeDef rtc_date = {0};

    char time_buf[9];
    char date_buf[11];

    /* ========================================================
     * 读取RTC时间
     *
     * 建议顺序：
     * HAL_RTC_GetTime()
     * HAL_RTC_GetDate()
     * ======================================================== */
    if (HAL_RTC_GetTime(&hrtc,&rtc_time,RTC_FORMAT_BIN) != HAL_OK)
    {
        return;
    }

    if (HAL_RTC_GetDate(&hrtc,&rtc_date,RTC_FORMAT_BIN) != HAL_OK)
    {
        return;
    }

    /* 生成显示字符串 */
    UI_RTC_MakeTimeString(time_buf,&rtc_time);
    UI_RTC_MakeDateString(date_buf,&rtc_date);


    /* ========================================================
     * 顶部标题
     * ======================================================== */
    OLED_ShowString(
        0,
        0,
        "RTC / ENV",
        OLED_6X8
    );

    OLED_ShowString(
        110,
        0,
        "3/3",
        OLED_6X8
    );

    /* 顶部分隔线 */
    OLED_DrawLine(
        0,
        9,
        127,
        9
    );


    /* ========================================================
     * 时间显示
     *
     * 12x24字体
     * HH:MM:SS共8个字符
     *
     * 宽度：
     * 8 x 12 = 96 pixel
     *
     * x = 16后正好居中
     * ======================================================== */
    OLED_ShowString(
        16,
        12,
        time_buf,
        OLED_12X24
    );


    /* ========================================================
     * 日期显示
     *
     * YYYY-MM-DD
     *
     * 10个字符 x 6 pixel = 60 pixel
     * x = 34基本居中
     * ======================================================== */
    OLED_ShowString(
        34,
        39,
        date_buf,
        OLED_6X8
    );


    /* ========================================================
     * 温度预留区域
     *
     * 当前没有温湿度传感器：
     *
     * TEMP --C
     * ======================================================== */
    OLED_ShowString(
        0,
        53,
        "TEMP --C",
        OLED_6X8
    );


    /* ========================================================
     * 湿度预留区域
     *
     * HUM --%
     * ======================================================== */
    OLED_ShowString(
        78,
        53,
        "HUM --%",
        OLED_6X8
    );
}
