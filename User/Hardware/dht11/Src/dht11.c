#include "dht11.h"
#include "bsp_dwt.h"
#include "gpio.h"
#include "stm32f4xx_hal.h"
#include "SEGGER_RTT_Log.h"
#include <string.h>

// ============================================
// 硬件定义
// ============================================
#define DHT11_PORT          GPIOE
#define DHT11_PIN           GPIO_PIN_6

// ============================================
// 内部函数：GPIO 模式切换
// ============================================

static void DHT11_SetOutput(void)
{
    GPIO_InitTypeDef gpio_init = {0};
    gpio_init.Pin = DHT11_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_OD;   // 开漏输出（配合外部上拉）
    gpio_init.Pull = GPIO_NOPULL;           // 不要内部上拉
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT11_PORT, &gpio_init);
}

static void DHT11_SetInput(void)
{
    GPIO_InitTypeDef gpio_init = {0};
    gpio_init.Pin = DHT11_PIN;
    gpio_init.Mode = GPIO_MODE_INPUT;
    gpio_init.Pull = GPIO_PULLUP;           // 内部上拉作为备用
    HAL_GPIO_Init(DHT11_PORT, &gpio_init);
}

// ============================================
// 微秒级延时（使用 bsp_DelayUS）
// ============================================
#define DHT11_DELAY_US(us)  bsp_DelayUS(us)

// ============================================
// 读取一个位（核心时序）
// ============================================

static uint8_t DHT11_ReadBit(void)
{
    uint32_t timeout;
    uint32_t start_time;
    uint32_t high_time_us;

    // 1. 等待低电平开始（DHT11 拉低，约 50us）
    timeout = 1000;  // 最多等 1000us
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET)
    {
        DHT11_DELAY_US(1);
        if (--timeout == 0)
        {
            return 0xFF;  // 超时
        }
    }

    // 2. 等待高电平开始（DHT11 释放总线，约 50us）
    timeout = 1000;
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET)
    {
        DHT11_DELAY_US(1);
        if (--timeout == 0)
        {
            return 0xFF;  // 超时
        }
    }

    // 3. 测量高电平持续时间（决定 bit 是 0 还是 1）
    start_time = DWT->CYCCNT;
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET)
    {
        // 防止死循环：如果高电平超过 200us，直接退出
        if ((DWT->CYCCNT - start_time) > (200 * (SystemCoreClock / 1000000)))
        {
            break;
        }
    }

    // 计算高电平持续时间（微秒）
    high_time_us = (DWT->CYCCNT - start_time) / (SystemCoreClock / 1000000);

    // DHT11: 26~28us = '0', 70us = '1'
    return (high_time_us > 40) ? 1 : 0;
}

// ============================================
// 读取 40 位数据
// ============================================

static uint8_t DHT11_ReadRaw(uint8_t *buffer)
{
    uint8_t bit;
    uint32_t timeout;

    // 1. 发送启动信号
    DHT11_SetOutput();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET);
    DHT11_DELAY_US(18000);  // 拉低至少 18ms

    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
    DHT11_DELAY_US(30);     // 拉高 30us

    // 2. 切换为输入
    DHT11_SetInput();

    // 3. 等待 DHT11 应答（拉低约 80us）
    timeout = 1000;
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET)
    {
        DHT11_DELAY_US(1);
        if (--timeout == 0)
        {
            TEMP_LOG("DHT11: No response (pin stayed HIGH)\r\n");
            return 0;
        }
    }

    // 4. 等待 DHT11 释放总线（拉高约 80us）
    timeout = 1000;
    while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET)
    {
        DHT11_DELAY_US(1);
        if (--timeout == 0)
        {
            TEMP_LOG("DHT11: Stuck LOW\r\n");
            return 0;
        }
    }

    // 5. 读取 40 位数据
    for (uint8_t i = 0; i < 40; i++)
    {
        bit = DHT11_ReadBit();
        if (bit == 0xFF)
        {
            TEMP_LOG("DHT11: Read bit timeout at bit %d\r\n", i);
            return 0;
        }

        buffer[i / 8] <<= 1;
        buffer[i / 8] |= bit;
    }

    return 1;
}

// ============================================
// 对外接口：初始化
// ============================================

void DHT11_Init(void)
{
    DHT11_SetInput();
    TEMP_LOG("DHT11: Driver initialized (GPIO + DWT)\r\n");
}

// ============================================
// 对外接口：读取数据
// ============================================

uint8_t DHT11_Read(DHT11_Data_t *pData)
{
    uint8_t buffer[5] = {0};
    uint8_t result;

    if (pData == NULL)
    {
        return 0;
    }

    // 关中断，保护时序
    __disable_irq();

    result = DHT11_ReadRaw(buffer);

    __enable_irq();

    if (!result)
    {
        pData->is_valid = 0;
        return 0;
    }

    // 校验
    uint8_t sum = buffer[0] + buffer[1] + buffer[2] + buffer[3];
    if (buffer[4] != sum)
    {
        TEMP_LOG("DHT11: Checksum fail (sum=%d, recv=%d)\r\n", sum, buffer[4]);
        pData->is_valid = 0;
        return 0;
    }

    pData->hum_int   = buffer[0];
    pData->hum_dec   = buffer[1];
    pData->temp_int  = buffer[2];
    pData->temp_dec  = buffer[3];
    pData->checksum  = buffer[4];
    pData->is_valid  = 1;

    return 1;
}