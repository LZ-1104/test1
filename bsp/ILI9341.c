#include "ILI9341.h"
#include "dma.h"
#include "fsmc.h"
#include "fonts.h"
#include "math.h"
#include "string.h"
#include <stdarg.h>
static SRAM_HandleTypeDef SRAM_Handler;
static FSMC_NORSRAM_TimingTypeDef Timing;

// 根据液晶扫描方向而变化的XY像素宽度
// 调用ILI9341_GramScan函数设置方向时会自动更改
uint16_t LCD_X_LENGTH = ILI9341_LESS_PIXEL;
uint16_t LCD_Y_LENGTH = ILI9341_MORE_PIXEL;

// 液晶屏扫描模式，本变量主要用于方便选择触摸屏的计算参数
// 参数可选值为0-7
// 调用ILI9341_GramScan函数设置方向时会自动更改
// LCD刚初始化完成时会使用本默认值
uint8_t LCD_SCAN_MODE = 3;

static uint16_t CurrentTextColor = BLACK; // 前景色
static uint16_t CurrentBackColor = WHITE; // 背景色

__inline void ILI9341_Write_Cmd(uint16_t usCmd);
__inline void ILI9341_Write_Data(uint16_t usData);
__inline uint16_t ILI9341_Read_Data(void);

static void ILI9341_Delay(__IO uint32_t nCount);
static void ILI9341_GPIO_Config(void);
static void ILI9341_FSMC_Config(void);
static void ILI9341_REG_Config(void);

static void ILI9341_SetCursor(uint16_t usX, uint16_t usY);
static __inline void ILI9341_FillColor(uint32_t ulAmout_Point, uint16_t usColor);
static uint16_t ILI9341_Read_PixelData(void);
void ILI9341_WritePixels(const uint16_t *data, uint32_t length);

/**
 * @brief  向ILI9341写入命令
 * @param  usCmd :要写入的命令（表寄存器地址）
 * @retval 无
 */
void ILI9341_Write_Cmd(uint16_t usCmd)
{
    *(__IO uint16_t *)(FSMC_Addr_ILI9341_CMD) = usCmd;
}

/**
 * @brief  向ILI9341写入数据
 * @param  usData :要写入的数据
 * @retval 无
 */
void ILI9341_Write_Data(uint16_t usData)
{
    *(__IO uint16_t *)(FSMC_Addr_ILI9341_DATA) = usData;
}

/**
 * @brief  从ILI9341读取数据
 * @param  无
 * @retval 读取到的数据
 */
uint16_t ILI9341_Read_Data(void)
{
    return (*(__IO uint16_t *)(FSMC_Addr_ILI9341_DATA));
}

/**
 * @brief  用于 ILI9341 简单延时函数
 * @param  nCount ：延时计数值
 * @retval 无
 */
static void ILI9341_Delay(__IO uint32_t nCount)
{
    for (; nCount != 0; nCount--)
        ;
}

/**
 * @brief  初始化ILI9341的IO引脚
 * @param  无
 * @retval 无
 */
static void ILI9341_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_Initure;

    /* Enable GPIOs clock */
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_FSMC_CLK_ENABLE(); // 使能FSMC时钟

    /* Common GPIO configuration */
    GPIO_Initure.Mode = GPIO_MODE_OUTPUT_PP; // 推挽输出
    GPIO_Initure.Pull = GPIO_PULLUP;
    GPIO_Initure.Speed = GPIO_SPEED_FREQ_HIGH;

    GPIO_Initure.Pin = GPIO_PIN_12;
    HAL_GPIO_Init(GPIOD, &GPIO_Initure);

    // 初始化复位引脚E1
    GPIO_Initure.Pin = GPIO_PIN_1;
    HAL_GPIO_Init(GPIOE, &GPIO_Initure);

    GPIO_Initure.Mode = GPIO_MODE_AF_PP;
    //  GPIO_Initure.Alternate=GPIO_AF12_FSMC;	//复用为FSMC

    // 初始化PD0,1,4,5,8,9,10,14,15
    GPIO_Initure.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_7 | GPIO_PIN_8 |
                       GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOD, &GPIO_Initure);

    // 初始化PE2,7,8,9,10,11,12,13,14,15
    GPIO_Initure.Pin = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 |
                       GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOE, &GPIO_Initure);
}

/**
 * @brief  LCD  FSMC 模式配置
 * @param  无
 * @retval 无
 */
static void ILI9341_FSMC_Config(void)
{
    // MX_FSMC_Init();
    SRAM_Handler.Instance = FSMC_NORSRAM_DEVICE;
    SRAM_Handler.Extended = FSMC_NORSRAM_EXTENDED_DEVICE;

    /* SRAM device configuration */
    Timing.AddressSetupTime = 0x00; /// 地址建立时间
    Timing.AddressHoldTime = 0x00;
    Timing.DataSetupTime = 0x04;    // 提高FSMC读写速度
    Timing.BusTurnAroundDuration = 0x00;
    Timing.CLKDivision = 0x00;
    Timing.DataLatency = 0x00;
    Timing.AccessMode = FSMC_ACCESS_MODE_B;

    SRAM_Handler.Init.NSBank = FSMC_Bank1_NORSRAMx;                       // 使用NE4
    SRAM_Handler.Init.DataAddressMux = FSMC_DATA_ADDRESS_MUX_DISABLE;     // 地址/数据线不复用
    SRAM_Handler.Init.MemoryType = FSMC_MEMORY_TYPE_NOR;                  // NOR
    SRAM_Handler.Init.MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_16;    // 16位数据宽度
    SRAM_Handler.Init.BurstAccessMode = FSMC_BURST_ACCESS_MODE_DISABLE;   // 是否使能突发访问,仅对同步突发存储器有效,此处未用到
    SRAM_Handler.Init.WaitSignalPolarity = FSMC_WAIT_SIGNAL_POLARITY_LOW; // 等待信号的极性,仅在突发模式访问下有用
    SRAM_Handler.Init.WaitSignalActive = FSMC_WAIT_TIMING_BEFORE_WS;      // 存储器是在等待周期之前的一个时钟周期还是等待周期期间使能NWAIT
    SRAM_Handler.Init.WriteOperation = FSMC_WRITE_OPERATION_ENABLE;       // 存储器写使能
    SRAM_Handler.Init.WaitSignal = FSMC_WAIT_SIGNAL_DISABLE;              // 等待使能位,此处未用到
    SRAM_Handler.Init.ExtendedMode = FSMC_EXTENDED_MODE_DISABLE;          // 读写使用相同的时序
    SRAM_Handler.Init.AsynchronousWait = FSMC_ASYNCHRONOUS_WAIT_DISABLE;  // 是否使能同步传输模式下的等待信号,此处未用到
    SRAM_Handler.Init.WriteBurst = FSMC_WRITE_BURST_DISABLE;              // 禁止突发写

    // /* SRAM controller initialization */
    ILI9341_GPIO_Config();
    HAL_SRAM_Init(&SRAM_Handler, &Timing, &Timing);
}

/**
 * @brief  初始化ILI9341寄存器
 * @param  无
 * @retval 无
 */
static void ILI9341_REG_Config(void)
{

    /*  Power control B (CFh)  */
    DEBUG_DELAY();
    ILI9341_Write_Cmd(0xCF);
    ILI9341_Write_Data(0x00);
    ILI9341_Write_Data(0x81);
    ILI9341_Write_Data(0x30);

    /*  Power on sequence control (EDh) */
    DEBUG_DELAY();
    ILI9341_Write_Cmd(0xED);
    ILI9341_Write_Data(0x64);
    ILI9341_Write_Data(0x03);
    ILI9341_Write_Data(0x12);
    ILI9341_Write_Data(0x81);

    /*  Driver timing control A (E8h) */
    DEBUG_DELAY();
    ILI9341_Write_Cmd(0xE8);
    ILI9341_Write_Data(0x85);
    ILI9341_Write_Data(0x10);
    ILI9341_Write_Data(0x78);

    /*  Power control A (CBh) */
    DEBUG_DELAY();
    ILI9341_Write_Cmd(0xCB);
    ILI9341_Write_Data(0x39);
    ILI9341_Write_Data(0x2C);
    ILI9341_Write_Data(0x00);
    ILI9341_Write_Data(0x34);
    // ILI9341_Write_Data ( 0x02 );
    ILI9341_Write_Data(0x06); // 原来是0x02改为0x06可防止液晶显示白屏时有条纹的情况

    /* Pump ratio control (F7h) */
    DEBUG_DELAY();
    ILI9341_Write_Cmd(0xF7);
    ILI9341_Write_Data(0x20);

    /* Driver timing control B */
    DEBUG_DELAY();
    ILI9341_Write_Cmd(0xEA);
    ILI9341_Write_Data(0x00);
    ILI9341_Write_Data(0x00);

    /* Frame Rate Control (In Normal Mode/Full Colors) (B1h) */
    DEBUG_DELAY();
    ILI9341_Write_Cmd(0xB1);
    ILI9341_Write_Data(0x00);
    ILI9341_Write_Data(0x1B);

    /*  Display Function Control (B6h) */
    DEBUG_DELAY();
    ILI9341_Write_Cmd(0xB6);
    ILI9341_Write_Data(0x0A);
    ILI9341_Write_Data(0xA2);

    /* Power Control 1 (C0h) */
    DEBUG_DELAY();
    ILI9341_Write_Cmd(0xC0);
    ILI9341_Write_Data(0x35);

    /* Power Control 2 (C1h) */
    DEBUG_DELAY();
    ILI9341_Write_Cmd(0xC1);
    ILI9341_Write_Data(0x11);

    /* VCOM Control 1 (C5h) */
    ILI9341_Write_Cmd(0xC5);
    ILI9341_Write_Data(0x45);
    ILI9341_Write_Data(0x45);

    /*  VCOM Control 2 (C7h)  */
    ILI9341_Write_Cmd(0xC7);
    ILI9341_Write_Data(0xA2);

    /* Enable 3G (F2h) */
    ILI9341_Write_Cmd(0xF2);
    ILI9341_Write_Data(0x00);

    /* Gamma Set (26h) */
    ILI9341_Write_Cmd(0x26);
    ILI9341_Write_Data(0x01);
    DEBUG_DELAY();

    /* Positive Gamma Correction */
    ILI9341_Write_Cmd(0xE0); // Set Gamma
    ILI9341_Write_Data(0x0F);
    ILI9341_Write_Data(0x26);
    ILI9341_Write_Data(0x24);
    ILI9341_Write_Data(0x0B);
    ILI9341_Write_Data(0x0E);
    ILI9341_Write_Data(0x09);
    ILI9341_Write_Data(0x54);
    ILI9341_Write_Data(0xA8);
    ILI9341_Write_Data(0x46);
    ILI9341_Write_Data(0x0C);
    ILI9341_Write_Data(0x17);
    ILI9341_Write_Data(0x09);
    ILI9341_Write_Data(0x0F);
    ILI9341_Write_Data(0x07);
    ILI9341_Write_Data(0x00);

    /* Negative Gamma Correction (E1h) */
    ILI9341_Write_Cmd(0XE1); // Set Gamma
    ILI9341_Write_Data(0x00);
    ILI9341_Write_Data(0x19);
    ILI9341_Write_Data(0x1B);
    ILI9341_Write_Data(0x04);
    ILI9341_Write_Data(0x10);
    ILI9341_Write_Data(0x07);
    ILI9341_Write_Data(0x2A);
    ILI9341_Write_Data(0x47);
    ILI9341_Write_Data(0x39);
    ILI9341_Write_Data(0x03);
    ILI9341_Write_Data(0x06);
    ILI9341_Write_Data(0x06);
    ILI9341_Write_Data(0x30);
    ILI9341_Write_Data(0x38);
    ILI9341_Write_Data(0x0F);

    /* memory access control set */
    DEBUG_DELAY();
    ILI9341_Write_Cmd(0x36);
    ILI9341_Write_Data(0xC8); /*竖屏  左上角到 (起点)到右下角 (终点)扫描方式*/
    DEBUG_DELAY();

    /* column address control set */
    ILI9341_Write_Cmd(CMD_SetCoordinateX);
    ILI9341_Write_Data(0x00);
    ILI9341_Write_Data(0x00);
    ILI9341_Write_Data(0x00);
    ILI9341_Write_Data(0xEF);

    /* page address control set */
    DEBUG_DELAY();
    ILI9341_Write_Cmd(CMD_SetCoordinateY);
    ILI9341_Write_Data(0x00);
    ILI9341_Write_Data(0x00);
    ILI9341_Write_Data(0x01);
    ILI9341_Write_Data(0x3F);

    /*  Pixel Format Set (3Ah)  */
    DEBUG_DELAY();
    ILI9341_Write_Cmd(0x3a);
    ILI9341_Write_Data(0x55);

    /* Sleep Out (11h)  */
    ILI9341_Write_Cmd(0x11);
    ILI9341_Delay(0xAFFf << 2);
    DEBUG_DELAY();

    /* Display ON (29h) */
    ILI9341_Write_Cmd(0x29);
}

/**
 * @brief  ILI9341初始化函数，如果要用到lcd，一定要调用这个函数
 * @param  无
 * @retval 无
 */
void ILI9341_Init(void)
{
    ILI9341_GPIO_Config();
    ILI9341_FSMC_Config();
    ILI9341_DMA_Config();
    
    ILI9341_BackLed_Control(ENABLE); // 点亮LCD背光灯
    ILI9341_Rst();
    ILI9341_REG_Config();

    // 设置默认扫描方向，其中 6 模式为大部分液晶例程的默认显示方向
    ILI9341_GramScan(LCD_SCAN_MODE);
    ILI9341_Clear(0, 0, ILI9341_MORE_PIXEL, ILI9341_LESS_PIXEL);
}

/**
 * @brief  ILI9341G背光LED控制
 * @param  enumState ：决定是否使能背光LED
 *   该参数为以下值之一：
 *     @arg ENABLE :使能背光LED
 *     @arg DISABLE :禁用背光LED
 * @retval 无
 */
void ILI9341_BackLed_Control(FunctionalState enumState)
{
    if (enumState)
    {
        digitalL(GPIOD, GPIO_PIN_12);
    }
    else
    {
        digitalH(GPIOD, GPIO_PIN_12);
    }
}

/**
 * @brief  ILI9341G 软件复位
 * @param  无
 * @retval 无
 */
void ILI9341_Rst(void)
{
    digitalL(GPIOE, GPIO_PIN_1); // 低电平复位

    ILI9341_Delay(0xAFF);

    digitalH(GPIOE, GPIO_PIN_1);

    ILI9341_Delay(0xAFF);
}

/**
 * @brief  设置ILI9341的GRAM的扫描方向
 * @param  ucOption ：选择GRAM的扫描方向
 *     @arg 0-7 :参数可选值为0-7这八个方向
 *
 *	！！！其中0、3、5、6 模式适合从左至右显示文字，
 *				不推荐使用其它模式显示文字	其它模式显示文字会有镜像效果
 *
 *	其中0、2、4、6 模式的X方向像素为240，Y方向像素为320
 *	其中1、3、5、7 模式下X方向像素为320，Y方向像素为240
 *
 *	其中 6 模式为大部分液晶例程的默认显示方向
 *	其中 3 模式为摄像头例程使用的方向
 *	其中 0 模式为BMP图片显示例程使用的方向
 *
 * @retval 无
 * @note  坐标图例：A表示向上，V表示向下，<表示向左，>表示向右
                    X表示X轴，Y表示Y轴

------------------------------------------------------------
模式0：				.		模式1：		.	模式2：			.	模式3：
                    A		.					A		.		A					.		A
                    |		.					|		.		|					.		|
                    Y		.					X		.		Y					.		X
                    0		.					1		.		2					.		3
    <--- X0 o		.	<----Y1	o		.		o 2X--->  .		o 3Y--->
------------------------------------------------------------
模式4：				.	模式5：			.	模式6：			.	模式7：
    <--- X4 o		.	<--- Y5 o		.		o 6X--->  .		o 7Y--->
                    4		.					5		.		6					.		7
                    Y		.					X		.		Y					.		X
                    |		.					|		.		|					.		|
                    V		.					V		.		V					.		V
---------------------------------------------------------
                                             LCD屏示例
                                |-----------------|
                                |			野火Logo		|
                                |									|
                                |									|
                                |									|
                                |									|
                                |									|
                                |									|
                                |									|
                                |									|
                                |-----------------|
                                屏幕正面（宽240，高320）

 *******************************************************/
void ILI9341_GramScan(uint8_t ucOption)
{
    // 参数检查，只可输入0-7
    if (ucOption > 7)
        return;

    // 根据模式更新LCD_SCAN_MODE的值，主要用于触摸屏选择计算参数
    LCD_SCAN_MODE = ucOption;

    // 根据模式更新XY方向的像素宽度
    if (ucOption % 2 == 0)
    {
        // 0 2 4 6模式下X方向像素宽度为240，Y方向为320
        LCD_X_LENGTH = ILI9341_LESS_PIXEL;
        LCD_Y_LENGTH = ILI9341_MORE_PIXEL;
    }
    else
    {
        // 1 3 5 7模式下X方向像素宽度为320，Y方向为240
        LCD_X_LENGTH = ILI9341_MORE_PIXEL;
        LCD_Y_LENGTH = ILI9341_LESS_PIXEL;
    }

    // 0x36命令参数的高3位可用于设置GRAM扫描方向
    ILI9341_Write_Cmd(0x36);
    ILI9341_Write_Data(0x08 | (ucOption << 5)); // 根据ucOption的值设置LCD参数，共0-7种模式
    ILI9341_Write_Cmd(CMD_SetCoordinateX);
    ILI9341_Write_Data(0x00);                             /* x 起始坐标高8位 */
    ILI9341_Write_Data(0x00);                             /* x 起始坐标低8位 */
    ILI9341_Write_Data(((LCD_X_LENGTH - 1) >> 8) & 0xFF); /* x 结束坐标高8位 */
    ILI9341_Write_Data((LCD_X_LENGTH - 1) & 0xFF);        /* x 结束坐标低8位 */

    ILI9341_Write_Cmd(CMD_SetCoordinateY);
    ILI9341_Write_Data(0x00);                             /* y 起始坐标高8位 */
    ILI9341_Write_Data(0x00);                             /* y 起始坐标低8位 */
    ILI9341_Write_Data(((LCD_Y_LENGTH - 1) >> 8) & 0xFF); /* y 结束坐标高8位 */
    ILI9341_Write_Data((LCD_Y_LENGTH - 1) & 0xFF);        /* y 结束坐标低8位 */

    /* write gram start */
    ILI9341_Write_Cmd(CMD_SetPixel);
}

/**
 * @brief  在ILI9341显示器上开辟一个窗口
 * @param  usX ：在特定扫描方向下窗口的起点X坐标
 * @param  usY ：在特定扫描方向下窗口的起点Y坐标
 * @param  usWidth ：窗口的宽度
 * @param  usHeight ：窗口的高度
 * @retval 无
 */
void ILI9341_OpenWindow(uint16_t usX, uint16_t usY, uint16_t usWidth, uint16_t usHeight)
{
    ILI9341_Write_Cmd(CMD_SetCoordinateX); /* 设置X坐标 */
    ILI9341_Write_Data(usX >> 8);          /* 先高8位，然后低8位 */
    ILI9341_Write_Data(usX & 0xff);        /* 设置起始点和结束点*/
    ILI9341_Write_Data((usX + usWidth - 1) >> 8);
    ILI9341_Write_Data((usX + usWidth - 1) & 0xff);

    ILI9341_Write_Cmd(CMD_SetCoordinateY); /* 设置Y坐标*/
    ILI9341_Write_Data(usY >> 8);
    ILI9341_Write_Data(usY & 0xff);
    ILI9341_Write_Data((usY + usHeight - 1) >> 8);
    ILI9341_Write_Data((usY + usHeight - 1) & 0xff);
}

/**
 * @brief  设定ILI9341的光标坐标
 * @param  usX ：在特定扫描方向下光标的X坐标
 * @param  usY ：在特定扫描方向下光标的Y坐标
 * @retval 无
 */
static void ILI9341_SetCursor(uint16_t usX, uint16_t usY)
{
    ILI9341_OpenWindow(usX, usY, 1, 1);
}

/**
 * @brief  在ILI9341显示器上以某一颜色填充像素点
 * @param  ulAmout_Point ：要填充颜色的像素点的总数目
 * @param  usColor ：颜色
 * @retval 无
 */
static __inline void ILI9341_FillColor(uint32_t ulAmout_Point, uint16_t usColor)
{
    uint32_t i = 0;

    /* memory write */
    ILI9341_Write_Cmd(CMD_SetPixel);

    for (i = 0; i < ulAmout_Point; i++)
        ILI9341_Write_Data(usColor);
}

/**
 * @brief  对ILI9341显示器的某一窗口以某种颜色进行清屏
 * @param  usX ：在特定扫描方向下窗口的起点X坐标
 * @param  usY ：在特定扫描方向下窗口的起点Y坐标
 * @param  usWidth ：窗口的宽度
 * @param  usHeight ：窗口的高度
 * @note 可使用LCD_SetBackColor、LCD_SetTextColor、LCD_SetColors函数设置颜色
 * @retval 无
 */
void ILI9341_Clear(uint16_t usX, uint16_t usY, uint16_t usWidth, uint16_t usHeight)
{
    ILI9341_OpenWindow(usX, usY, usWidth, usHeight);

    ILI9341_FillColor(usWidth * usHeight, CurrentBackColor);
}

/**
 * @brief  对ILI9341显示器的某一点以某种颜色进行填充
 * @param  usX ：在特定扫描方向下该点的X坐标
 * @param  usY ：在特定扫描方向下该点的Y坐标
 * @note 可使用LCD_SetBackColor、LCD_SetTextColor、LCD_SetColors函数设置颜色
 * @retval 无
 */
void ILI9341_SetPointPixel(uint16_t usX, uint16_t usY)
{
    if ((usX < LCD_X_LENGTH) && (usY < LCD_Y_LENGTH))
    {
        ILI9341_SetCursor(usX, usY);

        ILI9341_FillColor(1, CurrentTextColor);
    }
}

/**
 * @brief  读取 GRAM 的一个像素数据
 * @param  无
 * @retval 像素数据
 */
static uint16_t ILI9341_Read_PixelData(void)
{
    uint16_t usRG = 0, usB = 0;

    ILI9341_Write_Cmd(0x2E); /* 读数据 */
    // 去掉前一次读取结果
    ILI9341_Read_Data(); /*FIRST READ OUT DUMMY DATA*/

    // 获取红色通道与绿色通道的值
    usRG = ILI9341_Read_Data(); /*READ OUT RED AND GREEN DATA  */
    usB = ILI9341_Read_Data();  /*READ OUT BLUE DATA*/

    return ((usRG & 0xF800) | ((usRG << 3) & 0x7E0) | (usB >> 11));
}

/**
 * @brief  连续写入像素数据（RGB565）。调用前应先通过 ILI9341_OpenWindow 设定好窗口。
 * @param  data: 像素数组指针（每像素 16 位 RGB565）
 * @param  length: 像素个数
 */
void ILI9341_WritePixels(const uint16_t *data, uint32_t length)
{
    if (length == 0 || data == NULL)
        return;
    /* memory write */
    ILI9341_Write_Cmd(CMD_SetPixel);
    while (length--)
    {
        ILI9341_Write_Data(*data++);
    }
}
/* 快速整块写入 RGB565 像素到区域 (x,y,w,h) */
void ILI9341_BlitAreaFast(uint16_t x, uint16_t y,
                          uint16_t w, uint16_t h,
                          const uint16_t *pixels)
{
    if (w == 0 || h == 0 || pixels == NULL)
        return;

    /* 设定一次完整窗口 */
    ILI9341_OpenWindow(x, y, w, h);

    /* 开始写GRAM */
    *(__IO uint16_t *)(FSMC_Addr_ILI9341_CMD) = CMD_SetPixel;

    /* 目标数据寄存器（16位并口） */
    volatile uint16_t *lcd = (volatile uint16_t *)FSMC_Addr_ILI9341_DATA;

    /* 连续写入 w*h 个像素（循环展开加速） */
    uint32_t count = (uint32_t)w * (uint32_t)h;

    while (count >= 8)
    {
        lcd[0] = *pixels++;
        lcd[0] = *pixels++;
        lcd[0] = *pixels++;
        lcd[0] = *pixels++;
        lcd[0] = *pixels++;
        lcd[0] = *pixels++;
        lcd[0] = *pixels++;
        lcd[0] = *pixels++;
        count -= 8;
    }
    while (count--)
    {
        lcd[0] = *pixels++;
    }
}
/**
 * @brief  获取 ILI9341 显示器上某一个坐标点的像素数据
 * @param  usX ：在特定扫描方向下该点的X坐标
 * @param  usY ：在特定扫描方向下该点的Y坐标
 * @retval 像素数据
 */
uint16_t ILI9341_GetPointPixel(uint16_t usX, uint16_t usY)
{
    uint16_t usPixelData;

    ILI9341_SetCursor(usX, usY);

    usPixelData = ILI9341_Read_PixelData();

    return usPixelData;
}

/**
 * @brief  设置LCD的前景(字体)及背景颜色,RGB565
 * @param  TextColor: 指定前景(字体)颜色
 * @param  BackColor: 指定背景颜色
 * @retval None
 */
void LCD_SetColors(uint16_t TextColor, uint16_t BackColor)
{
    CurrentTextColor = TextColor;
    CurrentBackColor = BackColor;
}

/**
 * @brief  获取LCD的前景(字体)及背景颜色,RGB565
 * @param  TextColor: 用来存储前景(字体)颜色的指针变量
 * @param  BackColor: 用来存储背景颜色的指针变量
 * @retval None
 */
void LCD_GetColors(uint16_t *TextColor, uint16_t *BackColor)
{
    *TextColor = CurrentTextColor;
    *BackColor = CurrentBackColor;
}

/**
 * @brief  设置LCD的前景(字体)颜色,RGB565
 * @param  Color: 指定前景(字体)颜色
 * @retval None
 */
void LCD_SetTextColor(uint16_t Color)
{
    CurrentTextColor = Color;
}

/**
 * @brief  设置LCD的背景颜色,RGB565
 * @param  Color: 指定背景颜色
 * @retval None
 */
void LCD_SetBackColor(uint16_t Color)
{
    CurrentBackColor = Color;
}
/*********************end of file*************************/

// 配置DMA用于ILI9341的GRAM写入
/* USER CODE BEGIN 2 */

// 设置LCD显存缓存区
uint16_t GRAM_Buffer[GRAM_HEIGHT][GRAM_WIDTH];

void ILI9341_DMA_Config(void)
{
    /* DMA is initialized once in main (MX_DMA_Init). Avoid re-initializing here. */
}

void ILI9341_WriteBuffer(uint16_t Color)
{
    for (uint32_t y = 0; y < GRAM_HEIGHT; y++)
    {
        for (uint32_t x = 0; x < GRAM_WIDTH; x++)
        {
            GRAM_Buffer[y][x] = Color;
        }
    }
}
// 8位数据转化位16位数据，并存储在GRAM_Buffer中
void ILI9341_Data_Tranfer(uint8_t *data, uint16_t w, uint16_t h)
{
    uint32_t k = 0;
    uint16_t PixL, PixH;
    if (w == 0 || h == 0)
        return;
    for (uint32_t y = 0; y < h; y++)
    {
        for (uint32_t x = 0; x < w; x++)
        {
            PixL = *(data + k * 2); // 数据低位在前
            PixH = *(data + k * 2 + 1);
            GRAM_Buffer[y][x] = ((PixH << 8) | PixL);
            k++;
        }
    }
}

void ILI9341_DMA_WritePixels(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                             const uint16_t *data, uint32_t length)
{
    if (w == 0 || h == 0)
        return;
    uint32_t need = (uint32_t)w * (uint32_t)h;
    // if (data == NULL)
    //     data = &GRAM_Buffer[0][0]; // 允许传 NULL 时用内部缓冲
    // if (length < need)
    //     need = length; // 如果传入长度小于窗口像素数, 只传 length 个

    ILI9341_OpenWindow(x, y, w, h);
    ILI9341_Write_Cmd(CMD_SetPixel);

    // 启动DMA: 源 data, 目的 LCD 数据寄存器 (FSMC映射地址)
    HAL_DMA_Start_IT(&hdma_memtomem_dma1_channel6, (uint32_t)data, (uint32_t)FSMC_Addr_ILI9341_DATA, need);
    //采用中断时，注释掉下面的轮询函数，否则会阻塞在这里，无法响应DMA中断，导致DMA传输无法完成，完成检测在DMA中断回调函数中实现
    //如果不使用中断，可以使用下面的轮询函数等待DMA传输完成，但会阻塞CPU，无法执行其他任务
    //HAL_DMA_PollForTransfer(&hdma_memtomem_dma1_channel6, HAL_DMA_FULL_TRANSFER, HAL_MAX_DELAY);
}

/*工具函数仅供内部部分函数使用*/

/**
 * 函    数：次方函数
 * 参    数：X 底数
 * 参    数：Y 指数
 * 返 回 值：等于X的Y次方
 */
uint32_t ILI9341_Pow(uint32_t X, uint32_t Y)
{
    uint32_t Result = 1; // 结果默认为1
    while (Y--)          // 累乘Y次
    {
        Result *= X; // 每次把X累乘到结果上
    }
    return Result;
}

/**
 * 函    数：判断指定点是否在指定多边形内部
 * 参    数：nvert 多边形的顶点数
 * 参    数：vertx verty 包含多边形顶点的x和y坐标的数组
 * 参    数：testx testy 测试点的X和y坐标
 * 返 回 值：指定点是否在指定多边形内部，1：在内部，0：不在内部
 */
uint8_t ILI9341_pnpoly(uint8_t nvert, int16_t *vertx, int16_t *verty, int16_t testx, int16_t testy)
{
    int16_t i, j, c = 0;

    /*此算法由W. Randolph Franklin提出*/
    /*参考链接：https://wrfranklin.org/Research/Short_Notes/pnpoly.html*/
    for (i = 0, j = nvert - 1; i < nvert; j = i++)
    {
        if (((verty[i] > testy) != (verty[j] > testy)) &&
            (testx < (vertx[j] - vertx[i]) * (testy - verty[i]) / (verty[j] - verty[i]) + vertx[i]))
        {
            c = !c;
        }
    }
    return c;
}

/**
 * 函    数：判断指定点是否在指定角度内部
 * 参    数：X Y 指定点的坐标
 * 参    数：StartAngle EndAngle 起始角度和终止角度，范围：-180~180
 *           水平向右为0度，水平向左为180度或-180度，下方为正数，上方为负数，顺时针旋转
 * 返 回 值：指定点是否在指定角度内部，1：在内部，0：不在内部
 */
uint8_t ILI9341_IsInAngle(int16_t X, int16_t Y, int16_t StartAngle, int16_t EndAngle)
{
    int16_t PointAngle;
    PointAngle = atan2(Y, X) / 3.14 * 180; // 计算指定点的弧度，并转换为角度表示
    if (StartAngle < EndAngle)             // 起始角度小于终止角度的情况
    {
        /*如果指定角度在起始终止角度之间，则判定指定点在指定角度*/
        if (PointAngle >= StartAngle && PointAngle <= EndAngle)
        {
            return 1;
        }
    }
    else // 起始角度大于于终止角度的情况
    {
        /*如果指定角度大于起始角度或者小于终止角度，则判定指定点在指定角度*/
        if (PointAngle >= StartAngle || PointAngle <= EndAngle)
        {
            return 1;
        }
    }
    return 0; // 不满足以上条件，则判断判定指定点不在指定角度
}

/*********************工具函数*/

//************一般操作(非DMA)********* */
void ILI9341_ShowImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *data)
{
    ILI9341_OpenWindow(x, y, w, h);
    ILI9341_Write_Cmd(CMD_SetPixel);
    uint16_t i;
    uint16_t PixL, PixH;
    for (i = 0; i < w * h; i++)
    {
        PixL = *(data + i * 2); // 数据低位在前
        PixH = *(data + i * 2 + 1);
        ILI9341_Write_Data((PixH << 8) | PixL);
    }
}

void ILI9341_ShowChar(uint16_t Xstart, uint16_t Ystart, char Char, FontDef Font)
{
    uint16_t FontLength = (Font.Height * Font.Width) / 8;          // 得到字体一个字符对应点阵集所占的字节数
    const uint8_t *pChar = &Font.Table[(Char - ' ') * FontLength]; // 得到字符对应点阵集的起始地址
    ILI9341_OpenWindow(Xstart, Ystart, Font.Width, Font.Height);
    ILI9341_Write_Cmd(CMD_SetPixel);
    uint16_t byteCount, bitCount;
    for (byteCount = 0; byteCount < FontLength; byteCount++)
    {
        // 一位一位处理要显示的颜色
        for (bitCount = 0; bitCount < 8; bitCount++)
        {
            if (pChar[byteCount] & (0x80 >> bitCount))
                ILI9341_Write_Data(CurrentTextColor);
            else
                ILI9341_Write_Data(CurrentBackColor);
        }
    }
}

void ILI9341_ShowString(uint16_t Xstart, uint16_t Ystart, const char *pString, FontDef Font)
{
    uint16_t i;
    for (i = 0; pString[i] != '\0'; i++) // 遍历字符串的每个字符
    {
        /*调用ILI9341_ShowChar函数，依次显示每个字符*/
        ILI9341_ShowChar(Xstart + i * Font.Width, Ystart, pString[i], Font);
    }
}

/**
 * 函    数：ILI9341显示数字（十进制，正整数）
 * 参    数：X 指定数字左上角的横坐标，范围：-32768~32767，屏幕区域：0~127
 * 参    数：Y 指定数字左上角的纵坐标，范围：-32768~32767，屏幕区域：0~63
 * 参    数：Number 指定要显示的数字，范围：0~4294967295
 * 参    数：Length 指定数字的长度，范围：0~10
 * 参    数：FontSize 指定字体大小
 *           范围：ILI9341_8X16		宽8像素，高16像素
 *                 ILI9341_6X8		宽6像素，高8像素
 * 返 回 值：无
 * 说    明：调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
 */
void ILI9341_ShowNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, FontDef Font)
{
    uint8_t i;
    for (i = 0; i < Length; i++) // 遍历数字的每一位
    {
        /*调用ILI9341_ShowChar函数，依次显示每个数字*/
        /*Number / ILI9341_Pow(10, Length - i - 1) % 10 可以十进制提取数字的每一位*/
        /*+ '0' 可将数字转换为字符格式*/
        ILI9341_ShowChar(X + i * Font.Width, Y, Number / ILI9341_Pow(10, Length - i - 1) % 10 + '0', Font);
    }
}

/**
 * 函    数：ILI9341显示有符号数字（十进制，整数）
 * 参    数：X 指定数字左上角的横坐标，范围：-32768~32767，屏幕区域：0~127
 * 参    数：Y 指定数字左上角的纵坐标，范围：-32768~32767，屏幕区域：0~63
 * 参    数：Number 指定要显示的数字，范围：-2147483648~2147483647
 * 参    数：Length 指定数字的长度，范围：0~10
 * 参    数：FontSize 指定字体大小
 *           范围：ILI9341_8X16		宽8像素，高16像素
 *                 ILI9341_6X8		宽6像素，高8像素
 * 返 回 值：无
 * 说    明：调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
 */
void ILI9341_ShowSignedNum(int16_t X, int16_t Y, int32_t Number, uint8_t Length, FontDef Font)
{
    uint8_t i;
    uint32_t Number1;

    if (Number >= 0) // 数字大于等于0
    {
        ILI9341_ShowChar(X, Y, '+', Font); // 显示+号
        Number1 = Number;                  // Number1直接等于Number
    }
    else // 数字小于0
    {
        ILI9341_ShowChar(X, Y, '-', Font); // 显示-号
        Number1 = -Number;                 // Number1等于Number取负
    }

    for (i = 0; i < Length; i++) // 遍历数字的每一位
    {
        /*调用ILI9341_ShowChar函数，依次显示每个数字*/
        /*Number1 / ILI9341_Pow(10, Length - i - 1) % 10 可以十进制提取数字的每一位*/
        /*+ '0' 可将数字转换为字符格式*/
        ILI9341_ShowChar(X + (i + 1) * Font.Width, Y, Number1 / ILI9341_Pow(10, Length - i - 1) % 10 + '0', Font);
    }
}

/**
 * 函    数：ILI9341显示浮点数字（十进制，小数）
 * 参    数：X 指定数字左上角的横坐标，范围：-32768~32767，屏幕区域：0~127
 * 参    数：Y 指定数字左上角的纵坐标，范围：-32768~32767，屏幕区域：0~63
 * 参    数：Number 指定要显示的数字，范围：-4294967295.0~4294967295.0
 * 参    数：IntLength 指定数字的整数位长度，范围：0~10
 * 参    数：FraLength 指定数字的小数位长度，范围：0~9，小数进行四舍五入显示
 * 参    数：FontSize 指定字体大小
 *           范围：ILI9341_8X16		宽8像素，高16像素
 *                 ILI9341_6X8		宽6像素，高8像素
 * 返 回 值：无
 * 说    明：调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
 */
void ILI9341_ShowFloatNum(int16_t X, int16_t Y, double Number, uint8_t IntLength, uint8_t FraLength, FontDef Font)
{
    uint32_t PowNum, IntNum, FraNum;

    if (Number >= 0) // 数字大于等于0
    {
        ILI9341_ShowChar(X, Y, '+', Font); // 显示+号
    }
    else // 数字小于0
    {
        ILI9341_ShowChar(X, Y, '-', Font); // 显示-号
        Number = -Number;                  // Number取负
    }

    /*提取整数部分和小数部分*/
    IntNum = Number;                     // 直接赋值给整型变量，提取整数
    Number -= IntNum;                    // 将Number的整数减掉，防止之后将小数乘到整数时因数过大造成错误
    PowNum = ILI9341_Pow(10, FraLength); // 根据指定小数的位数，确定乘数
    FraNum = round(Number * PowNum);     // 将小数乘到整数，同时四舍五入，避免显示误差
    IntNum += FraNum / PowNum;           // 若四舍五入造成了进位，则需要再加给整数

    /*显示整数部分*/
    ILI9341_ShowNum(X + Font.Width, Y, IntNum, IntLength, Font);

    /*显示小数点*/
    ILI9341_ShowChar(X + (IntLength + 1) * Font.Width, Y, '.', Font);

    /*显示小数部分*/
    ILI9341_ShowNum(X + (IntLength + 2) * Font.Width, Y, FraNum, FraLength, Font);
}

/**
 * 函    数：ILI9341显示汉字串
 * 参    数：X 指定汉字串左上角的横坐标，范围：-32768~32767，屏幕区域：0~127
 * 参    数：Y 指定汉字串左上角的纵坐标，范围：-32768~32767，屏幕区域：0~63
 * 参    数：Chinese 指定要显示的汉字串，范围：必须全部为汉字或者全角字符，不要加入任何半角字符
 *           显示的汉字需要在ILI9341_Data.c里的ILI9341_CF16x16数组定义
 *           未找到指定汉字时，会显示默认图形（一个方框，内部一个问号）
 * 返 回 值：无
 * 说    明：调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
 */
void ILI9341_ShowChinese(int16_t X, int16_t Y, char *Chinese)
{
    uint8_t pChinese = 0;
    uint8_t pIndex;
    uint8_t i;
    char SingleChinese[ILI9341_CHN_CHAR_WIDTH + 1] = {0};

    for (i = 0; Chinese[i] != '\0'; i++) // 遍历汉字串
    {
        SingleChinese[pChinese] = Chinese[i]; // 提取汉字串数据到单个汉字数组
        pChinese++;                           // 计次自增

        /*当提取次数到达ILI9341_CHN_CHAR_WIDTH时，即代表提取到了一个完整的汉字*/
        if (pChinese >= ILI9341_CHN_CHAR_WIDTH)
        {
            pChinese = 0; // 计次归零

            /*遍历整个汉字字模库，寻找匹配的汉字*/
            /*如果找到最后一个汉字（定义为空字符串），则表示汉字未在字模库定义，停止寻找*/
            for (pIndex = 0; strcmp(CFont16x16.Table[pIndex].Index, "") != 0; pIndex++)
            {
                /*找到匹配的汉字*/
                if (strcmp(CFont16x16.Table[pIndex].Index, SingleChinese) == 0)
                {
                    break; // 跳出循环，此时pIndex的值为指定汉字的索引
                }
            }

            /*将汉字字模库ILI9341_CF16x16的指定数据以16*16的图像格式显示*/
            ILI9341_ShowImage(X + ((i + 1) / ILI9341_CHN_CHAR_WIDTH - 1) * 16, Y, 16, 16, CFont16x16.Table[pIndex].Data);
        }
    }
}


/**
  * 函    数：ILI9341使用printf函数打印格式化字符串
  * 参    数：X 指定格式化字符串左上角的横坐标，范围：-32768~32767，屏幕区域：0~127
  * 参    数：Y 指定格式化字符串左上角的纵坐标，范围：-32768~32767，屏幕区域：0~63
  * 参    数：FontSize 指定字体大小
  *           范围：ILI9341_8X16		宽8像素，高16像素
  *                 ILI9341_6X8		宽6像素，高8像素
  * 参    数：format 指定要显示的格式化字符串，范围：ASCII码可见字符组成的字符串
  * 参    数：... 格式化字符串参数列表
  * 返 回 值：无
  * 说    明：调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
  */
void ILI9341_Printf(int16_t X, int16_t Y, FontDef Font, char *format, ...)
{
	char String[256];						//定义字符数组
	va_list arg;							//定义可变参数列表数据类型的变量arg
	va_start(arg, format);					//从format开始，接收参数列表到arg变量
	vsprintf(String, format, arg);			//使用vsprintf打印格式化字符串和参数列表到字符数组中
	va_end(arg);							//结束变量arg
	ILI9341_ShowString(X, Y, String, Font);//ILI9341显示字符数组（字符串）
	 
}