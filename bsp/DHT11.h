#ifndef __DHT11_H__
#define __DHT11_H__

#include "main.h"

#define DHT11_DATA_PORT        GPIOE
#define DHT11_DATA_PIN         GPIO_PIN_6
#define DHT11_GPIO_CLK_ENABLE() __HAL_RCC_GPIOE_CLK_ENABLE()

void DHT11_Init(void);
uint8_t DHT11_ReadByte(void);
uint8_t DHT11_Start(void);
uint8_t DHT11_ReadData(float *temperature, float *humidity);
void DHT11_Reset(void);

void DHT11_Test(void);


#endif /* __DHT11_H__ */