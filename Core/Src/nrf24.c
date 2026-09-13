/*
 * nrf24.c
 *
 * Minimal nRF24L01(+) driver for STM32 HAL.
 */

#include "nrf24.h"
#include "main.h"
#include <string.h>

/* Set to 1 to build this board as the transmitter, 0 for the receiver -
 * see NRF24_Demo() at the bottom. */
#define NRF24_DEMO_ROLE_TX   1

/* ---- SPI commands ---- */
#define NRF24_CMD_R_REGISTER    0x00U /* OR with 5-bit register address */
#define NRF24_CMD_W_REGISTER    0x20U /* OR with 5-bit register address */
#define NRF24_CMD_R_RX_PAYLOAD  0x61U
#define NRF24_CMD_W_TX_PAYLOAD  0xA0U
#define NRF24_CMD_FLUSH_TX      0xE1U
#define NRF24_CMD_FLUSH_RX      0xE2U
#define NRF24_CMD_NOP           0xFFU

/* ---- Registers ---- */
#define NRF24_REG_CONFIG        0x00U
#define NRF24_REG_EN_AA         0x01U
#define NRF24_REG_EN_RXADDR     0x02U
#define NRF24_REG_SETUP_AW      0x03U
#define NRF24_REG_SETUP_RETR    0x04U
#define NRF24_REG_RF_CH         0x05U
#define NRF24_REG_RF_SETUP      0x06U
#define NRF24_REG_STATUS        0x07U
#define NRF24_REG_RX_ADDR_P0    0x0AU
#define NRF24_REG_TX_ADDR       0x10U
#define NRF24_REG_RX_PW_P0      0x11U

#define NRF24_CONFIG_EN_CRC      (1U << 3)
#define NRF24_CONFIG_CRCO_2BYTE  (1U << 2)
#define NRF24_CONFIG_PWR_UP      (1U << 1)
#define NRF24_CONFIG_PRIM_RX     (1U << 0)

#define NRF24_STATUS_RX_DR       (1U << 6)
#define NRF24_STATUS_TX_DS       (1U << 5)
#define NRF24_STATUS_MAX_RT      (1U << 4)

#define NRF24_SPI_TIMEOUT_MS     100U
#define NRF24_TX_WAIT_TIMEOUT_MS 100U

static SPI_HandleTypeDef *nrf_spi;

/* Classic tutorial-default address - fine as a starting point, override with
 * NRF24_SetAddress() once you have more than one pair of nodes on air. */
static uint8_t nrf_addr[NRF24_ADDR_WIDTH] = {0xE7, 0xE7, 0xE7, 0xE7, 0xE7};

volatile NRF24_TxStatus_t g_nrf24_tx_status = NRF24_TX_TIMEOUT;
volatile uint32_t         g_nrf24_tx_count  = 0;
volatile uint8_t          g_nrf24_rx_buf[NRF24_PAYLOAD_SIZE] = {0};
volatile uint32_t         g_nrf24_rx_count  = 0;
volatile uint8_t          g_nrf24_present   = 0;

static uint8_t nrf_channel = 76;

/* ---------------- Low level pin control ---------------- */
static inline void NRF24_CsnLow(void)  { HAL_GPIO_WritePin(NRF_CSN_GPIO_Port, NRF_CSN_Pin, GPIO_PIN_RESET); }
static inline void NRF24_CsnHigh(void) { HAL_GPIO_WritePin(NRF_CSN_GPIO_Port, NRF_CSN_Pin, GPIO_PIN_SET); }
static inline void NRF24_CeLow(void)   { HAL_GPIO_WritePin(NRF_CE_GPIO_Port, NRF_CE_Pin, GPIO_PIN_RESET); }
static inline void NRF24_CeHigh(void)  { HAL_GPIO_WritePin(NRF_CE_GPIO_Port, NRF_CE_Pin, GPIO_PIN_SET); }

/* ---------------- Low level SPI ---------------- */
static uint8_t NRF24_ReadReg(uint8_t reg)
{
    uint8_t tx[2] = { (uint8_t)(NRF24_CMD_R_REGISTER | (reg & 0x1FU)), NRF24_CMD_NOP };
    uint8_t rx[2] = {0};

    NRF24_CsnLow();
    HAL_SPI_TransmitReceive(nrf_spi, tx, rx, 2, NRF24_SPI_TIMEOUT_MS);
    NRF24_CsnHigh();

    return rx[1];
}

static void NRF24_WriteReg(uint8_t reg, uint8_t value)
{
    uint8_t tx[2] = { (uint8_t)(NRF24_CMD_W_REGISTER | (reg & 0x1FU)), value };

    NRF24_CsnLow();
    HAL_SPI_Transmit(nrf_spi, tx, 2, NRF24_SPI_TIMEOUT_MS);
    NRF24_CsnHigh();
}

static void NRF24_WriteRegMulti(uint8_t reg, const uint8_t *data, uint8_t len)
{
    uint8_t cmd = (uint8_t)(NRF24_CMD_W_REGISTER | (reg & 0x1FU));

    NRF24_CsnLow();
    HAL_SPI_Transmit(nrf_spi, &cmd, 1, NRF24_SPI_TIMEOUT_MS);
    HAL_SPI_Transmit(nrf_spi, (uint8_t *)data, len, NRF24_SPI_TIMEOUT_MS);
    NRF24_CsnHigh();
}

static uint8_t NRF24_GetStatus(void)
{
    uint8_t cmd = NRF24_CMD_NOP;
    uint8_t status = 0;

    NRF24_CsnLow();
    HAL_SPI_TransmitReceive(nrf_spi, &cmd, &status, 1, NRF24_SPI_TIMEOUT_MS);
    NRF24_CsnHigh();

    return status;
}

static void NRF24_FlushTx(void)
{
    uint8_t cmd = NRF24_CMD_FLUSH_TX;
    NRF24_CsnLow();
    HAL_SPI_Transmit(nrf_spi, &cmd, 1, NRF24_SPI_TIMEOUT_MS);
    NRF24_CsnHigh();
}

static void NRF24_FlushRx(void)
{
    uint8_t cmd = NRF24_CMD_FLUSH_RX;
    NRF24_CsnLow();
    HAL_SPI_Transmit(nrf_spi, &cmd, 1, NRF24_SPI_TIMEOUT_MS);
    NRF24_CsnHigh();
}

static void NRF24_ApplyAddress(void)
{
    NRF24_WriteRegMulti(NRF24_REG_RX_ADDR_P0, nrf_addr, NRF24_ADDR_WIDTH);
    NRF24_WriteRegMulti(NRF24_REG_TX_ADDR,    nrf_addr, NRF24_ADDR_WIDTH);
}

/* ---------------- Public API ---------------- */
void NRF24_Init(SPI_HandleTypeDef *hspi)
{
    nrf_spi = hspi;

    NRF24_CeLow();
    NRF24_CsnHigh();
    HAL_Delay(100); /* datasheet: >=100ms from power-on before first SPI command */

    /* Powered down, CRC on (2 bytes), PTX by default */
    NRF24_WriteReg(NRF24_REG_CONFIG, NRF24_CONFIG_EN_CRC | NRF24_CONFIG_CRCO_2BYTE);
    NRF24_WriteReg(NRF24_REG_EN_AA, 0x00);      /* auto-ack off for first bring-up */
    NRF24_WriteReg(NRF24_REG_EN_RXADDR, 0x01);  /* pipe 0 enabled */
    NRF24_WriteReg(NRF24_REG_SETUP_AW, 0x03);   /* 5-byte addresses */
    NRF24_WriteReg(NRF24_REG_SETUP_RETR, 0x00); /* no auto retransmit (matches EN_AA=0) */
    NRF24_WriteReg(NRF24_REG_RF_CH, nrf_channel); /* channel 76 (2.476GHz) - must match both ends */
    NRF24_WriteReg(NRF24_REG_RF_SETUP, 0x06);   /* 1Mbps, 0dBm */
    NRF24_WriteReg(NRF24_REG_RX_PW_P0, NRF24_PAYLOAD_SIZE);

    NRF24_ApplyAddress();

    NRF24_FlushTx();
    NRF24_FlushRx();
    NRF24_WriteReg(NRF24_REG_STATUS, NRF24_STATUS_RX_DR | NRF24_STATUS_TX_DS | NRF24_STATUS_MAX_RT);

    /* Power up into Standby-I */
    NRF24_WriteReg(NRF24_REG_CONFIG, NRF24_CONFIG_EN_CRC | NRF24_CONFIG_CRCO_2BYTE | NRF24_CONFIG_PWR_UP);
    HAL_Delay(2); /* datasheet: power-down -> standby settle, ~1.5ms */

    g_nrf24_present = NRF24_SelfTest();
}

uint8_t NRF24_SelfTest(void)
{
    /* 0x55 (0b01010101) is a distinctive bit pattern - unlikely to show up by
     * coincidence if MISO is floating (usually reads all-0 or all-1) or if
     * nothing is actually on the other end of the SPI bus. */
    const uint8_t test_value = 0x55;
    uint8_t readback;
    uint8_t ok;

    NRF24_WriteReg(NRF24_REG_RF_CH, test_value);
    readback = NRF24_ReadReg(NRF24_REG_RF_CH);
    ok = (readback == test_value) ? 1U : 0U;

    NRF24_WriteReg(NRF24_REG_RF_CH, nrf_channel); /* restore the real channel */

    return ok;
}

void NRF24_SetAddress(const uint8_t addr[NRF24_ADDR_WIDTH])
{
    memcpy(nrf_addr, addr, NRF24_ADDR_WIDTH);
    NRF24_ApplyAddress();
}

void NRF24_SetChannel(uint8_t channel)
{
    nrf_channel = channel;
    NRF24_WriteReg(NRF24_REG_RF_CH, channel);
}

void NRF24_SetTxMode(void)
{
    uint8_t cfg = NRF24_ReadReg(NRF24_REG_CONFIG);
    cfg &= (uint8_t)~NRF24_CONFIG_PRIM_RX;
    NRF24_WriteReg(NRF24_REG_CONFIG, cfg);
    NRF24_CeLow();
}

void NRF24_SetRxMode(void)
{
    uint8_t cfg = NRF24_ReadReg(NRF24_REG_CONFIG);
    cfg |= NRF24_CONFIG_PRIM_RX;
    NRF24_WriteReg(NRF24_REG_CONFIG, cfg);
    NRF24_CeHigh(); /* start listening */
    HAL_Delay(1);
}

NRF24_TxStatus_t NRF24_Transmit(const uint8_t *data, uint8_t len)
{
    uint8_t payload[NRF24_PAYLOAD_SIZE] = {0};
    uint8_t n = (len > NRF24_PAYLOAD_SIZE) ? NRF24_PAYLOAD_SIZE : len;
    uint8_t cmd = NRF24_CMD_W_TX_PAYLOAD;
    uint8_t status;
    uint32_t start;
    NRF24_TxStatus_t result;

    memcpy(payload, data, n);

    NRF24_SetTxMode();
    NRF24_FlushTx();

    NRF24_CsnLow();
    HAL_SPI_Transmit(nrf_spi, &cmd, 1, NRF24_SPI_TIMEOUT_MS);
    HAL_SPI_Transmit(nrf_spi, payload, NRF24_PAYLOAD_SIZE, NRF24_SPI_TIMEOUT_MS);
    NRF24_CsnHigh();

    /* Pulse CE >=10us to kick off the transmission - 1ms is comfortably more
     * than enough and keeps this driver DHT22-timer-free. */
    NRF24_CeHigh();
    HAL_Delay(1);
    NRF24_CeLow();

    start = HAL_GetTick();
    do
    {
        status = NRF24_GetStatus();
        if (status & (NRF24_STATUS_TX_DS | NRF24_STATUS_MAX_RT))
            break;
    } while ((HAL_GetTick() - start) < NRF24_TX_WAIT_TIMEOUT_MS);

    NRF24_WriteReg(NRF24_REG_STATUS, NRF24_STATUS_TX_DS | NRF24_STATUS_MAX_RT);

    if (status & NRF24_STATUS_TX_DS)
        result = NRF24_TX_OK;
    else if (status & NRF24_STATUS_MAX_RT)
        result = NRF24_TX_MAX_RETRIES; /* only reachable once you enable EN_AA/SETUP_RETR */
    else
        result = NRF24_TX_TIMEOUT; /* check wiring/power/address/channel match on both ends */

    g_nrf24_tx_status = result;
    return result;
}

uint8_t NRF24_Available(void)
{
    return (NRF24_GetStatus() & NRF24_STATUS_RX_DR) ? 1U : 0U;
}

void NRF24_Receive(uint8_t *data)
{
    uint8_t cmd = NRF24_CMD_R_RX_PAYLOAD;

    NRF24_CsnLow();
    HAL_SPI_Transmit(nrf_spi, &cmd, 1, NRF24_SPI_TIMEOUT_MS);
    HAL_SPI_Receive(nrf_spi, data, NRF24_PAYLOAD_SIZE, NRF24_SPI_TIMEOUT_MS);
    NRF24_CsnHigh();

    NRF24_WriteReg(NRF24_REG_STATUS, NRF24_STATUS_RX_DR);
}

void NRF24_Demo(void)
{
#if NRF24_DEMO_ROLE_TX
    uint32_t counter = 0;

    while (1)
    {
        uint8_t payload[NRF24_PAYLOAD_SIZE] = {0};
        memcpy(payload, &counter, sizeof(counter));

        NRF24_Transmit(payload, sizeof(payload)); /* result mirrored into g_nrf24_tx_status */
        g_nrf24_tx_count = counter;
        counter++;

        HAL_Delay(1000);
    }
#else
    NRF24_SetRxMode();

    while (1)
    {
        if (NRF24_Available())
        {
            uint8_t payload[NRF24_PAYLOAD_SIZE];
            NRF24_Receive(payload);
            memcpy((void *)g_nrf24_rx_buf, payload, NRF24_PAYLOAD_SIZE);
            g_nrf24_rx_count++;
        }
    }
#endif
}
