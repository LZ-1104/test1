// #include "MAX30102.h"
// #include "ILI9341.h"
// #include <string.h>
#include "IIC.h"

void LED_Init(void)
{
    GPIO_InitTypeDef GPIO_Initure;
    __HAL_RCC_GPIOB_CLK_ENABLE();              // 使能GPIOB时钟
    GPIO_Initure.Pin = GPIO_PIN_0;             // PB0
    GPIO_Initure.Mode = GPIO_MODE_OUTPUT_PP;   // 推挽输出
    GPIO_Initure.Pull = GPIO_NOPULL;           // 不带上下拉
    GPIO_Initure.Speed = GPIO_SPEED_FREQ_HIGH; // 高速
    HAL_GPIO_Init(GPIOB, &GPIO_Initure);       // 初始化PB0

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET); // 初始状态为高，关闭LED
}

void LED_Control(uint8_t state)
{
    if (state)
    {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET); // 关闭LED
    }
    else
    {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET); // 点亮LED
    }
}
// static void MAX30102_WriteReg(uint8_t RegAddress, uint8_t Data)
// {
//     IIC_Start();                   // I2C起始
//     IIC_SendByte(MAX30102_I2C_ADDRESS); // 发送从机地址，读写位为0，表示即将写入
//     IIC_ReceiveAck();              // 接收应答

//     IIC_SendByte(RegAddress);      // 发送寄存器地址
//     IIC_ReceiveAck();              // 接收应答
//     IIC_SendByte(Data);            // 发送要写入的数据
//     IIC_ReceiveAck();              // 接收应答
//     IIC_Stop();                    // I2C终止
// }

#include "max30102.h"
#include <math.h>
#include "ILI9341.h"
MAX30102_DataTypedef max30102_data;

void MAX30102_IIC_Init(void)
{
    // 配置PB6为SCL，PB7为SDA
    GPIO_InitTypeDef GPIO_Initure;
    __HAL_RCC_GPIOB_CLK_ENABLE();               // 使能GPIOB时钟
    GPIO_Initure.Pin = GPIO_PIN_6 | GPIO_PIN_7; // PB6 PB7
    GPIO_Initure.Mode = GPIO_MODE_OUTPUT_OD;    // 开漏输出
    GPIO_Initure.Pull = GPIO_PULLUP;            // 上拉
    GPIO_Initure.Speed = GPIO_SPEED_FREQ_HIGH;  // 高速
    HAL_GPIO_Init(GPIOB, &GPIO_Initure);        // 初始化PB6 PB7

    IIC_W_SCL(1); // 释放SCL
    IIC_W_SDA(1); // 释放SDA
}

static void MAX30102_WriteRegs(uint8_t RegAddress, uint8_t Data, uint8_t Length)
{
    IIC_Start();                           // I2C起始
    IIC_SendByte(MAX30102_I2C_ADDR_WRITE); // 发送从机地址，读写位为0，表示即将写入
    IIC_ReceiveAck();                      // 接收应答

    IIC_SendByte(RegAddress); // 发送寄存器地址
    IIC_ReceiveAck();         // 接收应答

    for (uint8_t i = 0; i < Length; i++)
    {
        IIC_SendByte(Data); // 发送要写入的数据
        IIC_ReceiveAck();   // 接收应答
    }

    IIC_Stop(); // I2C终止
}

static void MAX30102_ReadRegs(uint8_t RegAddress, uint8_t *pData, uint8_t Length)
{
    IIC_Start();                           // I2C起始
    IIC_SendByte(MAX30102_I2C_ADDR_WRITE); // 发送从机地址，读写位为0，表示即将写入
    IIC_ReceiveAck();                      // 接收应答
    IIC_SendByte(RegAddress);              // 发送寄存器地址
    IIC_ReceiveAck();                      // 接收应答

    IIC_Start();                          // 重新发送起始信号
    IIC_SendByte(MAX30102_I2C_ADDR_READ); // 发送从机地址，读写位为1，表示即将读取
    IIC_ReceiveAck();                     // 接收应答

    for (uint8_t i = 0; i < Length; i++)
    {
        pData[i] = IIC_ReceiveByte(); // 接收数据
        if (i < Length - 1)
        {
            IIC_SendAck(0); // 发送应答，表示还要继续接收数据
        }
        else
        {
            IIC_SendAck(1); // 最后一个字节，发送非应答
        }
    }

    IIC_Stop(); // I2C终止
}

// I2C写入1字节
static void I2C_WriteByte(uint8_t reg_addr, uint8_t data)
{
    MAX30102_WriteRegs(reg_addr, data, 1);
}

// I2C读取1字节
static uint8_t I2C_ReadByte(uint8_t reg_addr)
{
    uint8_t data;
    MAX30102_ReadRegs(reg_addr, &data, 1);
    return data;
}

// I2C读取多字节
static void I2C_ReadBytes(uint8_t reg_addr, uint8_t len, uint8_t *buf)
{
    MAX30102_ReadRegs(reg_addr, buf, len);
}

// 初始化MAX30102
uint8_t MAX30102_Init(uint8_t mode, uint8_t sr, uint8_t pw, uint8_t adc_range)
{
    uint8_t id;
    MAX30102_IIC_Init(); // 初始化I2C接口
    // 读取器件ID验证通信
    id = MAX30102_ReadID();
    if (id != 0x15)
        return 1; // ID错误返回1

    // 软复位
    MAX30102_Reset();
    HAL_Delay(200);

    // 等待复位完成
    while (I2C_ReadByte(MAX30102_REG_MODE_CONFIG) & 0x40)
        ;
    // 配置FIFO：关闭平均采样，开启溢出循环，FIFO满中断阈值为16
    I2C_WriteByte(MAX30102_REG_FIFO_CONFIG, 0x00 | 0x10 | 0x0F);
    // 清空FIFO写指针、读指针、溢出计数器
    I2C_WriteByte(MAX30102_REG_FIFO_WR_PTR, 0x00);
    I2C_WriteByte(MAX30102_REG_FIFO_RD_PTR, 0x00);
    I2C_WriteByte(MAX30102_REG_OVF_COUNTER, 0x00);

    // 配置血氧参数：ADC量程+采样率+脉冲宽度
    uint8_t spo2_config = (adc_range << 5) | (sr << 2) | pw;
    I2C_WriteByte(MAX30102_REG_SPO2_CONFIG, spo2_config);

    // 配置工作模式
    I2C_WriteByte(MAX30102_REG_MODE_CONFIG, mode);

    // 设置LED电流（默认红光27.1mA，红外光27.1mA）
    MAX30102_SetLEDCurrent(0x1F, 0x1F);

    // 使能数据就绪中断
    I2C_WriteByte(MAX30102_REG_INT_ENABLE2, 0x02);

    return 0; // 初始化成功返回0
}

// 读取器件ID
uint8_t MAX30102_ReadID(void)
{
    return I2C_ReadByte(MAX30102_REG_PART_ID);
}

// 设置LED电流（0~51mA，手册Table8）
void MAX30102_SetLEDCurrent(uint8_t red_current, uint8_t ir_current)
{
    I2C_WriteByte(MAX30102_REG_LED1_PA, red_current); // 红光
    I2C_WriteByte(MAX30102_REG_LED2_PA, ir_current);  // 红外光
    // I2C_WriteByte(MAX30102_REG_MULTI_LED1, 0x02<<4|0x01);   // LED1=红光，LED2=红外光
    // I2C_WriteByte(MAX30102_REG_MULTI_LED2, 0x02<<4|0x01);          // LED3关闭
}

// 读取FIFO数据（严格按手册流程）
uint8_t MAX30102_ReadFIFO_Manual(MAX30102_DataTypedef *data, uint8_t *sample_cnt)
{
    uint8_t wr_ptr, rd_ptr;
    uint8_t num_available, num_to_read;
    uint8_t fifo_raw[6 * 32]; // FIFO最大32个样本，每个样本6字节（红光3+红外3）

    // ---------------- 第一步：读取FIFO_WR_PTR和FIFO_RD_PTR ----------------
    // 读FIFO_WR_PTR（0x04）
    wr_ptr = I2C_ReadByte(MAX30102_REG_FIFO_WR_PTR);
    // 读FIFO_RD_PTR（0x05）
    rd_ptr = I2C_ReadByte(MAX30102_REG_FIFO_RD_PTR);

    // 计算可用样本数（处理指针回绕）
    num_available = (wr_ptr - rd_ptr) & 0x1F; // FIFO深度32，用&0x1F处理回绕
    if (num_available == 0)
    {
        *sample_cnt = 0;
        return 0; // 无可用数据
    }

    // 选择要读取的样本数（建议一次读满，或不超过可用数）
    num_to_read = num_available; // 读取所有可用样本
    *sample_cnt = num_to_read;

    // ---------------- 第二步：读取FIFO_DATA寄存器的num_to_read个样本 ----------------
    // 发送FIFO_DATA地址（0x07），准备连续读取
    I2C_ReadBytes(MAX30102_REG_FIFO_DATA, num_to_read * 6, fifo_raw); // 每个样本6字节

    // ---------------- 第三步：解析数据（红光+红外） ----------------
    for (uint8_t i = 0; i < num_to_read; i++)
    {
        // 每个样本占6字节：红光3字节（index 0-2）、红外3字节（index 3-5）
        uint32_t red = ((uint32_t)fifo_raw[i * 6 + 0] << 16) |
                       ((uint32_t)fifo_raw[i * 6 + 1] << 8) |
                       (uint32_t)fifo_raw[i * 6 + 2];
        uint32_t ir = ((uint32_t)fifo_raw[i * 6 + 3] << 16) |
                      ((uint32_t)fifo_raw[i * 6 + 4] << 8) |
                      (uint32_t)fifo_raw[i * 6 + 5];

        // 18位分辨率过滤
        data[i].red_data = red & 0x03FFFF;
        data[i].ir_data = ir & 0x03FFFF;
    }

    // ---------------- 第四步：更新FIFO_RD_PTR ----------------
    // 将读指针更新为当前写指针（完成读取）
    I2C_WriteByte(MAX30102_REG_FIFO_RD_PTR, wr_ptr);

    return 1; // 读取成功
}

// 读取FIFO数据
uint8_t MAX30102_ReadFIFO(MAX30102_DataTypedef *data)
{
    uint8_t fifo_buf[6];
    uint8_t int_status;

    // 读取中断状态
    // int_status = I2C_ReadByte(MAX30102_REG_INT_STATUS1);
    // if (!(int_status & 0x02))
    //     return 1; // 无新数据返回1

    // uint8_t wr_ptr = I2C_ReadByte(MAX30102_REG_FIFO_WR_PTR); // 读取当前写指针
    // uint8_t rd_ptr = I2C_ReadByte(MAX30102_REG_FIFO_RD_PTR); // 读取当前读指针

    // // 计算待读取的数据量（FIFO深度32，需处理溢出）
    // uint8_t data_len = (wr_ptr - rd_ptr) & 0x1F;
    // if (data_len > 0)
    // {
    //     I2C_ReadBytes(MAX30102_REG_FIFO_DATA, 6, fifo_buf); // 读取6字节数据
    //     I2C_WriteByte(MAX30102_REG_FIFO_RD_PTR, wr_ptr);    // 更新读指针至写指针位置
    // }

    // 读取6字节FIFO数据（红光3字节+红外光3字节）
    // 正确读取一个样本：读FIFO_DATA自动硬件推进读指针，无需手动写FIFO_RD_PTR
    I2C_ReadBytes(MAX30102_REG_FIFO_DATA, 6, fifo_buf);
    // 解析18位数据（手册FIFO Data Structure）
    data->red_data = ((uint32_t)fifo_buf[0] << 16) | ((uint32_t)fifo_buf[1] << 8) | fifo_buf[2];
    data->red_data &= 0x03FFFF; // 18位有效
    data->ir_data = ((uint32_t)fifo_buf[3] << 16) | ((uint32_t)fifo_buf[4] << 8) | fifo_buf[5];
    data->ir_data &= 0x03FFFF;

    return 0; // 读取成功返回0
}

// 读取温度
float MAX30102_ReadTemperature(void)
{
    int8_t temp_int;
    uint8_t temp_frac;
    uint8_t status2;
    uint16_t waited = 0;
    LED_Control(1);

    // 触发一次温度转换 (TEMP_EN=1)
    I2C_WriteByte(MAX30102_REG_TEMP_CONFIG, 0x01);

    // 轮询 INT_STATUS2 bit0 (TEMP_RDY) 最长 ~200ms
    while (waited < 200)
    {
        status2 = I2C_ReadByte(MAX30102_REG_INT_STATUS2);
        if (status2 & 0x02)
            break; // 温度就绪
        HAL_Delay(5);
        waited += 5;
    }

    if (!(status2 & 0x02))
    {
        LED_Control(0);
        // 超时返回一个标记值(可改为上次缓存，这里简单返回0)
        return max30102_data.temperature;
    }
    // 读取整数与小数部分 (低4位为 0.0625°C/LSB)
    temp_int = (int8_t)I2C_ReadByte(MAX30102_REG_DIE_TEMP_INT);
    temp_frac = I2C_ReadByte(MAX30102_REG_DIE_TEMP_FR) & 0x0F;

    return (float)temp_int + (temp_frac * 0.0625f);
}

// 软复位
void MAX30102_Reset(void)
{
    I2C_WriteByte(MAX30102_REG_MODE_CONFIG, 0x40); // 置位RESET位
    while (I2C_ReadByte(MAX30102_REG_MODE_CONFIG) & 0x40)
        ; // 等待复位完成
}

// 进入低功耗模式
void MAX30102_Shutdown(void)
{
    I2C_WriteByte(MAX30102_REG_MODE_CONFIG, I2C_ReadByte(MAX30102_REG_MODE_CONFIG) | 0x80);
}

// 唤醒
void MAX30102_Wakeup(void)
{
    I2C_WriteByte(MAX30102_REG_MODE_CONFIG, I2C_ReadByte(MAX30102_REG_MODE_CONFIG) & 0x7F);
}

// 一阶低通滤波
static float LowPassFilter(float input, float last_output)
{
    const float alpha = 0.7f; // 滤波系数
    return alpha * input + (1 - alpha) * last_output;
}

// 峰值检测
static uint8_t PeakDetection(float *data, uint16_t len, uint8_t *peak_idx)
{
    uint8_t peak_count = 0;
    const float threshold = 0.3f; // 峰值阈值

    // 遍历检测峰值（中间点大于左右两点）
    for (uint16_t i = 1; i < len - 1; i++)
    {
        if (data[i] > data[i - 1] && data[i] > data[i + 1] && data[i] > (data[0] * threshold))
        {
            peak_idx[peak_count++] = i;
            if (peak_count >= 10)
                break; // 最多检测10个峰值
        }
    }
    return peak_count;
}

// 计算心率和血氧饱和度
uint8_t MAX30102_CalculateHRAndSpO2(MAX30102_DataTypedef *data, uint32_t *red_buf, uint32_t *ir_buf, uint16_t buf_len)
{
    float red_filtered[buf_len];
    float ir_filtered[buf_len];
    uint8_t peak_idx[10];
    uint8_t peak_count;
    float ac_red, ac_ir, dc_red, dc_ir;

    // 数据滤波
    red_filtered[0] = red_buf[0];
    ir_filtered[0] = ir_buf[0];
    for (uint16_t i = 1; i < buf_len; i++)
    {
        red_filtered[i] = LowPassFilter(red_buf[i], red_filtered[i - 1]);
        ir_filtered[i] = LowPassFilter(ir_buf[i], ir_filtered[i - 1]);
    }

    // 计算直流分量（平均值）和交流分量（峰值）
    dc_red = dc_ir = 0;
    for (uint16_t i = 0; i < buf_len; i++)
    {
        dc_red += red_filtered[i];
        dc_ir += ir_filtered[i];
    }
    dc_red /= buf_len;
    dc_ir /= buf_len;

    ac_red = ac_ir = 0;
    for (uint16_t i = 0; i < buf_len; i++)
    {
        ac_red = fmaxf(ac_red, fabs(red_filtered[i] - dc_red));
        ac_ir = fmaxf(ac_ir, fabs(ir_filtered[i] - dc_ir));
    }

    // 检测红外光峰值（抗干扰更强）
    peak_count = PeakDetection(ir_filtered, buf_len, peak_idx);
    if (peak_count < 2)
        return 1; // 峰值不足返回1

    // 计算心率（采样率100Hz为例）
    uint16_t avg_interval = 0;
    for (uint8_t i = 1; i < peak_count; i++)
    {
        avg_interval += peak_idx[i] - peak_idx[i - 1];
    }
    avg_interval /= (peak_count - 1);
    data->heart_rate = (60 * 200) / avg_interval; // 需根据实际采样率调整，60是据实际采样率调整，200是据实际采样率调整

    // 计算血氧饱和度（经验公式）
    if (ac_ir == 0)
        return 1;
    float ratio = ac_red / ac_ir;
    data->spo2 = 100 - 18 * ratio;

    // 数据有效性校验（合理范围：心率50~180，血氧90~100）
    if (data->heart_rate < 50 || data->heart_rate > 180 || data->spo2 < 60 || data->spo2 > 100)
    {
        return 1;
    }

    return 0; // 计算成功返回0
}

void MAX30102_Test(void)
{
    const uint16_t buf_size = 200; // 25个采样点（1秒@50Hz）
    uint32_t red_buf[buf_size];    // 50个采样点（1秒@50Hz）
    uint32_t ir_buf[buf_size];
    uint16_t buf_idx = 0;
    // char display_str[64];
    ILI9341_Init();
    LED_Init();
    ILI9341_Printf(10, 10, Font16x24, "MAX30102 TEST");
    if (MAX30102_Init(MAX30102_MODE_SPO2, MAX30102_SR_200HZ, MAX30102_PW_411US, MAX30102_ADC_RANGE_16384NA) != 0)
    {
        ILI9341_Printf(10, 40, Font16x24, "Init Failed");
    }

    while (1)
    {
        // 读取FIFO数据
        uint8_t sample_count;
        if (MAX30102_ReadFIFO(&max30102_data) == 0)
        {
            // 存储采样数据
            red_buf[buf_idx] = max30102_data.red_data;
            ir_buf[buf_idx] = max30102_data.ir_data;
            buf_idx++;

            // 每25个采样点（1秒）计算一次心率和血氧
            if (buf_idx >= buf_size)
            {
                if (MAX30102_CalculateHRAndSpO2(&max30102_data, red_buf, ir_buf, buf_size) == 0)
                {
                    // 将字符串
                    ILI9341_Printf(10, 70, Font16x24, "HRate:%dBPM", max30102_data.heart_rate);
                    ILI9341_Printf(10, 100, Font16x24, "SpO2:%d%%", max30102_data.spo2);
                }
                buf_idx = 0;
            }
        }

        // 读取温度（每5秒一次）
        static uint32_t temp_cnt = 0; // ms 计数
        if (temp_cnt >= 2500)
        {
            float t = MAX30102_ReadTemperature();
            max30102_data.temperature = t;
            ILI9341_Printf(10, 130, Font16x24, "Temp:%.2fC", max30102_data.temperature);
            ILI9341_Printf(10, 160, Font16x24, "test:%.2f", 0.45f);
            temp_cnt = 0;
            LED_Control(0);
        }
        temp_cnt += 1;
        HAL_Delay(1);
    }
}