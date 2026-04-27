/**
 ******************************************************************************
 * @file    ESP8266.c
 * @author  Copilot
 * @brief   ESP8266 WiFi模块驱动源文件。
 *          包含精简的底层引脚操作与基于USART的高级AT命令封包逻辑。
 ******************************************************************************
 */
#include "ESP8266.h"
#include "log.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

 /* 用于接收来自ESP8266串口数据的缓冲区大小 */
#define ESP8266_RX_BUFFER_SIZE 256U

/**
 * @brief  单字节轮询读取ESP8266返回的数据，放入缓冲区中。
 * @note   由于ESP8266存在分段回包现象，每次读到新字节时都会重置超时倒计时。
 * @param  rx_buf:      接收缓冲区指针
 * @param  rx_buf_size: 缓冲区最大长度
 * @param  timeout:     字节间最大等待超时（毫秒）
 */
static void ESP8266_ReadResponse(uint8_t* rx_buf, uint16_t rx_buf_size, uint32_t timeout, const char* ack) {
    uint16_t idx = 0U;
    uint8_t ch = 0U;
    uint32_t start_tick = HAL_GetTick( );

    if (rx_buf == NULL || rx_buf_size == 0U) {
        return;
    }

    memset(rx_buf, 0, rx_buf_size);

    while ((HAL_GetTick( ) - start_tick) < timeout) {
        if (HAL_UART_Receive(&ESP8266_UART_HANDLE, &ch, 1U, 10U) == HAL_OK) {
            if (idx < (rx_buf_size - 1U)) {
                rx_buf[idx++] = ch;
                rx_buf[idx] = '\0'; /* 持续保持字符串终止符结尾，方便后续字符串匹配 */
            }
            /* 收到期望的内容，立即跳出，不再傻等 */
            if (ack != NULL && strstr((const char*)rx_buf, ack) != NULL) {
                break;
            }
            /* 如果没指定 ack，只要收到数据就刷新超时继续收完未知长的数据 */
            if (ack == NULL) {
                start_tick = HAL_GetTick( );
            }
        }
    }
}

/**
 * @brief  拉高EN(使能)引脚，开启模块电源。
 */
void ESP8266_PowerOn(void) {
    HAL_GPIO_WritePin(ESP8266_EN_GPIO_PORT, ESP8266_EN_GPIO_PIN, GPIO_PIN_SET);
}

/**
 * @brief  拉低EN(使能)引脚，断开模块电源。
 */
void ESP8266_PowerOff(void) {
    HAL_GPIO_WritePin(ESP8266_EN_GPIO_PORT, ESP8266_EN_GPIO_PIN, GPIO_PIN_RESET);
}

/**
 * @brief  执行硬件复位时序：拉低RST引脚，延时后拉高（低电平复位）。
 */
void ESP8266_HardReset(void) {
    HAL_GPIO_WritePin(ESP8266_RST_GPIO_PORT, ESP8266_RST_GPIO_PIN, GPIO_PIN_RESET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(ESP8266_RST_GPIO_PORT, ESP8266_RST_GPIO_PIN, GPIO_PIN_SET);
    HAL_Delay(500); /* 预留充足时间等待模块固件完成硬重启 */
}

/**
 * @brief  初始化模块：执行上电并做一次完整的硬件复位。
 */
void ESP8266_Init(void) {
    ESP8266_PowerOn( );
    HAL_Delay(50);
    ESP8266_HardReset( );
}

/* ==================================================================== */
/*                                AT命令接口                            */
/* ==================================================================== */

/// @brief
/// @param cmd 该参数如果非NULL，则先发送这个命令字符串（通常是以 \r\n 结尾的AT指令），再进入接收等待；如果为NULL，则直接进入接收等待（适用于发送完命令后单纯等待回应的场景）
/// @param ack 期望在回复中出现的字符串，收到它就提前跳出等待；如果为NULL，则不关心回复内容，只要有回复就继续等待直到超时
/// @param timeout 每次等待接收新字节的超时时间，单位毫秒。注意：每收到一个新字节都会重置这个超时计时器，因此它是字节间的最大允许间隔，而不是总的等待时间上限
/// @return
bool ESP8266_SendCmd(const char* cmd, const char* ack, uint32_t timeout) {
    uint8_t rx_buf[ESP8266_RX_BUFFER_SIZE];

    if (cmd != NULL) {
        /* 清空串口底层可能残留的上次通信的尾部数据（例如多余的 \r\n 或 OK） */
        uint8_t dummy;
        while (HAL_UART_Receive(&ESP8266_UART_HANDLE, &dummy, 1U, 2U) == HAL_OK) {
        }

        /* 1. 通过串口发送原始AT指令 */
        if (HAL_UART_Transmit(&ESP8266_UART_HANDLE, (uint8_t*)cmd, (uint16_t)strlen(cmd), timeout) != HAL_OK) {
            return false;
        }
    }

    /* 2. 读取模块应答的返回信息流，带上ack去快速跳出 */
    ESP8266_ReadResponse(rx_buf, ESP8266_RX_BUFFER_SIZE, timeout, ack);

    /* 3. 如果我们不需要关心它的具体答复，发完能走就算成功 */
    if (ack == NULL) {
        return true;
    }

    /* 4. 在收到的数据里查找我们期待的字符串 (如 "OK" 或 "CONNECT") */
    if (strstr((const char*)rx_buf, ack) != NULL) {
        return true;
    }

    return false;
}

/// @brief  检查ESP8266模块是否响应正常
/// @param  无
/// @return true: 模块有响应且回复正常 / false: 模块无响应或回复异常
bool ESP8266_CheckAlive(void) {
    /* 测试基础命令AT，预期返回OK即可 */
    return ESP8266_SendCmd("AT\r\n", "OK", 500U);
}

/// @brief  设置WiFi工作模式
/// @param  mode: WiFi工作模式，0-AP模式，1-STA模式，2-混合模式
/// @return true: 设置成功 / false: 设置失败
bool ESP8266_SetWifiMode(uint8_t mode) {
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+CWMODE=%u\r\n", mode);
    return ESP8266_SendCmd(cmd, "OK", 500U);
}

/// @brief  连接WiFi网络
/// @param  ssid: WiFi网络名称
/// @param  pwd:  WiFi密码
/// @return true: 连接成功 / false: 连接失败
bool ESP8266_ConnectAP(const char* ssid, const char* pwd) {
    char cmd[128];

    if (ssid == NULL || pwd == NULL) {
        return false;
    }

    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, pwd);

    /* 直接等待最终的OK即可，它标志着WIFI CONNECTED和WIFI GOT IP全部走完 */
    return ESP8266_SendCmd(cmd, "OK", 10000U);
}

/// @brief          连接TCP服务器
/// @param ip       服务器IP地址字符串
/// @param port     服务器端口号
/// @return         true: 成功建立连接 / false: 连接失败
bool ESP8266_ConnectTcpServer(const char* ip, uint16_t port) {
    char cmd[128];

    if (ip == NULL) {
        return false;
    }

    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u\r\n", ip, port);
    /* 建立TCP连接最终也会返回OK。使用OK作为判定既能确保真的连上了，又能顺便清空接收缓存里的CONNECT等前置回复 */
    return ESP8266_SendCmd(cmd, "OK", 5000U) || ESP8266_SendCmd(NULL, "ALREADY CONNECTED", 1000U);
}

/**
 * @brief       发送TCP数据
 * @param data    待发送的数据缓冲区指针
 * @param len   待发送的数据长度（字节数）
 * @return  true: 数据成功发送 / false: 发送失败
 * @note    发送流程分为三步：1. 先发AT+CIPSEND=数据长度，等待模块回复 '>' 提示符；2. 收到提示符后立即发送原始数据；3. 等待模块回复 "SEND OK" 确认数据已发出
 */
bool ESP8266_SendTcpData(const uint8_t* data, uint16_t len) {
    char cmd[32];

    if (data == NULL || len == 0U) {
        return false;
    }

    /* 1. 通知ESP8266我们要发几字节的数据，等它回应 '>' 输入提示符 */
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u\r\n", len);
    if (!ESP8266_SendCmd(cmd, ">", 2000U)) {
        return false;
    }

    /* 2. 直接将负载原始数据按量推送给它 */
    if (HAL_UART_Transmit(&ESP8266_UART_HANDLE, data, len, 5000U) != HAL_OK) {
        return false;
    }

    /* 3. 阻塞等待固件将其发送出去后的 SEND OK 确认回包 (不需要发命令，传NULL只读) */
    return ESP8266_SendCmd(NULL, "SEND OK", 5000U);
}

/// @brief          关闭TCP连接
/// @param  无
/// @return         true: 关闭成功 / false: 关闭失败
bool ESP8266_CloseTcpConnection(void) {
    /* 尝试发送关闭连接命令 */
    return ESP8266_SendCmd("AT+CIPCLOSE\r\n", "CLOSED", 2000U) || ESP8266_SendCmd(NULL, "OK", 1000U);
}

/* ==================================================================== */
/*                             数据接收功能                             */
/* ==================================================================== */

/**
 * @brief  尝试无阻塞/短超时读取 TCP 下行来的数据
 * @param  rx_buf: 用来存放数据的缓冲区
 * @param  max_len: 缓冲区最大大小
 * @return 读到的数据长度。0 表示没收到数据
 * @note   当 ESP8266 收到电脑 TCP 发来的数据时，默认格式为：+IPD,<len>:<data>
 */
uint16_t ESP8266_ReceiveTcpData(char* rx_buf, uint16_t max_len) {
    uint8_t ch;
    uint32_t start_tick;
    char* data_ptr;
    char* colon_ptr;
    uint16_t expected_len = 0;
    uint16_t received_len = 0;
    // 使用静态缓冲区或者足够大的栈空间来暂存原始流，避免多次串口读取造成的碎片化
    static char temp_buf[ESP8266_RX_BUFFER_SIZE];
    static uint16_t temp_idx = 0;

    if (rx_buf == NULL || max_len == 0)
        return 0;

    // 1. 阶段一：快速捕获所有当前可用的串口数据
    // 只要串口缓冲区有数据，就尽快读完，减少因 LVGL 刷新导致的延迟丢包
    start_tick = HAL_GetTick( );
    while ((HAL_GetTick( ) - start_tick) < 50U) { // 缩短单次等待窗口，提高响应频率
        // 检查是否有数据可用 (非阻塞检查)
        // 注意：HAL_UART_Receive 带超时是阻塞的，这里我们用极短超时模拟非阻塞
        if (HAL_UART_Receive(&ESP8266_UART_HANDLE, &ch, 1U, 1U) == HAL_OK) {
            if (temp_idx < (ESP8266_RX_BUFFER_SIZE - 1)) {
                temp_buf[temp_idx++] = (char)ch;
            }
            else {
                // 缓冲区满，重置或处理错误，这里简单重置以防死锁
                temp_idx = 0;
                break;
            }
        }
        else {
            // 没有更多数据立即跳出，去处理已有数据
            break;
        }
    }

    if (temp_idx == 0) {
        rx_buf[0] = '\0';
        return 0;
    }

    temp_buf[temp_idx] = '\0';

    /* 寻找ESP8266特有的网络透传投递标志 +IPD,长度:真实内容 */
    data_ptr = strstr(temp_buf, "+IPD,");

    if (data_ptr != NULL) {
        /* 找到分号 ":" ，也就是透传数据头的终点 */
        colon_ptr = strchr(data_ptr, ':');

        if (colon_ptr != NULL) {
            // 2. 阶段二：解析数据长度
            // 格式: +IPD,len:data
            // 提取 len
            char len_str[10] = { 0 };
            int i = 0;
            char* p = data_ptr + 5; // 跳过 "+IPD,"
            while (p < colon_ptr && i < 9) {
                len_str[i++] = *p++;
            }
            // 使用 atoi 解析长度，需确保 stdlib.h 已包含
            expected_len = (uint16_t)atoi(len_str);

            // 计算当前已经接收到的有效数据部分（冒号后面的）
            uint16_t current_data_len = temp_idx - (uint16_t)(colon_ptr - temp_buf) - 1;

            // 3. 阶段三：如果数据没收完，继续等待剩余数据
            // 只有当期望长度大于当前已接收长度时才等待
            if (expected_len > current_data_len) {
                uint16_t remaining = expected_len - current_data_len;

                // 安全检查：防止恶意或错误的大包导致缓冲区溢出
                if (expected_len >= ESP8266_RX_BUFFER_SIZE) {
                    temp_idx = 0; // 丢弃异常大包
                    rx_buf[0] = '\0';
                    return 0;
                }

                start_tick = HAL_GetTick( );
                // 动态计算超时：根据剩余字节数和波特率估算，这里给一个固定宽松值
                while (remaining > 0 && (HAL_GetTick( ) - start_tick) < 200U) {
                    if (HAL_UART_Receive(&ESP8266_UART_HANDLE, &ch, 1U, 5U) == HAL_OK) {
                        if (temp_idx < (ESP8266_RX_BUFFER_SIZE - 1)) {
                            temp_buf[temp_idx++] = (char)ch;
                            remaining--;
                        }
                        else {
                            break;
                        }
                    }
                }
            }

            // 4. 阶段四：提取最终数据
            // 重新定位指针，因为 temp_buf 内容可能已更新
            data_ptr = strstr(temp_buf, "+IPD,");
            colon_ptr = strchr(data_ptr, ':');

            if (colon_ptr) {
                colon_ptr++; // 越过冒号，指向数据起始位

                // 计算实际可提取的数据长度
                // 注意：temp_buf 中可能包含后续包的开头，所以只取 expected_len
                uint16_t actual_valid_len = expected_len;

                // 确保不越界
                if (actual_valid_len > 0) {
                    if (actual_valid_len < max_len) {
                        memcpy(rx_buf, colon_ptr, actual_valid_len);
                        rx_buf[actual_valid_len] = '\0';

                        // 清理已处理的数据：将未处理的部分移到缓冲区头部
                        // 计算下一个包可能的起始位置
                        uint16_t processed_total = (uint16_t)(colon_ptr - temp_buf) + 1 + actual_valid_len;
                        if (processed_total < temp_idx) {
                            memmove(temp_buf, temp_buf + processed_total, temp_idx - processed_total);
                            temp_idx -= processed_total;
                        }
                        else {
                            temp_idx = 0;
                        }

                        return actual_valid_len;
                    }
                    else {
                        // 用户缓冲区太小，截断返回
                        memcpy(rx_buf, colon_ptr, max_len - 1);
                        rx_buf[max_len - 1] = '\0';

                        // 同样清理缓冲区
                        uint16_t processed_total = (uint16_t)(colon_ptr - temp_buf) + 1 + (max_len - 1);
                        if (processed_total < temp_idx) {
                            memmove(temp_buf, temp_buf + processed_total, temp_idx - processed_total);
                            temp_idx -= processed_total;
                        }
                        else {
                            temp_idx = 0;
                        }

                        return max_len - 1;
                    }
                }
            }
        }
    }

    /* 如果收到数据但不是 +IPD 格式(比如是OK或别的系统回声) 就干脆扔掉 */
    // 优化：如果不是 +IPD，可能是之前的残留或命令回显，直接清空缓冲区以免堆积垃圾数据
    temp_idx = 0;
    rx_buf[0] = '\0';
    return 0;
}

/*
功能测试函数
连接wifi，连接服务器，发送数据，接收数据
*/

void ESP8266_Test(void) {
}
