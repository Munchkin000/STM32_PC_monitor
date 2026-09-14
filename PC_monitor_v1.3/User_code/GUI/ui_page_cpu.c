#include "ui_page_cpu.h"

#include "OLED.h"
#include "ui_common.h"

void UI_PageCPU_Draw(const UI_PC_Status_t *data)
{
    if (data == NULL)
    {
        return;
    }

    /* ========================================================
     * 顶部标题
     * ======================================================== */
    OLED_ShowString(
        0,
        0,
        "CPU / RAM",
        OLED_6X8
    );
    OLED_ShowString(
        110,
        0,
        "1/3",
        OLED_6X8
    );
    /* 分割线 */
    OLED_DrawLine(
        0,
        9,
        127,
        9
    );

    /* ========================================================
     * CPU区域
     * ======================================================== */
    OLED_ShowString(
        0,
        14,
        "CPU",
        OLED_8X16
    );
    /* 使用12x24大字显示CPU占用率 */
    UI_DrawPercent(
        28,
        10,
        data->cpu_usage,
        OLED_12X24
    );

    /* ========================================================
     * CPU温度
     * ======================================================== */
    OLED_ShowString(
        82,
        12,
        "TEMP",
        OLED_6X8
    );
    UI_DrawUInt(
        84,
        23,
        (uint16_t)data->cpu_temp,
        OLED_6X8
    );
    OLED_ShowChar(
        102,
        23,
        'C',
        OLED_6X8
    );
	OLED_ShowImage(112,18,16,16,Degree);
    /* ========================================================
     * RAM区域
     * ======================================================== */
    OLED_ShowString(
        0,
        42,
        "RAM",
        OLED_8X16
    );
    UI_DrawPercent(
        28,
        46,
        data->ram_usage,
        OLED_6X8
    );
    UI_DrawProgressBar(
        60,
        44,
        67,
        12,
        data->ram_usage
    );
}
