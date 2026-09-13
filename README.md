# Multi Modular Node

STM32U031F8P6-based sensor/radio node. This README documents the pinout, how
each sensor/module driver works, which demo functions exist, and what's
currently active in `main.c` vs. wired-but-disabled.

- MCU: STM32U031F8P6 (Cortex-M0+, 20-pin TSSOP package)
- Toolchain: STM32CubeIDE
- Peripherals in use: ADC1 (3-channel scan), SPI2 (shared bus), TIM6 (µs
  timebase for bit-banging), GPIO

> **Package note:** this MCU's TSSOP20 package doesn't break out PA9/PA10 as
> separate physical pins - they're remapped onto the PA11/PA12 pads instead
> (CubeMX shows this as `PA11 [PA9]` / `PA12 [PA10]`). Wherever this doc says
> PA11/PA12, wire to the pins physically silkscreened that way on your board.

## Pinout

![STM32U031F8Px pinout (CubeMX pinout view, TSSOP20)](Doc/STM%20U031%20Pin%20Config.png)

| Signal | MCU Pin | Port | Mode | Used by |
|---|---|---|---|---|
| `SD_CS` | PB8 | GPIOB | Output | Reused as W25Q64 flash CS (see [W25Q64](#w25q64-spi-nor-flash) below) |
| `HC_ECHO` | PC14 | GPIOC | Input | Reserved for an HC-SR04 ultrasonic sensor (no driver yet) |
| `DHT_DATA` | PC15 | GPIOC | Output (switched to input during reads) | [DHT22](#dht22-temperaturehumidity) |
| `TURBIDITY_ADC` | PA0 | GPIOA | Analog (ADC1_IN4) | Turbidity sensor (no driver yet - see [ADC bus](#adc1---shared-3-channel-scan)) |
| `MQ135_ADC` | PA3 | GPIOA | Analog (ADC1_IN7) | [MQ135](#mq135-air-quality) - **currently disconnected** |
| `BAT_ADC` | PA5 | GPIOA | Analog (ADC1_IN9) | Battery voltage sense (no driver yet) |
| `BMP_CS` | PB1 | GPIOB | Output | [BMP280](#bmp280-pressuretemperature) |
| `NRF_CSN` | PA8 | GPIOA | Output | [NRF24L01](#nrf24l01-radio) |
| `NRF_CE` | PB7 | GPIOB | Output | [NRF24L01](#nrf24l01-radio) |
| `HC_TRIG` | PF3 | GPIOF | Output | Reserved for the same HC-SR04 (no driver yet) |
| `SPI2_SCK` | PA1 | GPIOA | AF (SPI2) | Shared bus - BMP280, W25Q64, NRF24L01 |
| `SPI2_MISO` | PA11 *(PA9 pad)* | GPIOA | AF (SPI2) | Shared bus |
| `SPI2_MOSI` | PA12 *(PA10 pad)* | GPIOA | AF (SPI2) | Shared bus |
| — | — | TIM6 (internal, no pin) | 1MHz free-running counter | Microsecond timebase for DHT22 (this Cortex-M0+ has no DWT cycle counter) |

## SPI2 - shared bus

Three devices sit on the same SCK/MISO/MOSI lines, each with its own chip
select:

| Device | CS pin |
|---|---|
| BMP280 | `BMP_CS` (PB1) |
| W25Q64 flash | `SD_CS` (PB8) - reused since there's no dedicated flash CS; conflicts if you ever add a real SD card on the same line |
| NRF24L01 | `NRF_CSN` (PA8) |

SPI2 runs at 8 MHz (`SPI_BAUDRATEPRESCALER_2` off the 16MHz HSI), mode 0
(`CPOL=0`/`CPHA=0`). Each driver is responsible for keeping its own CS high
except while actively transacting - as long as that holds, sharing the bus is safe.

## ADC1 - shared 3-channel scan

`hadc1` runs a single 3-channel regular scan sequence (12-bit, software
triggered) covering all three analog sensors on this board:

| Regular rank | Channel | Pin | Sensor |
|---|---|---|---|
| 1 | ADC_CHANNEL_4 | PA0 | Turbidity |
| 2 | ADC_CHANNEL_7 | PA3 | MQ135 |
| 3 | ADC_CHANNEL_9 | PA5 | Battery |

A single `HAL_ADC_Start()` converts all three in sequence; a driver that only
wants one of them (like `mq135.c`) still has to poll+`HAL_ADC_GetValue()`
three times and keep the one it needs. See `MQ135_Read()` for the pattern.

> **Fixed bug:** the original generated code only ever set
> `sConfig.Channel = ADC_CHANNEL_4` for all three ranks, so every conversion
> silently read the turbidity pin. `MX_ADC1_Init()` now sets the channel
> explicitly for each rank (see `main.c`).

## Sensor/module drivers

Each driver follows the same shape: `<Name>_Init()`, a blocking `<Name>_Read()`
(or `_Transmit`/`_Receive` for the radio), a `volatile g_<name>_*` debug
mirror updated on every call (so you can watch results in a Live Expressions
view without catching a breakpoint mid-transaction - see
[Debugging](#debugging-with-live-expressions)), and a standalone
`<Name>_Demo()` loop for quick bring-up testing.

### BMP280 (pressure/temperature)

- Files: `Core/Inc/bmp280.c` (yes, a `.c` in `Inc/` - see [Build notes](#build-notes-stm32cubeide-quirks))
- Bus: SPI2, CS = `BMP_CS` (PB1)
- Entry point: `BMP280_RunDemo()` - reads chip ID, calibration data, then
  loops forever logging temperature/pressure over USART3
- **Status: not called from `main()`** (`BMP280_RunDemo();` is commented out).
  It also depends on `huart3`, which is itself commented out/unused in this
  build - don't enable one without the other.

### W25Q64 (SPI NOR flash)

- Files: `Core/Inc/w25q64.h`, `Core/Src/w25q64.c`
- Bus: SPI2, CS = `SD_CS` (PB8, reused)
- API: `W25Q64_Init()`, `W25Q64_ReadJEDECID()`, `W25Q64_ReadData()`,
  `W25Q64_PageProgram()`/`W25Q64_WritePages()`, `W25Q64_SectorErase()`,
  `W25Q64_ChipErase()`
- Demo: `W25Q64_demo()` - erases sector 0, writes a dummy `SensorData_t`
  struct, reads it back
- **Status:** `W25Q64_Init(&hspi2)` runs at boot; `W25Q64_demo()` is
  commented out in `main()`.

### DHT22 (temperature/humidity)

- Files: `Core/Inc/dht22.h`, `Core/Src/dht22.c`
- Pin: `DHT_DATA` (PC15), one-wire protocol - needs a 4.7k-10k pull-up to
  VCC on the data line (most 3-pin breakout modules already have one)
- Timing: bit-banged against `htim6` (1MHz free-running counter) since this
  MCU has no DWT cycle counter for `HAL_Delay()`-free microsecond waits
- API: `DHT22_Init(&htim6)`, `DHT22_Read(DHT22_Data_t *out)` (blocking,
  ~5-25ms), `DHT22_Demo()` (reads every 2s forever)
- Debug globals: `g_dht22_status` (`DHT22_OK` / `DHT22_ERR_TIMEOUT` /
  `DHT22_ERR_CHECKSUM`), `g_dht22_data` (last good reading),
  `g_dht22_raw[5]` (raw bytes, even from a failed transaction, for diagnosis)
- **Status: currently commented out** in `main()` (`DHT22_Init()` and the
  read call in the loop) - it was working, then disabled when the NRF24 went
  in. Uncomment both lines to bring it back.

### MQ135 (air quality)

- Files: `Core/Inc/mq135.h`, `Core/Src/mq135.c`
- Pin: `MQ135_ADC` (PA3) - shares `hadc1`'s 3-channel scan (rank 2)
- ⚠️ **Wire `AO` only from a module powered at 3.3V.** Most MQ135 boards run
  their sensing circuit at 5V and `AO` can swing close to that rail, which
  exceeds this MCU's ADC input rating. This board's module is confirmed
  running from 3.3V.
- API: `MQ135_Init(&hadc1)`, `MQ135_Read()` (runs the full 3-channel scan,
  keeps rank 2), `MQ135_Demo()`
- Output fields: `raw` (12-bit code), `voltage`, `rs_kohm` (derived sensor
  resistance), `ratio` (Rs/Ro - the real trend indicator), `ppm_co2` (rough
  estimate from a commonly-used curve fit - **not accurate without
  calibrating `MQ135_RO_CLEAN_AIR` in `mq135.c` to your physical sensor**,
  and running the heater at 3.3V instead of the datasheet's 5V makes it
  rougher still)
- Debug global: `g_mq135_data`
- **Status: physically disconnected.** `MQ135_Init()` and `MQ135_Read()` are
  commented out in `main()`. The driver code is untouched - reconnect the
  sensor and uncomment both lines to bring it back.

### NRF24L01 (radio)

- Files: `Core/Inc/nrf24.h`, `Core/Src/nrf24.c`
- Bus: SPI2, CS = `NRF_CSN` (PA8), CE = `NRF_CE` (PB7)
- ⚠️ **3.3V only, not 5V tolerant.** Add a 10-100µF cap across VCC/GND at the
  module - bare breakouts brown out on TX bursts without one.
- IRQ is not wired - this driver polls the `STATUS` register instead of
  using an interrupt.
- Config: channel 76 (2.476GHz), 5-byte address `E7 E7 E7 E7 E7` (both are
  just defaults - must match on both ends of a link; change via
  `NRF24_SetChannel()`/`NRF24_SetAddress()`)
- **Auto-ack/auto-retransmit are OFF** for the simplest possible bring-up -
  `NRF24_Transmit()` reports success once the packet goes out over the air,
  *not* proof a receiver got it. Turn `EN_AA`/`SETUP_RETR` on in `nrf24.c`
  once a basic link is confirmed if you want reliability.
- API: `NRF24_Init(&hspi2)`, `NRF24_SelfTest()`, `NRF24_SetTxMode()` /
  `NRF24_SetRxMode()`, `NRF24_Transmit()`, `NRF24_Available()` /
  `NRF24_Receive()`, `NRF24_Demo()` (role picked by `NRF24_DEMO_ROLE_TX` at
  the top of `nrf24.c` - `1` = sends an incrementing counter every second,
  `0` = listens and mirrors whatever arrives)
- Debug globals:
  - `g_nrf24_present` - set once by `NRF24_Init()`'s self-test (writes/reads
    back a distinctive register value). **`1`** = the chip responds over SPI
    correctly (proves wiring/power before worrying about anything going out
    over the air). **`0`** = dead chip, no power, or a miswired SPI/CSN line.
  - `g_nrf24_tx_status` - result of the last `NRF24_Transmit()`
    (`NRF24_TX_OK` / `NRF24_TX_MAX_RETRIES` / `NRF24_TX_TIMEOUT`)
  - `g_nrf24_tx_count` - increments once per transmit
  - `g_nrf24_rx_buf[32]` / `g_nrf24_rx_count` - only used on the RX-role side
- **Status: active.** `NRF24_Init(&hspi2)` runs at boot; the main loop
  transmits a 4-byte incrementing sequence counter (zero-padded to the fixed
  32-byte payload) every 2s.

## What's actually running right now

`main()`'s loop, as currently checked in:

```c
W25Q64_Init(&hspi2);
NRF24_Init(&hspi2);
// DHT22, MQ135, W25Q64 demo, BMP280 demo are all commented out

while (1) {
    static uint32_t seq = 0;
    uint8_t nrf_payload[32] = {0};
    memcpy(nrf_payload, &seq, sizeof(seq));
    NRF24_Transmit(nrf_payload, sizeof(nrf_payload)); /* -> g_nrf24_tx_status */
    seq++;
    HAL_Delay(2000);
}
```

So: only the NRF24 radio is actively doing anything every 2 seconds. Every
other driver is written, wired, and ready - just commented out one or two
lines away from being re-enabled.

## Debugging with Live Expressions

Every driver exposes plain `volatile` globals (`g_<name>_*`) specifically so
you can watch sensor state live without single-stepping through
timing-sensitive bit-banging/SPI code:

1. Start a debug session, then **Resume** so the target runs freely.
2. **Window → Show View → Live Expressions** (updates while the program
   keeps running, unlike the plain Expressions/Variables views which only
   refresh when halted).
3. Add the `g_<name>_*` variable(s) for whichever driver you're checking.

## Build notes (STM32CubeIDE quirks)

- **`.project` lists every compiled `.c` file individually** - no folder
  globbing, no wildcards. A brand-new source file needs an explicit
  `<link>` entry there (Refresh/Clean alone won't discover it). To avoid
  that step for small sensor drivers, `bmp280.c`, `w25q64.c`, `dht22.c`,
  `mq135.c`, and `nrf24.c` are all pulled into `main.c`'s translation unit
  via `#include "....c"` at the top of `main.c`, instead of being compiled
  as their own translation units. Genuine new HAL driver files (like
  `stm32u0xx_hal_tim.c`/`_ex.c`, added when TIM6 was introduced) don't fit
  that pattern and do have real `<link>` entries.
- **`HAL_TIM_MODULE_ENABLED`** was off in `stm32u0xx_hal_conf.h` until TIM6
  was added for DHT22's timebase - if you ever regenerate from the `.ioc` in
  CubeMX, add TIM6 as an activated internal Basic Timer there first, or
  regeneration will silently drop `MX_TIM6_Init()`/its MSP init (they live
  outside the `USER CODE` markers, matching CubeMX's own generated style).
- After adding/removing any driver's `#include` in `main.c`, a plain
  Refresh + Build is enough - no `.project` edit needed, per the point above.
