#include <stdio.h>
#include "OLED.h"
#include "ui_task.h"
#include "ui_page_cpu.h"
#include "ui_page_gpu.h"
#include "ui_page_rtc.h"
/* FreeRTOS头文件 */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

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
/* ============================================================
 * UI任务通信对象
 * ============================================================ */
/*
 * PC状态Queue。
 *
 * 长度固定为1：  PC监控只关心最新状态，不需要积压历史帧。
 *
 * USB发送速度快于OLED刷新时， 新数据直接覆盖旧数据。
 */
static QueueHandle_t s_ui_status_queue = NULL;

/*
 * PC状态数据
 *
 * 目前直接设置测试值，后续由USB Task更新。
 */
static UI_PC_Status_t s_pc_status =
{
    .cpu_usage  = 88,
    .ram_usage  = 88,
    .cpu_temp   = 88,

    .gpu_usage  = 88,
    .vram_usage = 88,
    .gpu_temp   = 88
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
         /* ====================================================
         * CPU页面
         * ==================================================== */
        case UI_PAGE_CPU:
        {
            UI_PageCPU_Draw(&s_pc_status);
            break;
        }
        /* ====================================================
         * GPU页面
         * ==================================================== */
        case UI_PAGE_GPU:
        {
            UI_PageGPU_Draw(&s_pc_status);
            break;
        }
        /* ====================================================
         * RTC / 温湿度页面
         * ==================================================== */
        case UI_PAGE_RTC:
        {
            UI_PageRTC_Draw();
            break;
        }
        /* ====================================================
         * 异常页面
         * ==================================================== */
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

	/* ========================================================
     * 创建PC状态Queue
     *
     * 长度必须为1，因为后面使用xQueueOverwrite()
     * ======================================================== */
    if (s_ui_status_queue == NULL)
    {
        s_ui_status_queue = xQueueCreate(1,sizeof(UI_PC_Status_t));

        if (s_ui_status_queue == NULL)
        {
            printf("[UI] Status queue create failed\r\n");

            /*
             * Queue创建失败时暂时保持默认UI数据。
             *
             * 后续USB发送数据时UI_PostStatus()会返回失败。
             */
        }
        else
        {
            printf("[UI] Status queue create OK\r\n");
        }
    }
    /*
     * 立即绘制第一页
     */
    UI_Process();
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
 * 向UI Task提交PC状态数据
 *
 * 注意：
 *
 * 此函数可以被USB_Task调用，但不会直接修改UI内部的s_pc_status。
 *
 * Queue长度为1，因此使用xQueueOverwrite()。
 *
 * 如果上一帧数据UI还没来得及处理，新数据会覆盖上一帧。
 *
 * 对PC性能监控而言，只需要最新状态。
 * ============================================================ */
uint8_t UI_PostStatus(const UI_PC_Status_t *data)
{
    if (data == NULL)
    {
        return 0;
    }

    if (s_ui_status_queue == NULL)
    {
        return 0;
    }
	/* 向队列写入状态数据 */
    if (xQueueOverwrite(s_ui_status_queue,data) != pdPASS)
    {
        return 0;
    }

    return 1;
}
/* ============================================================
 * 获取当前页面
 * ============================================================ */
UI_Page_t UI_GetPage(void)
{
    return s_current_page;
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
 * UI刷新任务
 * UI状态数据的唯一消费者。
 *
 * 原则：
 * 只有UI_Task允许：
 * 1. 修改s_pc_status
 * 2. 根据数据设置s_ui_dirty
 * 3. 执行OLED刷新
 * ============================================================ */
void UI_task(void *pvParameters)
{
    UI_PC_Status_t new_status;
    TickType_t rtc_last_refresh;

    (void)pvParameters;

    /*
     * 记录RTC页面上一次刷新时间
     */
    rtc_last_refresh = xTaskGetTickCount();

    printf("UI_Task start\r\n");

    while (1)
    {
        /* ====================================================
         * 等待USB Task提交新的PC状态
         *
         * 最大阻塞5ms。
         *
         * 即使没有PC数据，
         * UI Task仍然会周期性醒来，
         * 从而能够处理：
         *
         * 1. 页面切换
         * 2. RTC页面刷新
         * ==================================================== */
        if (s_ui_status_queue != NULL)
        {
            if (xQueueReceive(s_ui_status_queue,&new_status,pdMS_TO_TICKS(5)) == pdPASS)
            {
                /*
                 * 更新PC状态
                 */
                s_pc_status = new_status;

                /*
                 * PC数据发生变化，
                 * 请求重新绘制。
                 */
                s_ui_dirty = 1;
            }
        }
        else
        {
            /*
             * Queue初始化失败，
             * 防止任务死循环占满CPU。
             */
            vTaskDelay(pdMS_TO_TICKS(5));
        }

        /* ====================================================
         * RTC页面周期刷新
         *
         * RTC硬件会一直运行，
         * 但是OLED需要主动重新绘制。
         * ==================================================== */
        if (s_current_page == UI_PAGE_RTC)
        {
            TickType_t now;
            now = xTaskGetTickCount();

            if ((now - rtc_last_refresh) >= pdMS_TO_TICKS(UI_RTC_REFRESH_PERIOD_MS))
            {
                rtc_last_refresh = now;
                /*
                 * 请求重新读取RTC并刷新OLED
                 */
                s_ui_dirty = 1;
            }
        }
        else
        {
            /*
             * 离开RTC页面后更新基准时间，
             * 防止再次进入RTC页时出现异常的大时间差。
             */
            rtc_last_refresh = xTaskGetTickCount();
        }
        /* ====================================================
         * 刷新UI
         * ==================================================== */
        UI_Process();
    }
}
