/*
 * mq135.h
 *
 * MQ135 air-quality (analog) sensor driver for STM32 HAL.
 *
 * Wiring (from your pin config):
 *   AO -> MQ135_ADC_Pin (GPIOA, Pin 3 / ADC1_IN7)
 *
 * hadc1 is shared with two other analog sensors (TURBIDITY_ADC on rank 1,
 * BAT_ADC on rank 3 - see MX_ADC1_Init() in main.c). MQ135_Read() runs the
 * whole 3-conversion scan each time and keeps only the MQ135 result (rank 2),
 * since the ADC can't be triggered for a single channel out of that sequence.
 *
 * IMPORTANT - check before wiring AO to PA3:
 *   Most MQ135 breakout modules run their sensing circuit off 5V and the AO
 *   output can swing close to that 5V rail. This MCU's ADC input is only
 *   rated to VDDA (3.3V) + ~0.3V. Feeding it 5V directly can damage the pin.
 *   Either power the module from 3.3V (check its comparator/AO circuit
 *   tolerates that - readings will be scaled down accordingly), or add a
 *   resistor divider between AO and PA3 sized for your module's actual output
 *   range. Don't wire AO straight to PA3 from a 5V-powered module.
 *
 * The ppm estimate below uses the widely-cited MQ135 Rs/Ro curve fit (as used
 * in the common Arduino MQ135 library) and is only as good as MQ135_RO_CLEAN_AIR
 * in mq135.c, which MUST be calibrated for your physical sensor (see comment
 * there). Treat ppm_co2 as a rough trend indicator, not a calibrated
 * measurement, until you've done that.
 */

#ifndef MQ135_H
#define MQ135_H

#include "stm32u0xx_hal.h"
#include <stdint.h>

typedef struct {
    uint8_t  ok;       /* 1 if the ADC scan completed, 0 if a conversion timed out */
    uint16_t raw;      /* 12-bit ADC code for the MQ135 channel, 0-4095 */
    float    voltage;  /* volts at PA3 */
    float    rs_kohm;  /* sensor resistance, derived from voltage + MQ135_RL_KOHM */
    float    ratio;    /* Rs/Ro - rises as air quality worsens; the real indicator to log */
    float    ppm_co2;  /* rough CO2-equivalent estimate, see calibration note above */
} MQ135_Data_t;

/* Debug mirror updated on every MQ135_Read() call - drop into a Live
 * Expressions / watch window the same way as the DHT22 driver's g_dht22_*. */
extern volatile MQ135_Data_t g_mq135_data;

/* Call once after MX_ADC1_Init(); does not start the ADC itself. */
void MQ135_Init(ADC_HandleTypeDef *hadc);

/* Blocking read - runs the shared 3-channel ADC scan and returns the MQ135 result. */
MQ135_Data_t MQ135_Read(void);

/* Simple demo loop: reads every 2s forever, mirroring into g_mq135_data. */
void MQ135_Demo(void);

#endif /* MQ135_H */
