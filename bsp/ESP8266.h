/**
  ******************************************************************************
  * @file    ESP8266.h
  * @author  Copilot
  * @brief   ESP8266 WiFi模块驱动头文件。
  *          提供底层的硬件使能与复位控制，以及高层用于WiFi连接和TCP传输的AT指令接口。
  ******************************************************************************
  */
#ifndef __ESP8266_H
#define __ESP8266_H

#include "usart.h"
#include <stdbool.h>

/* ==================================================================== */
/*                              硬件引脚定义                            */
/* ==================================================================== */
#define ESP8266_EN_GPIO_PORT    GPIOB
#define ESP8266_EN_GPIO_PIN     GPIO_PIN_8

#define ESP8266_RST_GPIO_PORT   GPIOB
#define ESP8266_RST_GPIO_PIN    GPIO_PIN_9

/* 用于通信的串口句柄，ESP8266对接在USART3上 */
#define ESP8266_UART_HANDLE     huart3

void ESP8266_Init(void);
void ESP8266_PowerOn(void);
void ESP8266_PowerOff(void);
void ESP8266_HardReset(void);

bool ESP8266_SendCmd(const char *cmd, const char *ack, uint32_t timeout);
bool ESP8266_CheckAlive(void);

bool ESP8266_SetWifiMode(uint8_t mode);

bool ESP8266_ConnectAP(const char *ssid, const char *pwd);
bool ESP8266_ConnectTcpServer(const char *ip, uint16_t port);
bool ESP8266_SendTcpData(const uint8_t *data, uint16_t len);

bool ESP8266_CloseTcpConnection(void);

uint16_t ESP8266_ReceiveTcpData(char *rx_buf, uint16_t max_len);

bool ESP8266_Recover(void);

// void ESPTCP_Printf(const char* format, ...);

#endif /* __ESP8266_H */
