#ifndef __BH1750_H
#define __BH1750_H

#include "stm32f1xx_hal.h"

/*
 *   @brief BH1750 I2C地址
 *   如果 ADDR 引脚接地 (L)，地址为 0x23
 *   如果 ADDR 引脚接 VCC (H)，地址为 0x5C
 */
#define BH1750_I2C_ADDR_L       (0x23 << 1)
#define BH1750_I2C_ADDR_H       (0x5C << 1)

// 根据您的硬件连接选择使用的地址
#define BH1750_I2C_ADDR         BH1750_I2C_ADDR_L

/*
 *   @brief BH1750 指令集
 */
#define BH1750_POWER_DOWN       0x00 // 掉电模式
#define BH1750_POWER_ON         0x01 // 上电模式
#define BH1750_RESET            0x07 // 复位

// 连续测量模式
#define BH1750_CONTINUOUS_H_RES_MODE  0x10 // 连续高分辨率模式 (1 lx)
#define BH1750_CONTINUOUS_H_RES_MODE2 0x11 // 连续高分辨率模式2 (0.5 lx)
#define BH1750_CONTINUOUS_L_RES_MODE  0x13 // 连续低分辨率模式 (4 lx)

// 单次测量模式
#define BH1750_ONE_TIME_H_RES_MODE  0x20 // 单次高分辨率模式 (1 lx)
#define BH1750_ONE_TIME_H_RES_MODE2 0x21 // 单次高分辨率模式2 (0.5 lx)
#define BH1750_ONE_TIME_L_RES_MODE  0x23 // 单次低分辨率模式 (4 lx)


// 函数原型
void BH1750_Init(void);
void BH1750_Reset(void);
uint8_t BH1750_Read_Value(uint16_t *value);
float BH1750_Get_Lux(void);
void BH1750_Test(void);

#endif /* __BH1750_H */
