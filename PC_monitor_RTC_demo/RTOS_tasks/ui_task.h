#ifndef __UI_TASK_H__
#define __UI_TASK_H__

#include "ui_data.h"
#include <stdint.h>

/*
 * UI页面
 */
typedef enum
{
    UI_PAGE_CPU = 0,

    UI_PAGE_GPU,

	UI_PAGE_RTC,
	
    UI_PAGE_MAX

} UI_Page_t;


/*
 * 初始化UI管理器
 */
void UI_Init(void);

/*
 * UI周期处理
 *
 * 裸机：
 * main while循环调用
 *
 * RTOS：
 * UI_Task循环调用
 */
void UI_Process(void);


/*
 * 下一页
 */
void UI_NextPage(void);


/*
 * 上一页
 */
void UI_PreviousPage(void);


/*
 * 指定页面
 */
void UI_SetPage(UI_Page_t page);


/*
 * 更新PC状态数据
 */
void UI_SetStatus(const UI_PC_Status_t *data);


/*
 * 获取当前页面
 */
UI_Page_t UI_GetPage(void);

#endif
