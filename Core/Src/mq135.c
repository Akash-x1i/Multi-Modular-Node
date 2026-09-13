/*
 * mq135.c
 *
 * MQ135 air-quality (analog) sensor driver for STM32 HAL.
 */

#include "mq135.h"
#include "main.h"
#include <math.h>

/* ---- Board-specific constants: check/edit these for your actual circuit ---- */
#define MQ135_VREF_V         3.3f     /* ADC reference (VDDA) */
#define MQ135_ADC_MAX        4095.0f  /* 12-bit resolution */
#define MQ135_RL_KOHM        22.0f    /* load resistor on the module - confirm against yours */

/* Rs measured in known-clean air (~400ppm CO2), after a >=24-48h sensor burn-in.
 * The value below is a commonly-cited placeholder, not a measurement of your
 * specific sensor - recalibrate it or ppm_co2 will be meaningless. */
#define MQ135_RO_CLEAN_AIR   76.63f

/* Empirical CO2 curve fit (log-log line from the MQ135 datasheet's Rs/Ro-vs-ppm
 * graph), as commonly used in the Arduino MQ135 library. Approximate only -
 * don't use for anything safety-critical. */
#define MQ135_CO2_CURVE_A    116.6020682f
#define MQ135_CO2_CURVE_B    2.769034857f

#define MQ135_ADC_TIMEOUT_MS HAL_MAX_DELAY /* TEMP: was 10U - waiting forever to see if rank 2 ever completes */

static ADC_HandleTypeDef *mq_adc;

volatile MQ135_Data_t g_mq135_data = {0};

void MQ135_Init(ADC_HandleTypeDef *hadc)
{
    mq_adc = hadc;
}
MQ135_Data_t d = {0};
MQ135_Data_t MQ135_Read(void)
{

    uint32_t conv[3] = {0}; /* rank1=TURBIDITY, rank2=MQ135, rank3=BAT - see MX_ADC1_Init() */
    uint8_t ok = 1;

    HAL_ADC_Start(mq_adc);
    for (int i = 0; i < 3; i++)
    {
        if (HAL_ADC_PollForConversion(mq_adc, MQ135_ADC_TIMEOUT_MS) != HAL_OK)
        {
            ok = 0;
            break;
        }
        conv[i] = HAL_ADC_GetValue(mq_adc);
    }
    HAL_ADC_Stop(mq_adc);

    d.ok  = ok;
    d.raw = (uint16_t)conv[1]; /* MQ135_ADC is regular rank 2 */

    if (ok)
    {
        d.voltage = ((float)d.raw / MQ135_ADC_MAX) * MQ135_VREF_V;

        if (d.voltage > 0.01f)
        {
            d.rs_kohm = (MQ135_VREF_V - d.voltage) * MQ135_RL_KOHM / d.voltage;
            d.ratio   = d.rs_kohm / MQ135_RO_CLEAN_AIR;
            d.ppm_co2 = MQ135_CO2_CURVE_A * powf(d.ratio, -MQ135_CO2_CURVE_B);
        }
    }

    g_mq135_data = d;
    return d;
}

void MQ135_Demo(void)
{
	MQ135_Data_t data;
    while (1)
    {
        data = MQ135_Read(); /* result mirrored into g_mq135_data */
        HAL_Delay(2000);
    }
}
