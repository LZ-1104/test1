#ifndef __LOG_H
#define __LOG_H


#include <stdio.h>

//日志级别定义
#define LOG_LEVEL_ERROR 0
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_DEBUG 2
//日志级别设置
#define LOG_LEVEL LOG_LEVEL_DEBUG // 默认日志级别为信息


void log_Init(void);
void log_Info(const char* format, ...);
void log_Error(const char* format, ...);
void log_Debug(const char* format, ...);



#endif

