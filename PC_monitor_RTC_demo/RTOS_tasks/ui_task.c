#include "ui_task.h"

#include "OLED.h"

#include "ui_page_cpu.h"
#include "ui_page_gpu.h"
#include "ui_page_rtc.h"


/* ============================================================
 * UI内部状态
 * ============================================================ */

/* 当前页面 */
static UI_Page_t s_current_page = UI_PAGE_CPU;

/*
 * 页面是否需要重新绘制
 *
 * 1：需要
 * 0：不需要
 */
static uint8_t s_ui_dirty = 1;


/*
 * PC状态数据
 *
 * 目前直接设置测试值。
 * 后续由USB Task更新。
 */
static UI_PC_Status_t s_pc_status =
{
    .cpu_usage  = 68,
    .ram_usage  = 54,
    .cpu_temp   = 52,

    .gpu_usage  = 72,
    .vram_usage = 45,
    .gpu_temp   = 61,
	
//	.rtc_year	=	2000,      	/* rtc时钟时间年份 */
//    .rtc_month	=	1,      /* rtc时钟时间月份 */
//    .rtc_day	=	1,      	/* rtc时钟时间日期 */
//	
//	.rtc_hour	=	0,      	/* rtc时钟时间小时 */
//    .rtc_minute	=	0,     /* rtc时钟时间分钟 */
//    .rtc_second	=	0,     /* rtc时钟时间秒 */
//	
//	.env_temp	=	25,      	/* 环境温度 */
//    .env_humid	=	20,      /* 环境湿度 */
};


/* ============================================================
 * 内部页面绘制
 * ============================================================ */

static void UI_Render(void)
{
    /*
     * 先清空OLED显存。
     *
     * 注意这里只清RAM framebuffer，
     * OLED_Update以后才真正刷新屏幕。
     */
    OLED_Clear();

    switch (s_current_page)
    {
        case UI_PAGE_CPU:
        {
            UI_PageCPU_Draw(
                &s_pc_status
            );

            break;
        }
        case UI_PAGE_GPU:
        {
            UI_PageGPU_Draw(
                &s_pc_status
            );

            break;
        }
		case UI_PAGE_RTC:
		{
			UI_PageRTC_Draw();
			break;
		}
        default:
        {
            s_current_page = UI_PAGE_CPU;
            UI_PageCPU_Draw(&s_pc_status);
            break;
        }
    }

    /*
     * 整个页面绘制完毕后，
     * 一次性刷新OLED。
     */
    OLED_Update();
}


/* ============================================================
 * UI初始化
 * ============================================================ */

void UI_Init(void)
{
    s_current_page = UI_PAGE_CPU;

    s_ui_dirty = 1;

    /*
     * 立即绘制第一页
     */
    UI_Process();
}


/* ============================================================
 * UI周期处理
 * ============================================================ */

void UI_Process(void)
{
    /*
     * 页面或数据没有变化时不刷新OLED，
     * 减少I2C通信。
     */
    if (s_ui_dirty == 0)
    {
        return;
    }

    UI_Render();

    s_ui_dirty = 0;
}


/* ============================================================
 * 下一页
 * ============================================================ */

void UI_NextPage(void)
{
    s_current_page++;

    if (s_current_page >= UI_PAGE_MAX)
    {
        s_current_page = UI_PAGE_CPU;
    }

    s_ui_dirty = 1;
}


/* ============================================================
 * 上一页
 * ============================================================ */

void UI_PreviousPage(void)
{
    if (s_current_page == UI_PAGE_CPU)
    {
        s_current_page =
            (UI_Page_t)(UI_PAGE_MAX - 1);
    }
    else
    {
        s_current_page--;
    }

    s_ui_dirty = 1;
}


/* ============================================================
 * 指定页面
 * ============================================================ */

void UI_SetPage(UI_Page_t page)
{
    if (page >= UI_PAGE_MAX)
    {
        return;
    }

    if (page != s_current_page)
    {
        s_current_page = page;

        s_ui_dirty = 1;
    }
}


/* ============================================================
 * 更新PC运行数据
 * ============================================================ */

void UI_SetStatus(const UI_PC_Status_t *data)
{
    if (data == NULL)
    {
        return;
    }

    /*
     * 结构体复制
     */
    s_pc_status = *data;

    /*
     * 请求下一次UI_Process重新绘制
     */
    s_ui_dirty = 1;
}


/* ============================================================
 * 获取当前页面
 * ============================================================ */

UI_Page_t UI_GetPage(void)
{
    return s_current_page;
}
