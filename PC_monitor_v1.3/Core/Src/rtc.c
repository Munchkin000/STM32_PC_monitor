/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    rtc.c
  * @brief   This file provides code for the configuration
  *          of the RTC instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "rtc.h"

/* USER CODE BEGIN 0 */
/*
 * RTC已经初始化完成的标志。
 *
 * 存放在VBAT供电的Backup Register中。
 */
#define RTC_BKP_MAGIC_VALUE    0xA55CU
/*
 * ============================================================
 * 保存RTC日期到Backup Register
 *
 * BKP_DR2:
 *
 * [15:8] Year
 * [7:0]  Month
 *
 * BKP_DR3:
 *
 * [15:8] Date
 * [7:0]  WeekDay
 * ============================================================
 */
void RTC_BackupDate_Save(const RTC_DateTypeDef *date)
{
    uint16_t value;
    if (date == NULL)
    {
        return;
    }
    /*
     * 保存Year和Month
     */
    value = ((uint16_t)date->Year << 8) | ((uint16_t)date->Month);
    HAL_RTCEx_BKUPWrite(&hrtc,RTC_BKP_DR2,value);
    /*
     * 保存Date和WeekDay
     */
    value = ((uint16_t)date->Date << 8) | ((uint16_t)date->WeekDay);
    HAL_RTCEx_BKUPWrite(&hrtc,RTC_BKP_DR3,value);
}
/*
 * ============================================================
 * 从Backup Register恢复RTC日期
 * ============================================================
 */
void RTC_BackupDate_Load(RTC_DateTypeDef *date)
{
    uint16_t value;
    if (date == NULL)
    {
        return;
    }

    /*
     * Year + Month
     */
    value = (uint16_t)HAL_RTCEx_BKUPRead(&hrtc,RTC_BKP_DR2);
    date->Year = (uint8_t)((value >> 8) & 0xFF);
    date->Month = (uint8_t)(value & 0xFF);

    /*
     * Date + WeekDay
     */
    value = (uint16_t)HAL_RTCEx_BKUPRead(&hrtc,RTC_BKP_DR3
    );
    date->Date = (uint8_t)((value >> 8) & 0xFF);
    date->WeekDay = (uint8_t)(value & 0xFF);
}
/* 只在日期变化时刷新日期 */
void RTC_BackupDate_Update(const RTC_DateTypeDef *date)
{
    static uint8_t last_year  = 0xFF;
    static uint8_t last_month = 0xFF;
    static uint8_t last_date  = 0xFF;

    if (date == NULL)
    {
        return;
    }
    /*
     * 日期没有发生变化，
     * 不重复写Backup Register。
     */
    if ((last_year  == date->Year) && (last_month == date->Month) && (last_date  == date->Date))
    {
        return;
    }

    /*
     * 日期发生变化
     */
    last_year  = date->Year;
    last_month = date->Month;
    last_date  = date->Date;

    RTC_BackupDate_Save(date);
}
/* USER CODE END 0 */

RTC_HandleTypeDef hrtc;

/* RTC init function */
void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef DateToUpdate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.AsynchPrediv = RTC_AUTO_1_SECOND;
  hrtc.Init.OutPut = RTC_OUTPUTSOURCE_ALARM;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */
	/*
	 * 检查RTC是否已经初始化过。
	 *
	 * 如果BKP_DR1中不存在有效标志，
	 * 说明：
	 *
	 * 1. 第一次启动；
	 * 2. VBAT曾经完全掉电；
	 * 3. Backup Domain曾被复位。
	 *
	 * 此时才重新设置RTC时间。
	 */
	if (HAL_RTCEx_BKUPRead(
			&hrtc,
			RTC_BKP_DR1) != RTC_BKP_MAGIC_VALUE)
	{
  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0x17;
  sTime.Minutes = 0x5;
  sTime.Seconds = 0x0;

  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  DateToUpdate.WeekDay = RTC_WEEKDAY_MONDAY;
  DateToUpdate.Month = RTC_MONTH_SEPTEMBER;
  DateToUpdate.Date = 0x14;
  DateToUpdate.Year = 0x26;

  if (HAL_RTC_SetDate(&hrtc, &DateToUpdate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */
		/*
		 * HAL_RTC_SetDate上面使用的是BCD。
		 *
		 * 为了Backup Register统一保存普通BIN数据，
		 * 这里重新读取一次日期。
		 */
		if (HAL_RTC_GetDate(
				&hrtc,
				&DateToUpdate,
				RTC_FORMAT_BIN) == HAL_OK)
		{
			RTC_BackupDate_Save(
				&DateToUpdate
			);
		}
		/*
		 * RTC时间和日期设置成功以后，
		 * 写入初始化完成标志。
		 *
		 * 只要VBAT没有掉电，
		 * 这个值在VDD断电后仍然存在。
		 */
		HAL_RTCEx_BKUPWrite(
			&hrtc,
			RTC_BKP_DR1,
			RTC_BKP_MAGIC_VALUE
		);
	}
	else
    {
        /*
         * ====================================================
         * RTC已经初始化过：
         *
         * 硬件RTC计数器由VBAT保持，
         * 不再调用SetTime。
         *
         * 这里只恢复F103 HAL的软件日期状态。
         * ====================================================
         */
        RTC_BackupDate_Load(&DateToUpdate);


        /*
         * STM32F1的HAL日期由DateToUpdate维护。
         *
         * 此处不能调用HAL_RTC_SetDate()，
         * 否则可能重新修改RTC计数值。
         *
         * 直接恢复HAL的软件日期状态。
         */
        hrtc.DateToUpdate.Year = DateToUpdate.Year;
        hrtc.DateToUpdate.Month = DateToUpdate.Month;
        hrtc.DateToUpdate.Date = DateToUpdate.Date;
        hrtc.DateToUpdate.WeekDay = DateToUpdate.WeekDay;
    }
  /* USER CODE END RTC_Init 2 */

}

void HAL_RTC_MspInit(RTC_HandleTypeDef* rtcHandle)
{

  if(rtcHandle->Instance==RTC)
  {
  /* USER CODE BEGIN RTC_MspInit 0 */

  /* USER CODE END RTC_MspInit 0 */
    HAL_PWR_EnableBkUpAccess();
    /* Enable BKP CLK enable for backup registers */
    __HAL_RCC_BKP_CLK_ENABLE();
    /* RTC clock enable */
    __HAL_RCC_RTC_ENABLE();
  /* USER CODE BEGIN RTC_MspInit 1 */

  /* USER CODE END RTC_MspInit 1 */
  }
}

void HAL_RTC_MspDeInit(RTC_HandleTypeDef* rtcHandle)
{

  if(rtcHandle->Instance==RTC)
  {
  /* USER CODE BEGIN RTC_MspDeInit 0 */

  /* USER CODE END RTC_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_RTC_DISABLE();
  /* USER CODE BEGIN RTC_MspDeInit 1 */

  /* USER CODE END RTC_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
