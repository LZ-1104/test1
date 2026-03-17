#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include "bsp_debug_usart.h"
#include "stm32f1xx_hal_usart.h"


//日志颜色定义
#define LOG_COLOR_INFO "\033[32m"  // 绿色
#define LOG_COLOR_ERROR "\033[31m" // 红色
#define LOG_COLOR_DEBUG "\033[34m" // 蓝色


void log_Init(void)
{
    // 初始化串口或其他日志输出设备
    DEBUG_USART_Config();
    log_Info("Log system initialized successfully.");
}

//日志输出函数
void Serial_Printf(const char* format, ...)
{
    char buffer[256]; // 定义一个足够大的缓冲区
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args); // 格式化字符串
    va_end(args);
    
    Usart_SendString((uint8_t*)buffer); // 通过串口发送日志
    Usart_SendString((uint8_t*)"\r\n"); // 发送换行符
}


void log_Info(const char* format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    Serial_Printf(LOG_COLOR_INFO "[INFO] %s" "\033[0m", buffer); // 输出绿色信息日志
}

#if LOG_LEVEL >= LOG_LEVEL_ERROR
void log_Error(const char* format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    Serial_Printf(LOG_COLOR_ERROR "[ERROR] %s" "\033[0m", buffer); // 输出红色错误日志
}

#else
void log_Error(const char* format, ...)
{
    // 错误日志被禁用，不执行任何操作
}
#endif // LOG_LEVEL >= LOG_LEVEL_ERROR


#if LOG_LEVEL >= LOG_LEVEL_DEBUG
void log_Debug(const char* format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    Serial_Printf(LOG_COLOR_DEBUG "[DEBUG] %s" "\033[0m", buffer); // 输出蓝色调试日志
}

#else 
void log_Debug(const char* format, ...)
{
    // 调试日志被禁用，不执行任何操作
}
#endif // LOG_LEVEL >= LOG_LEVEL_DEBUG

