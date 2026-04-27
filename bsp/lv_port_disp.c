/**
 * @file lv_port_disp_template.c
 *
 */

/*Copy this file as "lv_port_disp.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp.h"
#include <stdbool.h>
#include "ILI9341.h"
/*********************
 *      DEFINES
 *********************/
#define MY_DISP_HOR_RES    160      //当前实际为320
#define MY_DISP_VER_RES    120      //当前实际为240
#ifndef MY_DISP_HOR_RES
    #warning Please define or replace the macro MY_DISP_HOR_RES with the actual screen width, default value 320 is used for now.
    #define MY_DISP_HOR_RES    320
#endif

#ifndef MY_DISP_VER_RES
    #warning Please define or replace the macro MY_DISP_VER_RES with the actual screen height, default value 240 is used for now.
    #define MY_DISP_VER_RES    240
#endif

#define BYTE_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565)) /*will be 2 for RGB565 */

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);

static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);
/**********************
 *  STATIC VARIABLES
 **********************/
lv_display_t * current_disp = NULL;

 /**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_init(void)
{
    /*-------------------------
     * Initialize your display
     * -----------------------*/
    disp_init();

    /*------------------------------------
     * Create a display and set a flush_cb
     * -----------------------------------*/
    lv_display_t * disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);
    lv_display_set_flush_cb(disp, disp_flush);
    lv_display_set_default(disp);
    current_disp = disp;

    /* Example 1
     * One buffer for partial rendering*/
    // LV_ATTRIBUTE_MEM_ALIGN
    // static uint8_t buf_1_1[MY_DISP_HOR_RES * 10 * BYTE_PER_PIXEL];            /*A buffer for 10 rows*/
    // lv_display_set_buffers(disp, buf_1_1, NULL, sizeof(buf_1_1), LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* Example 2
     * Two buffers for partial rendering
     * In flush_cb DMA or similar hardware should be used to update the display in the background.*/
    LV_ATTRIBUTE_MEM_ALIGN
    static uint8_t buf_2_1[MY_DISP_HOR_RES * 10 * BYTE_PER_PIXEL];

    LV_ATTRIBUTE_MEM_ALIGN
    static uint8_t buf_2_2[MY_DISP_HOR_RES * 10 * BYTE_PER_PIXEL];
    lv_display_set_buffers(disp, buf_2_1, buf_2_2, sizeof(buf_2_1), LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* Example 3
     * Two buffers screen sized buffer for double buffering.
     * Both LV_DISPLAY_RENDER_MODE_DIRECT and LV_DISPLAY_RENDER_MODE_FULL works, see their comments*/
    // LV_ATTRIBUTE_MEM_ALIGN
    // static uint8_t buf_3_1[MY_DISP_HOR_RES * MY_DISP_VER_RES * BYTE_PER_PIXEL];

    // LV_ATTRIBUTE_MEM_ALIGN
    // static uint8_t buf_3_2[MY_DISP_HOR_RES * MY_DISP_VER_RES * BYTE_PER_PIXEL];
    // lv_display_set_buffers(disp, buf_3_1, buf_3_2, sizeof(buf_3_1), LV_DISPLAY_RENDER_MODE_DIRECT);

}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*Initialize your display and the required peripherals.*/
static void disp_init(void)
{
    /*You code here*/
    ILI9341_Init();
}

volatile bool disp_flush_enabled = true;

/* Enable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

/* Disable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

/*Flush the content of the internal buffer the specific area on the display.
 *`px_map` contains the rendered image as raw pixel map and it should be copied to `area` on the display.
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_display_flush_ready()' has to be called when it's finished.*/
static void disp_flush(lv_display_t * disp_drv, const lv_area_t * area, uint8_t * px_map)
{
    if(disp_flush_enabled) {
        int32_t w = area->x2 - area->x1 + 1;
        int32_t h = area->y2 - area->y1 + 1;
        const uint16_t *p = (const uint16_t *)px_map;

        //ILI9341_BlitAreaFast((uint16_t)area->x1, (uint16_t)area->y1,(uint16_t)w, (uint16_t)h, p);
        ILI9341_DMA_WritePixels((uint16_t)area->x1, (uint16_t)area->y1,(uint16_t)w, (uint16_t)h, p, (uint32_t)(w * h));
        // /* Row-by-row flush: set window per row and stream RGB565 pixels */
        // int32_t w = area->x2 - area->x1 + 1;
        // int32_t h = area->y2 - area->y1 + 1;

        // /* Basic sanity: if area is out of bounds just finish */
        // if(w <= 0 || h <= 0) {
        //     lv_display_flush_ready(disp_drv);
        //     return;
        // }

        // for(int32_t row = 0; row < h; row++) {
        //     int32_t y = area->y1 + row;
        //     /* Set a one-line window */
        //     ILI9341_OpenWindow((uint16_t)area->x1, (uint16_t)y, (uint16_t)w, 1);
        //     /* Pointer to the start of this row in px_map (RGB565 -> 2 bytes per pixel) */
        //     const uint16_t * p = (const uint16_t *)(px_map + (row * w) * 2);

        //     /* Stream pixels of this row */
        //     ILI9341_WritePixels(p, (uint32_t)w);
        // }
    } 
    else {
        /*IMPORTANT!!!
         *Inform the graphics library that you are ready with the flushing*/
        lv_display_flush_ready(disp_drv);
    }
}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
