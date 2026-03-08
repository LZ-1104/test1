#ifndef __MAX30102_H
#define __MAX30102_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

// I2C地址定义（手册Table17）
#define MAX30102_I2C_ADDR_WRITE 0xAE
#define MAX30102_I2C_ADDR_READ 0xAF

// 寄存器地址定义（手册Register Maps）
#define MAX30102_REG_INT_STATUS1 0x00  // 中断状态1
#define MAX30102_REG_INT_STATUS2 0x01  // 中断状态2
#define MAX30102_REG_INT_ENABLE1 0x02  // 中断使能1
#define MAX30102_REG_INT_ENABLE2 0x03  // 中断使能2

#define MAX30102_REG_FIFO_WR_PTR 0x04  // FIFO写指针
#define MAX30102_REG_OVF_COUNTER 0x05  // 溢出计数器

#define MAX30102_REG_FIFO_RD_PTR 0x06  // FIFO读指针
#define MAX30102_REG_FIFO_DATA 0x07    // FIFO数据寄存器
#define MAX30102_REG_FIFO_CONFIG 0x08  // FIFO配置

#define MAX30102_REG_MODE_CONFIG 0x09  // 模式配置
#define MAX30102_REG_SPO2_CONFIG 0x0A  // 血氧配置
#define MAX30102_REG_LED1_PA 0x0C      // 红光LED电流
#define MAX30102_REG_LED2_PA 0x0D      // 红外光LED电流
#define MAX30102_REG_MULTI_LED1 0x11   // 多LED模式控制1
#define MAX30102_REG_MULTI_LED2 0x12   // 多LED模式控制2
#define MAX30102_REG_DIE_TEMP_INT 0x1F // 温度整数部分
#define MAX30102_REG_DIE_TEMP_FR 0x20  // 温度小数部分
#define MAX30102_REG_TEMP_CONFIG 0x21  // 温度配置
#define MAX30102_REG_PART_ID 0xFF      // 器件ID（默认0x15）

// 模式配置宏定义（手册Table4）
#define MAX30102_MODE_HR 0x02        // 心率模式（仅红光）
#define MAX30102_MODE_SPO2 0x03      // 血氧模式（红光+红外）
#define MAX30102_MODE_MULTI_LED 0x07 // 多LED模式

// 采样率宏定义（手册Table6）
#define MAX30102_SR_50HZ 0x00
#define MAX30102_SR_100HZ 0x01
#define MAX30102_SR_200HZ 0x02
#define MAX30102_SR_400HZ 0x03
#define MAX30102_SR_800HZ 0x04
#define MAX30102_SR_1000HZ 0x05
#define MAX30102_SR_1600HZ 0x06
#define MAX30102_SR_3200HZ 0x07

// LED脉冲宽度宏定义（手册Table7）
#define MAX30102_PW_69US 0x00  // 15位分辨率
#define MAX30102_PW_118US 0x01 // 16位分辨率
#define MAX30102_PW_215US 0x02 // 17位分辨率
#define MAX30102_PW_411US 0x03 // 18位分辨率

// ADC量程宏定义（手册Table5）
#define MAX30102_ADC_RANGE_2048NA 0x00
#define MAX30102_ADC_RANGE_4096NA 0x01
#define MAX30102_ADC_RANGE_8192NA 0x02
#define MAX30102_ADC_RANGE_16384NA 0x03

// 数据结构体
typedef struct
{
    uint32_t red_data;  // 红光数据（18位）
    uint32_t ir_data;   // 红外光数据（18位）
    float temperature;  // 温度（°C）
    uint8_t heart_rate; // 心率（BPM）
    uint8_t spo2;       // 血氧饱和度（%）
} MAX30102_DataTypedef;

// // 单样本数据
// typedef struct {
//     uint32_t red_data;    // 红光数据（18位）
//     uint32_t ir_data;     // 红外光数据（18位）
// } MAX30102_SampleTypedef;

// // 批量数据容器
// typedef struct {
//     MAX30102_SampleTypedef samples[32];  // FIFO最大32个样本
//     uint8_t sample_count;                // 实际读取的样本数
//     float temperature;                   // 温度（°C）
//     uint8_t heart_rate;                  // 心率（BPM）
//     uint8_t spo2;                        // 血氧饱和度（%）
// } MAX30102_BatchDataTypedef;


// 函数声明
extern I2C_HandleTypeDef hi2c1; // 需与CubeMX配置一致

uint8_t MAX30102_Init(uint8_t mode, uint8_t sr, uint8_t pw, uint8_t adc_range);
uint8_t MAX30102_ReadID(void);
void MAX30102_SetLEDCurrent(uint8_t red_current, uint8_t ir_current);
uint8_t MAX30102_ReadFIFO(MAX30102_DataTypedef *data);
float MAX30102_ReadTemperature(void);
void MAX30102_Reset(void);
void MAX30102_Shutdown(void);
void MAX30102_Wakeup(void);
uint8_t MAX30102_CalculateHRAndSpO2(MAX30102_DataTypedef *data, uint32_t *red_buf, uint32_t *ir_buf, uint16_t buf_len);
void MAX30102_Test(void);
#endif
