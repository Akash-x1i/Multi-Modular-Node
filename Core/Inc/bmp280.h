/*
 * bmp280.h
 *
 * BMP280 pressure/temperature driver for STM32 HAL, over SPI2 (CS = BMP_CS,
 * PB1). Same shape as the other drivers in this project: Init(), a blocking
 * Read(), and a volatile debug mirror.
 */

#ifndef BMP280_H
#define BMP280_H

#include "stm32u0xx_hal.h"
#include <stdint.h>

typedef struct {
    float temperature_c;   /* deg C */
    float pressure_hpa;    /* hPa */
} BMP280_Data_t;

typedef enum {
    BMP280_OK = 0,
    BMP280_ERR_CHIP_ID, /* chip ID didn't read back as 0x58 - check wiring */
} BMP280_Status_t;

/* Debug mirrors updated on every BMP280_Read() call, same convention as
 * g_dht22_status/g_mq135_data - drop into a Live Expressions view. */
extern volatile BMP280_Status_t g_bmp280_status;
extern volatile BMP280_Data_t   g_bmp280_data;
extern volatile uint8_t         g_bmp280_chip_id;

/* Call once after MX_SPI2_Init(). Reads chip ID + calibration data and
 * configures the sensor for normal mode (continuous sampling internally;
 * this driver still reads on demand via BMP280_Read()). */
void BMP280_Init(void);

/* Blocking read of one temperature+pressure sample. */
BMP280_Status_t BMP280_Read(BMP280_Data_t *out);

/* Standalone demo: logs over USART3 forever. Needs MX_USART3_UART_Init()
 * and huart3 - NOT wired up in this build (see README), kept only for
 * reference/standalone bring-up on a board where UART is enabled. */
void BMP280_RunDemo(void);

#endif /* BMP280_H */
