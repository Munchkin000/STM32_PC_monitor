#include "ui_page_gpu.h"

#include "OLED.h"
#include "ui_common.h"

void UI_PageGPU_Draw(const UI_PC_Status_t *data)
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
        "GPU / VRAM",
        OLED_6X8
    );
    OLED_ShowString(
        110,
        0,
        "2/2",
        OLED_6X8
    );
    OLED_DrawLine(
        0,
        9,
        127,
        9
    );

    /* ========================================================
     * GPU区域
     * ======================================================== */

    OLED_ShowString(
        0,
        14,
        "GPU",
        OLED_8X16
    );
    UI_DrawPercent(
        28,
        10,
        data->gpu_usage,
        OLED_12X24
    );

    /* ========================================================
     * GPU温度
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
        (uint16_t)data->gpu_temp,
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
     * VRAM区域
     * ======================================================== */
    OLED_ShowString(
        0,
        42,
        "VRAM",
        OLED_8X16
    );
    UI_DrawPercent(
        36,
        46,
        data->vram_usage,
        OLED_6X8
    );
    UI_DrawProgressBar(
        68,
        44,
        59,
        12,
        data->vram_usage
    );
}
