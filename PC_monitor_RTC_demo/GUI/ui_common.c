#include "ui_common.h"

/*
 * 显示无前导0整数
 */
uint8_t UI_DrawUInt(int16_t x,
                    int16_t y,
                    uint16_t value,
                    uint8_t font_size)
{
    char buf[6];
    char temp[6];

    uint8_t len = 0;
    uint8_t i = 0;

    /*
     * 处理0
     */
    if (value == 0)
    {
        buf[0] = '0';
        buf[1] = '\0';

        OLED_ShowString(x, y, buf, font_size);

        return 1;
    }

    /*
     * 反向提取数字
     */
    while ((value > 0) && (len < 5))
    {
        temp[len++] = (value % 10) + '0';

        value /= 10;
    }

    /*
     * 重新反转
     */
    for (i = 0; i < len; i++)
    {
        buf[i] = temp[len - i - 1];
    }

    buf[len] = '\0';

    OLED_ShowString(x, y, buf, font_size);

    return len;
}

/*
 * 显示百分比
 *
 * 例如：
 *
 * 68%
 */
void UI_DrawPercent(int16_t x,
                    int16_t y,
                    uint8_t value,
                    uint8_t font_size)
{
    uint8_t len;

    if (value > 100)
    {
        value = 100;
    }

    len = UI_DrawUInt(
        x,
        y,
        value,
        font_size
    );

    OLED_ShowChar(
        x + len * font_size,
        y,
        '%',
        font_size
    );
}

/*
 * 百分比进度条
 */
void UI_DrawProgressBar(int16_t x,
                        int16_t y,
                        uint8_t width,
                        uint8_t height,
                        uint8_t percent)
{
    uint8_t fill_width;

    if (percent > 100)
    {
        percent = 100;
    }

    /*
     * 外框
     */
    OLED_DrawRectangle(
        x,
        y,
        width,
        height,
        OLED_UNFILLED
    );

    /*
     * 计算内部填充宽度
     */
    fill_width =
        (uint16_t)(width - 2) *
        percent /
        100;

    /*
     * 防止0宽度矩形
     */
    if (fill_width > 0)
    {
        OLED_DrawRectangle(
            x + 1,
            y + 1,
            fill_width,
            height - 2,
            OLED_FILLED
        );
    }
}
