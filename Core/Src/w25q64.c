/*
 * w25q64.c
 *
 * Minimal W25Q64 SPI NOR flash driver for STM32 HAL
 */

#include "w25q64.h"
#include <string.h>

/* --- Adjust this if your flash is on a different SPI peripheral --- */
static SPI_HandleTypeDef *w25q64_spi;

/* --- W25Q64 command set --- */
#define CMD_WRITE_ENABLE      0x06
#define CMD_WRITE_DISABLE     0x04
#define CMD_READ_STATUS1      0x05
#define CMD_PAGE_PROGRAM      0x02
#define CMD_SECTOR_ERASE_4K   0x20
#define CMD_CHIP_ERASE        0xC7
#define CMD_READ_DATA         0x03
#define CMD_JEDEC_ID          0x9F

#define STATUS_BUSY_BIT       0x01


/* --- dummy sensor struct --- */
typedef struct {
    uint32_t timestamp;
    float temperature;
    float humidity;
    float distance_cm;
    uint16_t turbidity_raw;
    uint16_t air_quality_raw;
    float battery_voltage;
} SensorData_t;



/* ---------------- Low level CS control ---------------- */
static inline void W25Q64_CS_Low(void)
{
    HAL_GPIO_WritePin(W25Q64_CS_GPIO_Port, W25Q64_CS_Pin, GPIO_PIN_RESET);
}

static inline void W25Q64_CS_High(void)
{
    HAL_GPIO_WritePin(W25Q64_CS_GPIO_Port, W25Q64_CS_Pin, GPIO_PIN_SET);
}

static inline void W25Q64_SPI_TxRx(uint8_t *tx, uint8_t *rx, uint16_t len)
{
    HAL_SPI_TransmitReceive(w25q64_spi, tx, rx, len, HAL_MAX_DELAY);
}

static inline void W25Q64_SPI_Tx(uint8_t *tx, uint16_t len)
{
    HAL_SPI_Transmit(w25q64_spi, tx, len, HAL_MAX_DELAY);
}

static inline void W25Q64_SPI_Rx(uint8_t *rx, uint16_t len)
{
    uint8_t dummy = 0xFF;
    for (uint16_t i = 0; i < len; i++) {
        HAL_SPI_TransmitReceive(w25q64_spi, &dummy, &rx[i], 1, HAL_MAX_DELAY);
    }
}

/* ---------------- Address framing helper ---------------- */
static void W25Q64_SendAddr(uint8_t cmd, uint32_t addr)
{
    uint8_t frame[4];
    frame[0] = cmd;
    frame[1] = (addr >> 16) & 0xFF;
    frame[2] = (addr >> 8) & 0xFF;
    frame[3] = addr & 0xFF;
    W25Q64_SPI_Tx(frame, 4);
}

/* ---------------- Public API ---------------- */

void W25Q64_Init(SPI_HandleTypeDef *hspi)
{
    w25q64_spi = hspi;
    W25Q64_CS_High();  /* deselect by default */
    HAL_Delay(10);     /* allow flash power-up time */
}

uint32_t W25Q64_ReadJEDECID(void)
{
    uint8_t cmd = CMD_JEDEC_ID;
    uint8_t id[3] = {0};

    W25Q64_CS_Low();
    W25Q64_SPI_Tx(&cmd, 1);
    W25Q64_SPI_Rx(id, 3);
    W25Q64_CS_High();

    return ((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | id[2];
}

bool W25Q64_IsBusy(void)
{
    uint8_t cmd = CMD_READ_STATUS1;
    uint8_t status = 0;

    W25Q64_CS_Low();
    W25Q64_SPI_Tx(&cmd, 1);
    W25Q64_SPI_Rx(&status, 1);
    W25Q64_CS_High();

    return (status & STATUS_BUSY_BIT) != 0;
}

void W25Q64_WaitUntilReady(void)
{
    while (W25Q64_IsBusy()) {
        HAL_Delay(1);
    }
}

static void W25Q64_WriteEnable(void)
{
    uint8_t cmd = CMD_WRITE_ENABLE;
    W25Q64_CS_Low();
    W25Q64_SPI_Tx(&cmd, 1);
    W25Q64_CS_High();
}

void W25Q64_ReadData(uint32_t addr, uint8_t *buf, uint32_t len)
{
    W25Q64_WaitUntilReady();

    W25Q64_CS_Low();
    W25Q64_SendAddr(CMD_READ_DATA, addr);
    W25Q64_SPI_Rx(buf, len);
    W25Q64_CS_High();
}

bool W25Q64_PageProgram(uint32_t addr, const uint8_t *data, uint32_t len)
{
    if (len == 0 || len > W25Q64_PAGE_SIZE) {
        return false;
    }
    /* must not cross a page boundary */
    if ((addr % W25Q64_PAGE_SIZE) + len > W25Q64_PAGE_SIZE) {
        return false;
    }

    W25Q64_WaitUntilReady();
    W25Q64_WriteEnable();

    W25Q64_CS_Low();
    W25Q64_SendAddr(CMD_PAGE_PROGRAM, addr);
    W25Q64_SPI_Tx((uint8_t *)data, (uint16_t)len);
    W25Q64_CS_High();

    W25Q64_WaitUntilReady();
    return true;
}

bool W25Q64_WritePages(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint32_t remaining = len;
    uint32_t offset = 0;

    while (remaining > 0) {
        uint32_t page_offset = (addr + offset) % W25Q64_PAGE_SIZE;
        uint32_t chunk = W25Q64_PAGE_SIZE - page_offset;
        if (chunk > remaining) {
            chunk = remaining;
        }

        if (!W25Q64_PageProgram(addr + offset, data + offset, chunk)) {
            return false;
        }

        offset += chunk;
        remaining -= chunk;
    }
    return true;
}

void W25Q64_SectorErase(uint32_t addr)
{
    W25Q64_WaitUntilReady();
    W25Q64_WriteEnable();

    W25Q64_CS_Low();
    W25Q64_SendAddr(CMD_SECTOR_ERASE_4K, addr);
    W25Q64_CS_High();

    W25Q64_WaitUntilReady();
}

void W25Q64_ChipErase(void)
{
    uint8_t cmd = CMD_CHIP_ERASE;

    W25Q64_WaitUntilReady();
    W25Q64_WriteEnable();

    W25Q64_CS_Low();
    W25Q64_SPI_Tx(&cmd, 1);
    W25Q64_CS_High();

    W25Q64_WaitUntilReady();
}

void W25Q64_demo(void){


	  uint32_t jedec_id = W25Q64_ReadJEDECID();   /* expect 0xEF4017 */

	  /* build dummy sensor data */
	  SensorData_t sample = {
	      .timestamp = HAL_GetTick(),
	      .temperature = 23.5f,
	      .humidity = 55.2f,
	      .distance_cm = 120.4f,
	      .turbidity_raw = 812,
	      .air_quality_raw = 340,
	      .battery_voltage = 3.98f
	  };

	  uint32_t flash_addr = 0x000000;  /* first sector */

	  /* erase before writing (NOR flash requires erase before write) */
	  W25Q64_SectorErase(flash_addr);

	  /* write struct as raw bytes */
	  W25Q64_WritePages(flash_addr, (uint8_t *)&sample, sizeof(sample));

	  /* read it back */
	  SensorData_t readback = {0};
	  W25Q64_ReadData(flash_addr, (uint8_t *)&readback, sizeof(readback));
}
