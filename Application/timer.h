#ifndef __TIMER_H
#define __TIMER_H


#include "stdint.h"
#include "status.h"

status_t Timer_Init(void);
void Timer_GetCurrentTime(RTC_TimeTypeDef* time, RTC_DateTypeDef* date);





#endif /* __TIMER_H */
