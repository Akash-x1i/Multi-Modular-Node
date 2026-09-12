/*
 * dht22.h
 *
 * Minimal DHT22 (AM2302) single-wire driver for STM32 HAL.
 *
 * Wiring (from your pin config):
 *   DATA -> DHT_DATA_Pin  (GPIOC, Pin 15)
 *   Needs a 4.7k-10k pull-up to VCC on the DATA line. Most 3-pin DHT22
 *   breakout modules already have this resistor on-board; a bare DHT22
 *   sensor does not, so add one externally if you're using the bare part.
 *
 * Timing is done with a free-running 1 MHz timer (see MX_TIM6_Init() /
 * htim6 in main.c) since this MCU (Cortex-M0+) has no DWT cycle counter
 * to bit-bang against.
 */

#ifndef DHT22_H
#define DHT22_H

#include "stm32u0xx_hal.h"
#include <stdint.h>

typedef struct {
    float temperature; /* deg C */
    float humidity;    /* %RH   */
} DHT22_Data_t;

typedef enum {
    DHT22_OK = 0,
    DHT22_ERR_TIMEOUT,   /* sensor didn't respond / bit pulse never showed up */
    DHT22_ERR_CHECKSUM,  /* 40 bits received but checksum byte didn't match */
} DHT22_Status_t;

/* Debug mirrors, updated on every DHT22_Read() call (success or failure) so you
 * can drop them into a Live Expressions / watch window without needing to catch
 * a breakpoint mid-call:
 *   g_dht22_status - result of the most recent read
 *   g_dht22_data   - last successfully decoded temperature/humidity
 *   g_dht22_raw    - last 5 raw bytes shifted in before a timeout/checksum fail
 *                    cut the transaction short (humidity_hi, humidity_lo,
 *                    temp_hi, temp_lo, checksum); bytes not yet reached stay 0 */
extern volatile DHT22_Status_t g_dht22_status;
extern volatile DHT22_Data_t   g_dht22_data;
extern volatile uint8_t        g_dht22_raw[5];

/* Call once after MX_TIM6_Init(); htim must already be started (HAL_TIM_Base_Start). */
void DHT22_Init(TIM_HandleTypeDef *htim);

/* Blocking read of one temperature+humidity sample (~5-6 ms typical, up to ~25 ms
 * worst case including the 18 ms start pulse). DHT22 needs >=2 s between reads. */
DHT22_Status_t DHT22_Read(DHT22_Data_t *out);

/* Simple demo loop: reads every 2 s forever, mirroring each result into
 * g_dht22_status/g_dht22_data/g_dht22_raw above. */
void DHT22_Demo(void);

#endif /* DHT22_H */
