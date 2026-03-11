/*
功能：提供时间，闹钟，定时器等功能

*/
#include "log.h"
#include "rtc.h"
#include "timer.h"


status_t Timer_Init(void)
{
    log_Debug("Timer_Init: Initializing timer...");
    MX_RTC_Init();
    log_Debug("Timer_Init: RTC initialized successfully.");
    return STATUS_OK;
}

void Timer_GetCurrentTime(RTC_TimeTypeDef* time, RTC_DateTypeDef* date)
{
    HAL_RTC_GetTime(&hrtc, time, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, date, RTC_FORMAT_BIN);
}





