/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32u0xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

extern ADC_HandleTypeDef hadc1;
extern SPI_HandleTypeDef hspi2;
extern UART_HandleTypeDef huart3;

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define SD_CS_Pin GPIO_PIN_8
#define SD_CS_GPIO_Port GPIOB
#define HC_ECHO_Pin GPIO_PIN_14
#define HC_ECHO_GPIO_Port GPIOC
#define DHT_DATA_Pin GPIO_PIN_15
#define DHT_DATA_GPIO_Port GPIOC
#define TURBIDITY_ADC_Pin GPIO_PIN_0
#define TURBIDITY_ADC_GPIO_Port GPIOA
#define MQ135_ADC_Pin GPIO_PIN_3
#define MQ135_ADC_GPIO_Port GPIOA
#define BAT_ADC_Pin GPIO_PIN_5
#define BAT_ADC_GPIO_Port GPIOA
#define BMP_CS_Pin GPIO_PIN_1
#define BMP_CS_GPIO_Port GPIOB
#define NRF_CSN_Pin GPIO_PIN_8
#define NRF_CSN_GPIO_Port GPIOA
#define NRF_CE_Pin GPIO_PIN_7
#define NRF_CE_GPIO_Port GPIOB
#define HC_TRIG_Pin GPIO_PIN_3
#define HC_TRIG_GPIO_Port GPIOF

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
