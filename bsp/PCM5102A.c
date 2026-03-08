#include "PCM5102A.h"
#include  "stm32f1xx_hal.h"

void PCM5102A_Init(void)
{
    // 初始化PCM5102A相关的I2S接口和GPIO
    MX_I2S2_Init();
    // 其他初始化代码（如复位引脚配置等）可以在这里添加
}

