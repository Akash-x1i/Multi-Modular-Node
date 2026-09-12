/*
 * dht22.c
 *
 * Minimal DHT22 (AM2302) single-wire driver for STM32 HAL.
 */

#include "dht22.h"
#include "main.h"

#define DHT22_TIMEOUT_US   100U  /* generous margin over the ~20-80us pulses we wait on */

static TIM_HandleTypeDef *dht_tim;

/* Debug mirrors - see dht22.h. Not static so they show up by name in a
 * Live Expressions / watch window without needing this file's scope. */
volatile DHT22_Status_t g_dht22_status = DHT22_ERR_TIMEOUT;
volatile DHT22_Data_t   g_dht22_data   = {0};
volatile uint8_t        g_dht22_raw[5] = {0};

/* ---------------- Microsecond delay via free-running TIM6 ---------------- */
static inline void DHT22_DelayUs(uint16_t us)
{
    __HAL_TIM_SET_COUNTER(dht_tim, 0);
    while (__HAL_TIM_GET_COUNTER(dht_tim) < us) { }
}

/* ---------------- Low level pin control ---------------- */
static void DHT22_SetOutput(void)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = DHT_DATA_Pin;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DHT_DATA_GPIO_Port, &gpio);
}

static void DHT22_SetInput(void)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin  = DHT_DATA_Pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP; /* backup only - use a real 4.7k-10k external pull-up */
    HAL_GPIO_Init(DHT_DATA_GPIO_Port, &gpio);
}

static inline void DHT22_Write(GPIO_PinState state)
{
    HAL_GPIO_WritePin(DHT_DATA_GPIO_Port, DHT_DATA_Pin, state);
}

static inline GPIO_PinState DHT22_ReadPin(void)
{
    return HAL_GPIO_ReadPin(DHT_DATA_GPIO_Port, DHT_DATA_Pin);
}

/* Waits for the line to reach `level`. Returns elapsed microseconds, or -1 on timeout. */
static int32_t DHT22_WaitForLevel(GPIO_PinState level, uint16_t timeout_us)
{
    __HAL_TIM_SET_COUNTER(dht_tim, 0);
    while (DHT22_ReadPin() != level)
    {
        if (__HAL_TIM_GET_COUNTER(dht_tim) > timeout_us)
            return -1;
    }
    return (int32_t)__HAL_TIM_GET_COUNTER(dht_tim);
}

/* ---------------- Public API ---------------- */
void DHT22_Init(TIM_HandleTypeDef *htim)
{
    dht_tim = htim;
    DHT22_SetOutput();
    DHT22_Write(GPIO_PIN_SET); /* idle high */
}

DHT22_Status_t DHT22_Read(DHT22_Data_t *out)
{
    uint8_t data[5] = {0};
    DHT22_Status_t status;

    /* --- Start signal: MCU pulls the line low for >=1ms (18ms per datasheet) --- */
    DHT22_SetOutput();
    DHT22_Write(GPIO_PIN_RESET);
    HAL_Delay(18);
    DHT22_Write(GPIO_PIN_SET);
    DHT22_DelayUs(30);
    DHT22_SetInput();

    /* --- Sensor response: 80us low, then 80us high --- */
    if (DHT22_WaitForLevel(GPIO_PIN_RESET, DHT22_TIMEOUT_US) < 0) { status = DHT22_ERR_TIMEOUT; goto done; }
    if (DHT22_WaitForLevel(GPIO_PIN_SET,   DHT22_TIMEOUT_US) < 0) { status = DHT22_ERR_TIMEOUT; goto done; }
    if (DHT22_WaitForLevel(GPIO_PIN_RESET, DHT22_TIMEOUT_US) < 0) { status = DHT22_ERR_TIMEOUT; goto done; }

    /* --- 40 data bits: each starts with a ~50us low, then a high pulse whose
     *     length encodes the bit - ~26-28us for 0, ~70us for 1 --- */
    for (int i = 0; i < 40; i++)
    {
        if (DHT22_WaitForLevel(GPIO_PIN_SET, DHT22_TIMEOUT_US) < 0)
        {
            status = DHT22_ERR_TIMEOUT;
            goto done;
        }

        int32_t high_us = DHT22_WaitForLevel(GPIO_PIN_RESET, DHT22_TIMEOUT_US);
        if (high_us < 0)
        {
            status = DHT22_ERR_TIMEOUT;
            goto done;
        }

        data[i / 8] <<= 1;
        if (high_us > 40)
            data[i / 8] |= 1;
    }

    DHT22_SetOutput();
    DHT22_Write(GPIO_PIN_SET); /* release the bus, idle high */

    {
        uint8_t checksum = (uint8_t)(data[0] + data[1] + data[2] + data[3]);
        if (checksum != data[4])
        {
            status = DHT22_ERR_CHECKSUM;
            goto done;
        }
    }

    out->humidity = ((data[0] << 8) | data[1]) / 10.0f;

    {
        int16_t temp_raw = (int16_t)(((data[2] & 0x7F) << 8) | data[3]);
        out->temperature = temp_raw / 10.0f;
        if (data[2] & 0x80) /* sign bit: negative temperature */
            out->temperature = -out->temperature;
    }

    status = DHT22_OK;

done:
    /* Mirror whatever we got - even a partial/failed transaction - so the
     * debugger can tell a timed-out sensor from a garbled read. */
    for (int i = 0; i < 5; i++)
        g_dht22_raw[i] = data[i];
    g_dht22_status = status;
    if (status == DHT22_OK)
        g_dht22_data = *out;

    return status;
}

void DHT22_Demo(void)
{
    DHT22_Data_t reading = {0};

    while (1)
    {
        DHT22_Read(&reading); /* result also mirrored into g_dht22_status/g_dht22_data/g_dht22_raw */

        HAL_Delay(2000); /* DHT22 sampling rate is capped at ~0.5 Hz */
    }
}
