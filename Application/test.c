#include "test.h"
#include "ILI9341.h"
#include <string.h>
#include <stdio.h>
// #include "image.h"
#include "bsp_spi_flash.h"
#include "lv_port_indev.h"
#include "lv_port_disp.h"
#include "lvgl.h"



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


static lv_obj_t *g_touch_status = NULL;

static void touch_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = lv_event_get_target(e);

    if(code == LV_EVENT_PRESSED) {
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x44AA44), 0);
        if(g_touch_status) lv_label_set_text(g_touch_status, "Status: PRESSED");
    }
    else if(code == LV_EVENT_RELEASED) {
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2E6BD1), 0);
        if(g_touch_status) lv_label_set_text(g_touch_status, "Status: RELEASED");
    }
    else if(code == LV_EVENT_CLICKED) {
        if(g_touch_status) lv_label_set_text(g_touch_status, "Status: CLICKED");
    }
}

/*
测试LVGL控制触摸屏
*/
void lv_test(void)
{
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();

    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);

    // 创建一个标签对象
    lv_obj_t * label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello, LVGL!");
    lv_obj_set_style_text_color(label, lv_color_hex(0x000000), 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    //LVGL触摸测试
    lv_obj_t * touch_label = lv_label_create(lv_scr_act());
    lv_label_set_text(touch_label, "Touch me!");
    lv_obj_set_style_text_color(touch_label, lv_color_hex(0x000000), 0);
    lv_obj_align(touch_label, LV_ALIGN_CENTER, 0, 40);

    lv_obj_t *btn = lv_button_create(lv_scr_act());
    lv_obj_set_size(btn, 140, 50);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2E6BD1), 0);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 95);
    lv_obj_add_event_cb(btn, touch_btn_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Tap Here");
    lv_obj_center(btn_label);

    g_touch_status = lv_label_create(lv_scr_act());
    lv_label_set_text(g_touch_status, "Status: WAIT");
    lv_obj_set_style_text_color(g_touch_status, lv_color_hex(0x000000), 0);
    lv_obj_align(g_touch_status, LV_ALIGN_BOTTOM_MID, 0, -10);


    while (1)
    {
        lv_timer_handler(); // 处理LVGL任务
        HAL_Delay(5); // 延时，避免占用过多CPU资源
    }
}

void PCM_Test(void)
{
    // This function is intentionally left empty.
    // It serves to verify that PCM5102A.c and PCM5102A.h are included correctly.
}
