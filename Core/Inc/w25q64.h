/*
 * w25q64.h
 *
 * Minimal W25Q64 SPI NOR flash driver for STM32 HAL
 * Chip: Winbond W25Q64 (8 MB, 64 Mbit)
 *
 * Wiring assumption (from your pin config):
 *   CS   -> SD_CS_Pin  (GPIOB, Pin 8)   <-- change if you have a dedicated flash CS
 *   SCK/MOSI/MISO -> your SPI peripheral (assumed hspi1 in the .c file)
 */

#ifndef W25Q64_H
#define W25Q64_H

#include "stm32u0xx_hal.h"   /* adjust if your HAL include differs, e.g. stm32u031xx.h */
#include <stdint.h>
#include <stdbool.h>

/* ---- Chip select pin mapping (reusing SD_CS since no dedicated flash CS exists) ---- */
#define W25Q64_CS_Pin        SD_CS_Pin
#define W25Q64_CS_GPIO_Port  SD_CS_GPIO_Port

/* ---- Flash geometry ---- */
#define W25Q64_PAGE_SIZE      256U
#define W25Q64_SECTOR_SIZE    4096U
#define W25Q64_BLOCK_SIZE     65536U
#define W25Q64_TOTAL_SIZE     (8U * 1024U * 1024U)  /* 8 MB */

/* ---- Public API ---- */
void      W25Q64_Init(SPI_HandleTypeDef *hspi);
uint32_t  W25Q64_ReadJEDECID(void);          /* expect 0xEF4017 for W25Q64 */
void      W25Q64_ReadData(uint32_t addr, uint8_t *buf, uint32_t len);
bool      W25Q64_PageProgram(uint32_t addr, const uint8_t *data, uint32_t len); /* len <= 256, must not cross page */
bool      W25Q64_WritePages(uint32_t addr, const uint8_t *data, uint32_t len);  /* handles multi-page writes */
void      W25Q64_SectorErase(uint32_t addr);  /* erases the 4KB sector containing addr */
void      W25Q64_ChipErase(void);
bool      W25Q64_IsBusy(void);
void      W25Q64_WaitUntilReady(void);

#endif /* W25Q64_H */
