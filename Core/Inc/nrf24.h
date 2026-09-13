/*
 * nrf24.h
 *
 * Minimal nRF24L01(+) driver for STM32 HAL - shares SPI2 with the BMP280 and
 * W25Q64 flash on this board.
 *
 * Wiring (from your pin config):
 *   VCC  -> 3.3V   (NOT 5V tolerant - and add a 10-100uF cap across VCC/GND
 *                    right at the module, bare breakouts brown out on TX bursts)
 *   GND  -> GND
 *   CE   -> NRF_CE_Pin   (GPIOB, Pin 7)
 *   CSN  -> NRF_CSN_Pin  (GPIOA, Pin 8)
 *   SCK  -> SPI2_SCK     (PA1)
 *   MOSI -> SPI2_MOSI    (PA12, physically the PA10 pad on this package)
 *   MISO -> SPI2_MISO    (PA11, physically the PA9 pad on this package)
 *   IRQ  -> not connected - this driver polls the STATUS register instead
 *
 * Auto-ack/auto-retransmit are left OFF for the first bring-up (simplest to
 * get a link working); enable EN_AA + SETUP_RETR once basic TX/RX is
 * confirmed if you want reliability over the air.
 *
 * You need TWO nRF24 units talking to get anything to/from each other - set
 * NRF24_DEMO_ROLE_TX to 1 on one board's build and 0 on the other's.
 */

#ifndef NRF24_H
#define NRF24_H

#include "stm32u0xx_hal.h"
#include <stdint.h>

#define NRF24_PAYLOAD_SIZE   32U  /* fixed payload size used by this driver */
#define NRF24_ADDR_WIDTH     5U

typedef enum {
    NRF24_TX_OK = 0,
    NRF24_TX_MAX_RETRIES, /* only reachable if you later enable EN_AA/SETUP_RETR */
    NRF24_TX_TIMEOUT,     /* TX_DS never set - check wiring/power/address match */
} NRF24_TxStatus_t;

/* Debug mirror of the last TX attempt's outcome - same idea as g_dht22_status,
 * drop into a Live Expressions / watch window. */
extern volatile NRF24_TxStatus_t g_nrf24_tx_status;
extern volatile uint32_t         g_nrf24_tx_count;

/* Set by NRF24_Init() (and re-checkable any time via NRF24_SelfTest()): 1 if
 * a register we just wrote reads back correctly over SPI, 0 if the chip
 * never responds sanely (dead/missing chip, MOSI/MISO/SCK/CSN miswired, or
 * no power) - proves the SPI link and chip presence WITHOUT needing a second
 * radio, before you worry about anything going out over the air. */
extern volatile uint8_t g_nrf24_present;

/* Call once after MX_SPI2_Init(). Leaves the radio powered up in Standby-I,
 * and runs NRF24_SelfTest() once, mirroring the result into g_nrf24_present. */
void NRF24_Init(SPI_HandleTypeDef *hspi);

/* Writes a distinctive value to RF_CH and reads it back over SPI. Returns 1
 * if it matches (chip present and SPI wired correctly), 0 otherwise. Safe to
 * call any time - restores your actual channel afterward. */
uint8_t NRF24_SelfTest(void);

/* Set the 5-byte address both ends must agree on for pipe 0 / TX. Call before
 * switching mode if you don't want the {0xE7,0xE7,0xE7,0xE7,0xE7} default. */
void NRF24_SetAddress(const uint8_t addr[NRF24_ADDR_WIDTH]);

void NRF24_SetChannel(uint8_t channel); /* 0-125, must match on both ends */

void NRF24_SetTxMode(void);
void NRF24_SetRxMode(void); /* also raises CE to start listening */

/* Blocking send of up to NRF24_PAYLOAD_SIZE bytes (shorter payloads are
 * zero-padded). Puts the radio in TX mode as a side effect. */
NRF24_TxStatus_t NRF24_Transmit(const uint8_t *data, uint8_t len);

/* Call NRF24_SetRxMode() first. Returns 1 if a payload is waiting. */
uint8_t NRF24_Available(void);

/* Reads exactly NRF24_PAYLOAD_SIZE bytes into data (must be that large). */
void NRF24_Receive(uint8_t *data);

/* Demo: TX side sends an incrementing counter every second; RX side polls
 * and mirrors whatever it gets into g_nrf24_rx_buf/g_nrf24_rx_count.
 * Flip NRF24_DEMO_ROLE_TX at the top of nrf24.c to pick which this board runs. */
extern volatile uint8_t  g_nrf24_rx_buf[NRF24_PAYLOAD_SIZE];
extern volatile uint32_t g_nrf24_rx_count;
void NRF24_Demo(void);

#endif /* NRF24_H */
