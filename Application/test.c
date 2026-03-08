#include "test.h"
#include "ILI9341.h"
#include <string.h>
#include <stdio.h>
// #include "image.h"
#include "bsp_spi_flash.h"


void FLASH_Test(void)
{
    SPI_FLASH_Init();
    ILI9341_Init();
    uint8_t buffer[256];
    uint32_t id = SPI_FLASH_ReadID();
    ILI9341_ShowNum(10, 10, id, 8, Font8x16);
    // 读取数据
    // 验证数据                                                                                                                    
    // for (size_t i = 0; i < sizeof(buffer); i++)
    // {
    //     if (buffer[i] != 0xA5)
    //     {
    //         FLASH_INFO("Data verification failed at index %zu", i);
    //         return;
    //     }
    // }                                            
    // FLASH_INFO("Data verification successful");
}


void ILI9341_Test(void)
{
    ILI9341_Init();
    ILI9341_ShowChar(10, 10, 'F', Font8x16);
    ILI9341_ShowString(60, 60, "Hello, World~", Font16x32);
    ILI9341_ShowChar(20, 20, 'A', Font16x24);
    ILI9341_ShowNum(10, 100, 12345, 5, Font8x16);
    LCD_SetTextColor(RED);
    ILI9341_ShowFloatNum(10, 130, -123.456, 3, 3, Font16x24);
}








//初始化PA4引脚，用于翻转电平，记录DMA传输完成状态
void ILI9341_Init_Status_Pin(void)
{
    GPIO_InitTypeDef GPIO_Initure;

    __HAL_RCC_GPIOA_CLK_ENABLE(); //使能GPIOA时钟

    GPIO_Initure.Pin = GPIO_PIN_4;               //PA4
    GPIO_Initure.Mode = GPIO_MODE_OUTPUT_PP;     //推挽输出
    GPIO_Initure.Pull = GPIO_NOPULL;              //不带上下拉
    GPIO_Initure.Speed = GPIO_SPEED_FREQ_HIGH;    //高速
    HAL_GPIO_Init(GPIOA, &GPIO_Initure);         //初始化PA4

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET); //初始状态为低
}


void LCD_Test(void)
{
    ILI9341_Init();
    ILI9341_DMA_Config();
    ILI9341_Init_Status_Pin();
    // 先用CPU方式写一块，验证基础显示功能
    //ILI9341_BlitAreaFast(0, 0, GRAM_WIDTH, GRAM_HEIGHT, (const uint16_t *)GRAM_Buffer);

    // 再用DMA方式写相邻区域(向右偏移)，便于区分是否生效
    uint16_t i=0;
    // ILI9341_Data_Tranfer((uint8_t *)gImage_image, 160,160);
    // ILI9341_DMA_WritePixels(0, 0, 160, 160, (const uint16_t *)GRAM_Buffer, 160 * 160);
    //ILI9341_ShowImage(80, 60, 240, 180, gImage_image);


    //循环刷色块
    while (1)
    {
        //PA4翻转，作为DMA传输完成标志
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_4);
        switch (i)
        {
        case 0:
            ILI9341_WriteBuffer(RED);
            //ILI9341_BlitAreaFast(0, 0, GRAM_WIDTH, GRAM_HEIGHT, (const uint16_t *)GRAM_Buffer);
            ILI9341_DMA_WritePixels(0, 0, GRAM_WIDTH, GRAM_HEIGHT, (const uint16_t *)GRAM_Buffer, GRAM_WIDTH * GRAM_HEIGHT);
            i++;
            break;
        case 1:
            ILI9341_WriteBuffer(YELLOW);
            //ILI9341_BlitAreaFast(0, 0, GRAM_WIDTH, GRAM_HEIGHT, (const uint16_t *)GRAM_Buffer);
            ILI9341_DMA_WritePixels(0, 0, GRAM_WIDTH, GRAM_HEIGHT, (const uint16_t *)GRAM_Buffer, GRAM_WIDTH * GRAM_HEIGHT);
            i++;
            break;
        case 2:
            ILI9341_WriteBuffer(GREEN);
            //ILI9341_BlitAreaFast(0, 0, GRAM_WIDTH, GRAM_HEIGHT, (const uint16_t *)GRAM_Buffer);
            ILI9341_DMA_WritePixels(0, 0, GRAM_WIDTH, GRAM_HEIGHT, (const uint16_t *)GRAM_Buffer, GRAM_WIDTH * GRAM_HEIGHT);
            i++;
            break;
        case 3:
            ILI9341_WriteBuffer(BLACK);
            //ILI9341_BlitAreaFast(0, 0, GRAM_WIDTH, GRAM_HEIGHT, (const uint16_t *)GRAM_Buffer);
            ILI9341_DMA_WritePixels(0, 0, GRAM_WIDTH, GRAM_HEIGHT, (const uint16_t *)GRAM_Buffer, GRAM_WIDTH * GRAM_HEIGHT);
            i++;
            break;
        case 4:
            ILI9341_WriteBuffer(GREY);
            //ILI9341_BlitAreaFast(0, 0, GRAM_WIDTH, GRAM_HEIGHT, (const uint16_t *)GRAM_Buffer);
            ILI9341_DMA_WritePixels(0, 0, GRAM_WIDTH, GRAM_HEIGHT, (const uint16_t *)GRAM_Buffer, GRAM_WIDTH * GRAM_HEIGHT);
            i++;
            break;
        case 5:
            ILI9341_WriteBuffer(CYAN);
            //ILI9341_BlitAreaFast(0, 0, GRAM_WIDTH, GRAM_HEIGHT, (const uint16_t *)GRAM_Buffer);
            ILI9341_DMA_WritePixels(0, 0, GRAM_WIDTH, GRAM_HEIGHT, (const uint16_t *)GRAM_Buffer, GRAM_WIDTH * GRAM_HEIGHT);
            i++;
            break;
        case 6:
            ILI9341_WriteBuffer(MAGENTA);
            //ILI9341_BlitAreaFast(0, 0, GRAM_WIDTH, GRAM_HEIGHT, (const uint16_t *)GRAM_Buffer);
            ILI9341_DMA_WritePixels(0, 0, GRAM_WIDTH, GRAM_HEIGHT, (const uint16_t *)GRAM_Buffer, GRAM_WIDTH * GRAM_HEIGHT);
            i++;
            break;
        default:i=0;
            break;

        }
    }
}

// void test_function(void)
// {

//     lv_init();
//     lv_port_disp_init();

//     // 使用lvgl显示一些内容
//     lv_obj_t *label = lv_label_create(lv_scr_act());
//     lv_label_set_text(label, "Hello");
//     lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
//     // 显示
// }

void PCM_Test(void)
{
    // This function is intentionally left empty.
    // It serves to verify that PCM5102A.c and PCM5102A.h are included correctly.
}
