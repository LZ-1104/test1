/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "log.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "ESP8266.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

osThreadId_t LVGL_TestTaskHandle;
const osThreadAttr_t LVGL_TestTask_attributes = {
  .name = "LVGL_TestTask",
  .stack_size = 1024 * 4, // 增加堆栈大小
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t ESP8266_TestTaskHandle;
const osThreadAttr_t ESP8266_TestTask_attributes = {
  .name = "ESP8266_TestTask",
  .stack_size = 256 * 4, // 增加堆栈大小
  .priority = (osPriority_t) osPriorityNormal,
};


void LVGL_TestTask(void *argument);
void ESP8266_TestTask(void *argument);




/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationTickHook(void);
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName);

/* USER CODE BEGIN 3 */
void vApplicationTickHook( void )
{
  lv_tick_inc(1); // 每个tick增加1毫秒
   /* This function will be called by each tick interrupt if
   configUSE_TICK_HOOK is set to 1 in FreeRTOSConfig.h. User code can be
   added here, but the tick hook is called from an interrupt context, so
   code must not attempt to block, and only the interrupt safe FreeRTOS API
   functions can be used (those that end in FromISR()). */
}
/* USER CODE END 3 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  log_Init();
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  LVGL_TestTaskHandle = osThreadNew(LVGL_TestTask, NULL, &LVGL_TestTask_attributes);
  ESP8266_TestTaskHandle = osThreadNew(ESP8266_TestTask, NULL, &ESP8266_TestTask_attributes);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  // 初始化PB1为输出，后续在这个task里控制它闪烁
  __HAL_RCC_GPIOB_CLK_ENABLE();
  GPIO_InitTypeDef GPIO_Initure;
  GPIO_Initure.Pin = GPIO_PIN_1;
  GPIO_Initure.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_Initure.Pull = GPIO_NOPULL;
  GPIO_Initure.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_Initure);

  /* Infinite loop */
  for(;;)
  {
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_1);
    osDelay(200);
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_1);
    osDelay(800);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

//添加LVGL测试任务
void LVGL_TestTask(void *argument)
{
    /* 初始化 LVGL 及显示接口 */
    lv_init();
    lv_port_disp_init();

    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);

    /* 创建一个简单的标签显示 "Hello, LVGL!" */
    lv_obj_t * label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "Hello, LVGL!");
    lv_obj_set_style_text_color(label, lv_color_hex(0x000000), 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_scr_load(lv_scr_act());
    /* 进入LVGL任务循环，处理LVGL事件和刷新 */
    while (1) {
        lv_task_handler(); // 处理LVGL任务
        osDelay(100); // 增加延时，减少CPU占用

        /* 堆栈监控 */
        if (uxTaskGetStackHighWaterMark(NULL) < 50) {
            log_Error("LVGL_TestTask 堆栈不足！");
        }
    }
}

void LED_TestTask(void *argument)
{
    /* 初始化 PB1 为输出 */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_Initure;
    GPIO_Initure.Pin = GPIO_PIN_5;
    GPIO_Initure.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_Initure.Pull = GPIO_NOPULL;
    GPIO_Initure.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_Initure);

    /* 无限循环，控制 LED 闪烁 */
    while (1) {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5); // 切换 LED 状态
        osDelay(500); // 延时 500ms

        /* 堆栈监控 */
        if (uxTaskGetStackHighWaterMark(NULL) < 20) {
            log_Error("LED_TestTask 堆栈不足！");
        }
    }
}

void ESP8266_TestTask(void *argument)
{
    ESP8266_Init();
    bool status = true;

    /*
    配置ESP8266连接WiFi、连接服务器、发送数据、接收数据的测试代码
    */
    if(ESP8266_CheckAlive()==true){
        log_Debug("ESP8266 模块响应正常");
    } else {
        log_Debug("ESP8266 模块无响应");
        //挂起当前任务，避免继续执行后续操作
        vTaskSuspend(NULL);
        return;
    }
    // 连接WiFi
    if(ESP8266_ConnectAP("LZ1104", "246813579A") == false) {
        log_Debug("连接WiFi失败");
        vTaskSuspend(NULL);
        return;
    } else {
        log_Debug("连接WiFi成功");
    }
    if(ESP8266_ConnectTcpServer("192.168.57.62", 8080) == false) {
        log_Debug("连接TCP服务器失败");
        vTaskSuspend(NULL);
        return;
    } else {
        log_Debug("连接TCP服务器成功");
    }
    while (1) {
      ESP8266_SendTcpData((const uint8_t*)"Hello from STM32!", 18);
        osDelay(1000);
    }

}

  /* Compatibility shim for CMSIS-RTOS2 wrappers that declare but do not define osThreadDetach. */
  __attribute__((weak)) osStatus_t osThreadDetach(osThreadId_t thread_id)
  {
    (void)thread_id;
    return osOK;
  }

/* USER CODE END Application */

