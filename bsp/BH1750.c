#include "BH1750.h"
#include "ILI9341.h"
#include "IIC.h"
void BH1750_IIC_Init(void)
{
    // 配置PB6为SCL，PB7为SDA
    GPIO_InitTypeDef GPIO_Initure;
    __HAL_RCC_GPIOB_CLK_ENABLE();               // 使能GPIOB时钟
    GPIO_Initure.Pin = GPIO_PIN_6 | GPIO_PIN_7; // PB6 PB7
    GPIO_Initure.Mode = GPIO_MODE_OUTPUT_OD;    // 推挽输出
    GPIO_Initure.Pull = GPIO_NOPULL;            // 不带上下拉
    GPIO_Initure.Speed = GPIO_SPEED_FREQ_HIGH;  // 高速
    HAL_GPIO_Init(GPIOB, &GPIO_Initure);        // 初始化PB6 PB7

    IIC_W_SCL(1); // 释放SCL
    IIC_W_SDA(1); // 释放SDA
}




void BH1750_WriteCmd(uint8_t RegAddress)
{
    IIC_Start();                   // I2C起始
    IIC_SendByte(BH1750_I2C_ADDR); // 发送从机地址，读写位为0，表示即将写入
    IIC_ReceiveAck();              // 接收应答
    IIC_SendByte(RegAddress);      // 发送寄存器地址
    IIC_ReceiveAck();              // 接收应答
    IIC_Stop();                    // I2C终止
}
uint8_t BH1750_ReadData()
{
    uint8_t Data;

    IIC_Start();                   // I2C起始
    IIC_SendByte(BH1750_I2C_ADDR | 0x01); // 发送从机地址，读写位为1，表示即将读取
    IIC_ReceiveAck();                     // 接收应答
    Data = IIC_ReceiveByte();             // 接收指定寄存器的数据
    IIC_SendAck(1);                       // 发送应答，给从机非应答，终止从机的数据输出
    IIC_Stop();                           // I2C终止

    return Data;
}

void BH1750_Init(void)
{
    BH1750_IIC_Init();                     // 初始化I2C接口
    HAL_Delay(10);                         // 等待传感器上电稳定
    BH1750_WriteCmd(BH1750_POWER_ON);   // 上电传感器
    HAL_Delay(10);                         // 等待传感器上电完成
    BH1750_WriteCmd(BH1750_RESET);      // 复位传感器
    HAL_Delay(10);                         // 等待传感器复位完成
    BH1750_WriteCmd(BH1750_CONTINUOUS_H_RES_MODE2); // 设置为连续高分辨率模式
}

void BH1750_GetData(uint16_t *value)
{
    uint8_t highByte=0, lowByte=0;
    IIC_Start();                          // I2C起始
    IIC_SendByte(BH1750_I2C_ADDR | 0x01); // 发送从机地址，读写位为1，表示即将读取
    IIC_ReceiveAck();                     // 接收应答

    highByte = IIC_ReceiveByte(); // 接收高字节
    IIC_SendAck(0);               // 发送应答，表示还要继续接收数据

    lowByte = IIC_ReceiveByte(); // 接收低字节
    IIC_SendAck(1);              // 发送非应答，表示数据接收完毕

    IIC_Stop(); // I2C终止

    *value = (highByte << 8) | lowByte; // 合并高低字节为16位数据
}

void BH1750_Test(void)
{
    uint16_t lightIntensity;

    BH1750_Init(); // 初始化BH1750传感器
    ILI9341_Init(); // 初始化ILI9341显示屏
    ILI9341_ShowString(10,10,"BH1750 Test",Font16x24);
    while (1)
    {
        BH1750_GetData(&lightIntensity); // 获取光照强度数据
        float lux = lightIntensity / 1.2f; // 转换为勒克斯值
        ILI9341_ShowFloatNum(10, 50, lux, 3, 2, Font16x24);
        HAL_Delay(200); // 延时1秒
    }
}