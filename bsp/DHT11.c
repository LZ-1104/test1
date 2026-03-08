#include "DHT11.h"
#include "ILI9341.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief  微秒级延时函数
 * @param  us: 延时时间（微秒）
 * @retval 无
 */
void DHT11_Delay_us(uint32_t us)
{
    volatile uint32_t i;
    for (i = 0; i < us * 7; i++)
    {
        __NOP();
    }
}

/**
 * @brief  毫秒级延时函数
 * @param  time: 延时时间（毫秒）
 * @retval 无
 */
void DHT11_Delay(uint16_t time)
{
    HAL_Delay(time);
}

/**
 * @brief  DHT11初始化函数
 * @param  无
 * @retval 无
 */
void DHT11_Init(void)
{
    DHT11_GPIO_CLK_ENABLE(); // 使能GPIO时钟
    GPIO_InitTypeDef GPIO_Initure;

    // 配置数据引脚为推挽输出
    GPIO_Initure.Pin = DHT11_DATA_PIN;
    GPIO_Initure.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_Initure.Pull = GPIO_PULLUP;
    GPIO_Initure.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_DATA_PORT, &GPIO_Initure);

    // 设置数据引脚为高电平
    HAL_GPIO_WritePin(DHT11_DATA_PORT, DHT11_DATA_PIN, GPIO_PIN_SET);
}

/**
 * @brief  从DHT11读取一个字节数据
 * @param  无
 * @retval 读取到的字节数据
 */
uint8_t DHT11_ReadByte(void)
{
    uint8_t i, byte = 0;

    for (i = 0; i < 8; i++)
    {
        // 等待数据引脚变高，表示开始传输位
        while (HAL_GPIO_ReadPin(DHT11_DATA_PORT, DHT11_DATA_PIN) == GPIO_PIN_RESET)
            ;

        DHT11_Delay_us(30); // 延时30微秒

        // 读取数据引脚状态
        if (HAL_GPIO_ReadPin(DHT11_DATA_PORT, DHT11_DATA_PIN) == GPIO_PIN_SET)
        {
            byte |= (1 << (7 - i)); // 设置对应位为1
        }

        // 等待数据引脚变低，准备读取下一位
        while (HAL_GPIO_ReadPin(DHT11_DATA_PORT, DHT11_DATA_PIN) == GPIO_PIN_SET)
            ;
    }

    return byte;
}

/**
 * @brief  启动DHT11传输
 * @param  无
 * @retval 0: 成功, 1: 失败
 */
uint8_t DHT11_Start(void)
{
    // 配置数据引脚为推挽输出
    GPIO_InitTypeDef GPIO_Initure;
    GPIO_Initure.Pin = DHT11_DATA_PIN;
    GPIO_Initure.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_Initure.Pull = GPIO_NOPULL;
    GPIO_Initure.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_DATA_PORT, &GPIO_Initure);

    // 拉低数据引脚至少18ms
    HAL_GPIO_WritePin(DHT11_DATA_PORT, DHT11_DATA_PIN, GPIO_PIN_RESET);
    DHT11_Delay(20); // 延时20ms

    // 拉高数据引脚20-40us
    HAL_GPIO_WritePin(DHT11_DATA_PORT, DHT11_DATA_PIN, GPIO_PIN_SET);
    DHT11_Delay_us(30); // 延时30us

    // 配置数据引脚为输入
    GPIO_Initure.Mode = GPIO_MODE_INPUT;
    GPIO_Initure.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DHT11_DATA_PORT, &GPIO_Initure);

    // 等待DHT11响应信号
    uint32_t timeout = 0;
    while (HAL_GPIO_ReadPin(DHT11_DATA_PORT, DHT11_DATA_PIN) == GPIO_PIN_SET)
    {
        if (++timeout > 1000) // 超时处理
            return 1;         // 无响应
        DHT11_Delay_us(1);
    }

    timeout = 0;
    while (HAL_GPIO_ReadPin(DHT11_DATA_PORT, DHT11_DATA_PIN) == GPIO_PIN_RESET)
    {
        if (++timeout > 1000) // 超时处理
            return 1;         // 无响应
        DHT11_Delay_us(1);
    }

    timeout = 0;
    while (HAL_GPIO_ReadPin(DHT11_DATA_PORT, DHT11_DATA_PIN) == GPIO_PIN_SET)
    {
        if (++timeout > 1000) // 超时处理
            return 1;         // 无响应
        DHT11_Delay_us(1);
    }

    return 0; // 响应成功
}

/**
 * @brief  读取DHT11温湿度数据
 * @param  temperature: 存储温度值的指针
 * @param  humidity: 存储湿度值的指针
 * @retval 0: 成功, 1: 启动失败, 2: 校验和错误
 */
uint8_t DHT11_ReadData(float *temperature, float *humidity)
{
    uint8_t data[5];
    uint8_t i;

    if (DHT11_Start() != 0)
        return 1; // 启动失败

    // 读取5个字节的数据
    for (i = 0; i < 5; i++)
    {
        data[i] = DHT11_ReadByte();
    }

    // 校验和验证
    if (data[4] != (data[0] + data[1] + data[2] + data[3]))
        return 2; // 校验和错误

    // 计算温度和湿度
    *humidity = data[0] + data[1] * 0.1f;
    *temperature = data[2] + data[3] * 0.1f;

    return 0; // 读取成功
}

/**
 * @brief  复位DHT11
 * @param  无
 * @retval 无
 */
void DHT11_Reset(void)
{
    // 配置数据引脚为推挽输出
    GPIO_InitTypeDef GPIO_Initure;
    GPIO_Initure.Pin = DHT11_DATA_PIN;
    GPIO_Initure.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_Initure.Pull = GPIO_NOPULL;
    GPIO_Initure.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_DATA_PORT, &GPIO_Initure);

    // 拉高数据引脚
    HAL_GPIO_WritePin(DHT11_DATA_PORT, DHT11_DATA_PIN, GPIO_PIN_SET);
    DHT11_Delay(1);
}

/**
 * @brief  在LCD上画线
 * @param  x1: 起始点X坐标
 * @param  y1: 起始点Y坐标
 * @param  x2: 结束点X坐标
 * @param  y2: 结束点Y坐标
 * @param  color: 线条颜色
 * @retval 无
 */
void DHT11_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    LCD_SetTextColor(color);
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy, e2;

    for (;;)
    {
        ILI9341_SetPointPixel(x1, y1);
        if (x1 == x2 && y1 == y2)
            break;
        e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x1 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y1 += sy;
        }
    }
}

// 辅助函数：在缓冲区中画点
void DrawPixel_Buffer(uint16_t *buffer, uint16_t width, uint16_t height, uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= width || y >= height)
        return;
    buffer[y * width + x] = color;
}

// 辅助函数：在缓冲区中画线
void DrawLine_Buffer(uint16_t *buffer, uint16_t width, uint16_t height, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
    int err = dx + dy, e2;

    for (;;)
    {
        DrawPixel_Buffer(buffer, width, height, x1, y1, color);
        if (x1 == x2 && y1 == y2)
            break;
        e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x1 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y1 += sy;
        }
    }
}

#define MAX_GRAPH_SAMPLES 320
float temp_history[MAX_GRAPH_SAMPLES];
int temp_history_count = 0;

float humi_history[MAX_GRAPH_SAMPLES];
int humi_history_count = 0;

/**
 * @brief  绘制动态曲线图
 * @param  value: 当前值
 * @param  history: 历史数据数组
 * @param  count: 历史数据计数指针
 * @param  y_start: 图表起始Y坐标
 * @param  height: 图表高度
 * @param  color: 曲线颜色
 * @retval 无
 */
void DHT11_DrawGraph(float value, float *history, int *count, uint16_t y_start, uint16_t height, uint16_t color)
{
    // Graph Layout
    const uint16_t GRAPH_MARGIN_LEFT = 40;
    const uint16_t GRAPH_MARGIN_RIGHT = 10;
    const uint16_t GRAPH_MARGIN_BOTTOM = 20;

    uint16_t GRAPH_Y_START = y_start;
    uint16_t GRAPH_HEIGHT = height;
    uint16_t GRAPH_Y_END = GRAPH_Y_START + GRAPH_HEIGHT;

    // Calculate dynamic width based on screen orientation
    uint16_t graph_width = LCD_X_LENGTH - GRAPH_MARGIN_LEFT - GRAPH_MARGIN_RIGHT;
    if (graph_width > MAX_GRAPH_SAMPLES)
        graph_width = MAX_GRAPH_SAMPLES;

    // Update History (Shift Left)
    if (*count < graph_width)
    {
        history[(*count)++] = value;
    }
    else
    {
        for (int i = 1; i < *count; i++)
        {
            history[i - 1] = history[i];
        }
        history[*count - 1] = value;
    }

    // Find Min/Max for auto-scaling
    float min_val = history[0];
    float max_val = history[0];
    for (int i = 1; i < *count; i++)
    {
        if (history[i] < min_val)
            min_val = history[i];
        if (history[i] > max_val)
            max_val = history[i];
    }

    // Add padding to range (at least 5 units span)
    float range_min = min_val - 1.0f;
    float range_max = max_val + 1.0f;
    if ((range_max - range_min) < 5.0f)
    {
        float center = (range_max + range_min) / 2.0f;
        range_min = center - 2.5f;
        range_max = center + 2.5f;
    }

    // Prepare Buffer
    // Use GRAM_Buffer as scratchpad. It is 160x160 (25600 pixels).
    // We need graph_width * GRAPH_HEIGHT pixels.
    // Max 320 * 60 = 19200 pixels. Safe.
    uint16_t *frame_buffer = (uint16_t *)GRAM_Buffer;
    
    // Fill with White (0xFFFF)
    memset(frame_buffer, 0xFF, graph_width * GRAPH_HEIGHT * sizeof(uint16_t));

    // Draw Axes into Buffer
    // Y Axis (at x=0 in buffer)
    DrawLine_Buffer(frame_buffer, graph_width, GRAPH_HEIGHT, 0, 0, 0, GRAPH_HEIGHT - 1, BLACK);
    // X Axis (at y=height-1 in buffer)
    DrawLine_Buffer(frame_buffer, graph_width, GRAPH_HEIGHT, 0, GRAPH_HEIGHT - 1, graph_width - 1, GRAPH_HEIGHT - 1, BLACK);

    // Draw Curve into Buffer
    uint16_t prev_x = 0, prev_y = 0;
    for (int i = 0; i < *count; i++)
    {
        uint16_t x = i; // Relative to buffer start (0)

        // Map value to Y
        float val = history[i];
        // y = bottom - (val - min) / (max - min) * height
        float normalized = (val - range_min) / (range_max - range_min);
        if (normalized < 0)
            normalized = 0;
        if (normalized > 1)
            normalized = 1;

        // In buffer, y=0 is top, y=height-1 is bottom.
        // We want graph bottom at height-1.
        uint16_t y = (GRAPH_HEIGHT - 1) - (uint16_t)(normalized * (GRAPH_HEIGHT - 1));

        if (i > 0)
        {
            DrawLine_Buffer(frame_buffer, graph_width, GRAPH_HEIGHT, prev_x, prev_y, x, y, color);
        }
        else
        {
            DrawPixel_Buffer(frame_buffer, graph_width, GRAPH_HEIGHT, x, y, color);
        }
        prev_x = x;
        prev_y = y;
    }

    // Flush Buffer to LCD
    ILI9341_BlitAreaFast(GRAPH_MARGIN_LEFT, GRAPH_Y_START, graph_width, GRAPH_HEIGHT, frame_buffer);

    // Draw Y Axis Labels (Directly to LCD, outside buffer area)
    LCD_SetBackColor(WHITE);
    LCD_SetTextColor(BLACK);
    char buf[10];
    // Max label
    sprintf(buf, "%.1f", range_max);
    ILI9341_ShowString(0, GRAPH_Y_START, buf, Font8x16);
    // Min label
    sprintf(buf, "%.1f", range_min);
    ILI9341_ShowString(0, GRAPH_Y_END - 16, buf, Font8x16);
    // Mid label
    float mid_val = (range_max + range_min) / 2.0f;
    sprintf(buf, "%.1f", mid_val);
    ILI9341_ShowString(0, GRAPH_Y_START + (GRAPH_HEIGHT / 2) - 8, buf, Font8x16);
}

/**
 * @brief  绘制动态温度曲线
 * @param  temperature: 当前温度值
 * @retval 无
 */
void DHT11_DrawTemperatureCurve(float temperature)
{
    DHT11_DrawGraph(temperature, temp_history, &temp_history_count, 90, 60, RED);
}

/**
 * @brief  绘制动态湿度曲线
 * @param  humidity: 当前湿度值
 * @retval 无
 */
void DHT11_DrawHumidityCurve(float humidity)
{
    DHT11_DrawGraph(humidity, humi_history, &humi_history_count, 160, 60, BLUE);
}

/**
 * @brief  DHT11测试函数
 * @param  无
 * @retval 无
 */
void DHT11_Test(void)
{
    float temperature = 0.0f;
    float humidity = 0.0f;
    ILI9341_Init();
    DHT11_Init();

    HAL_Delay(500);
    ILI9341_Printf(10, 10, Font16x24, "DHT11 TEST");
    while (1)
    {
        DHT11_ReadData(&temperature, &humidity);
        ILI9341_Printf(10, 35, Font16x24, "Temp: %d.%dC   ", (int)temperature, (int)(temperature * 10) % 10);
        ILI9341_Printf(10, 60, Font16x24, "Humi: %d.%d%%  ", (int)humidity, (int)(humidity * 10) % 10);
        DHT11_DrawTemperatureCurve(temperature);
        DHT11_DrawHumidityCurve(humidity);
        //HAL_Delay(10);
    }
}
