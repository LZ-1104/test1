#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include "bsp_debug_usart.h"
#include "stm32f1xx_hal_usart.h"
#include "ESP8266.h"
#include "lvgl.h"
#include "lv_port_disp.h"

//日志颜色定义
#define LOG_COLOR_INFO "\033[32m"  // 绿色
#define LOG_COLOR_ERROR "\033[31m" // 红色
#define LOG_COLOR_DEBUG "\033[34m" // 蓝色



void log_Init(void)
{
#if LOG_HARDWARE == LOG_HARDWARE_UART
    // 初始化串口
    DEBUG_USART_Config();
    log_Info("Log system initialized successfully on UART.");
    printf("Log system initialized successfully on UART writed by printf.\r\n");
#elif LOG_HARDWARE == LOG_HARDWARE_LCD
    // 初始化LCD显示
    lv_init();
    lv_port_disp_init();
    log_Info("Log system initialized successfully on LCD.");
#elif LOG_HARDWARE == LOG_HARDWARE_WIFI
    // 初始化WiFi日志发送
    ESP8266_Init();
     if(ESP8266_CheckAlive() == false) {
        log_Debug("ESP8266模块无响应,日志系统初始化失败");
    } else {
    log_Info("Log system initialized successfully on WiFi.");
    }
#endif
}



/**
 * @brief 使用CMAKE配置printf重定向时，必须实现_write函数来处理标准输出。
 * @param file 
 * @param ptr 
 * @param len 
 * @return 
 */
int _write(int file, char *ptr, int len)
{
    (void)file;
    for (int DataIdx = 0; DataIdx < len; DataIdx++)
    {
        fputc(*ptr++, NULL);
    }
    return len;
}



int fputc(int _Ch,FILE *_File)
{
#if LOG_HARDWARE == LOG_HARDWARE_UART
    // 通过UART发送日志字符
    HAL_UART_Transmit(&huart1, (uint8_t *)&_Ch, 1, 1000);
#elif LOG_HARDWARE == LOG_HARDWARE_LCD
    // 通过LCD显示日志字符
    // 这里需要实现LCD显示函数，例如：
    
#elif LOG_HARDWARE == LOG_HARDWARE_WIFI
    // 通过WiFi发送日志字符
    // 这里需要实现WiFi发送函数，例如：
    ESP8266_SendTcpData((uint8_t *)&_Ch, 1);
#endif
    return _Ch;
}


//日志输出函数
void LOG_Printf(const char* format, ...)
{
    char buffer[256]; // 定义一个足够大的缓冲区
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args); // 格式化字符串
    va_end(args);
#if LOG_HARDWARE == LOG_HARDWARE_UART
    Usart_SendString((uint8_t*)buffer); // 通过串口发送日志
    Usart_SendString((uint8_t*)"\r\n"); // 发送换行符
#elif LOG_HARDWARE == LOG_HARDWARE_LCD
    // 通过LCD显示日志
    // 这里需要实现LCD显示函数，例如：
#elif LOG_HARDWARE == LOG_HARDWARE_WIFI
    // 通过WiFi发送日志
    ESP8266_SendTcpData((uint8_t*)buffer, strlen(buffer)); // 发送日志数据
    ESP8266_SendTcpData((uint8_t*)"\r\n", 2); // 发送换行符
#endif
}


void log_Info(const char* format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    // LOG_Printf(LOG_COLOR_INFO "[INFO] %s" "\033[0m", buffer); // 输出绿色信息日志
    printf(LOG_COLOR_INFO "[INFO] %s" "\033[0m\r\n", buffer); // 输出绿色信息日志并换行
}

#if LOG_LEVEL >= LOG_LEVEL_ERROR
void log_Error(const char* format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    // LOG_Printf(LOG_COLOR_ERROR "[ERROR] %s" "\033[0m", buffer); // 输出红色错误日志
    printf(LOG_COLOR_ERROR "[ERROR] %s" "\033[0m\r\n", buffer); // 输出红色错误日志并换行
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
    
    // LOG_Printf(LOG_COLOR_DEBUG "[DEBUG] %s" "\033[0m", buffer); // 输出蓝色调试日志
    printf(LOG_COLOR_DEBUG "[DEBUG] %s" "\033[0m\r\n", buffer); // 输出蓝色调试日志并换行
}

#else 
void log_Debug(const char* format, ...)
{
    // 调试日志被禁用，不执行任何操作
}
#endif // LOG_LEVEL >= LOG_LEVEL_DEBUG

