/*
 * Gateway_ESP32.ino
 *
 * ESP32 gateway for the "Multi Modular Node" project. Receives sensor
 * telemetry from the STM32 sensor node over NRF24L01, timestamps it with a
 * DS3231 RTC, and prints it to the serial console.
 *
 * The STM32 side uses a custom bare-register NRF24 driver (Core/Src/nrf24.c
 * in the STM32 project); this sketch uses the RF24 library instead - both
 * are configured to match on channel/address/payload size/CRC/data
 * rate/PA level, which is all that matters for two nRF24L01s to talk to
 * each other regardless of which driver either side runs.
 *
 * Wiring (see README.md in the STM32 project for the sensor-node side):
 *
 *   NRF24L01      ESP32
 *   VCC        -> 3.3V  (NOT 5V tolerant - add a 10-47uF cap across VCC/GND
 *                         at the module, bare breakouts brown out on TX/RX)
 *   GND        -> GND
 *   CE         -> GPIO4
 *   CSN        -> GPIO5
 *   SCK        -> GPIO18  (ESP32 VSPI default)
 *   MOSI       -> GPIO23  (ESP32 VSPI default)
 *   MISO       -> GPIO19  (ESP32 VSPI default)
 *   IRQ        -> not connected (this sketch polls radio.available())
 *
 *   DS3231 RTC    ESP32
 *   VCC        -> 3.3V or 5V (check your specific module)
 *   GND        -> GND
 *   SDA        -> GPIO21
 *   SCL        -> GPIO22
 *
 * Libraries required (Arduino IDE: Tools > Manage Libraries...):
 *   - "RF24" by TMRh20
 *   - "RTClib" by Adafruit
 *
 * Status: NRF24L01 + RTC only. The A7670C 4G module mentioned in the
 * wiring notes is not wired or used by this sketch yet.
 */

#include <SPI.h>
#include <Wire.h>
#include <RF24.h>
#include <RTClib.h>

/* ---- Pin assignments ---- */
#define NRF_CE_PIN   4
#define NRF_CSN_PIN  5
#define RTC_SDA_PIN  21
#define RTC_SCL_PIN  22

/* ---- Radio config - MUST match the STM32 sensor node (nrf24.c) ---- */
#define NRF_CHANNEL       76   /* 2.476GHz */
#define NRF_PAYLOAD_SIZE  32
static const uint8_t NRF_ADDRESS[5] = {0xE7, 0xE7, 0xE7, 0xE7, 0xE7};

RF24 radio(NRF_CE_PIN, NRF_CSN_PIN);
RTC_DS3231 rtc;
static bool rtc_ok = false;

/* Wire format sent by the STM32 sensor node - MUST stay byte-for-byte
 * identical to TelemetryPacket_t in the STM32 project's Core/Src/main.c
 * (packed, same fixed-width fields, same order). Both sides are
 * little-endian GCC targets, so the raw layout matches without any
 * conversion. */
struct __attribute__((packed)) TelemetryPacket {
    uint32_t seq;
    float    bmp_temp_c;
    float    bmp_pressure_hpa;
    float    mq135_ratio;
    float    mq135_ppm_co2;
    uint16_t mq135_raw;
    uint8_t  bmp_ok;
    uint8_t  mq135_ok;
}; /* 24 bytes - the STM32 side zero-pads the rest of the 32-byte payload */

void setup()
{
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println("Gateway ESP32 starting...");

    Wire.begin(RTC_SDA_PIN, RTC_SCL_PIN);
    rtc_ok = rtc.begin();
    if (!rtc_ok)
    {
        Serial.println("RTC not found - check wiring (SDA=21, SCL=22). Timestamps will read as 0.");
    }
    else if (rtc.lostPower())
    {
        Serial.println("RTC lost power - setting it to this sketch's compile time.");
        Serial.println("(Replace the coin cell if this keeps happening on every boot.)");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    if (!radio.begin())
    {
        Serial.println("nRF24L01 not responding on SPI - check wiring and that it's on 3.3V, not 5V.");
        while (1) { delay(1000); }
    }

    radio.setChannel(NRF_CHANNEL);
    radio.setDataRate(RF24_1MBPS);
    radio.setPALevel(RF24_PA_MAX);   /* 0dBm - matches RF_SETUP=0x06 on the STM32 side */
    radio.setCRCLength(RF24_CRC_16);
    radio.setAddressWidth(5);
    radio.setAutoAck(false);         /* matches EN_AA=0 on the STM32 side - no ack/retry yet */
    radio.disableDynamicPayloads();
    radio.setPayloadSize(NRF_PAYLOAD_SIZE);
    radio.openReadingPipe(0, NRF_ADDRESS);
    radio.startListening();

    Serial.println("Radio ready. Listening for telemetry from the sensor node...");
}

static void printTimestamp()
{
    if (!rtc_ok)
    {
        Serial.print("[no RTC]");
        return;
    }

    DateTime now = rtc.now();
    char buf[20];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(),
             now.hour(), now.minute(), now.second());
    Serial.print(buf);
}

void loop()
{
    if (radio.available())
    {
        TelemetryPacket packet;
        radio.read(&packet, sizeof(packet));

        printTimestamp();

        Serial.print("  seq=");
        Serial.print(packet.seq);

        Serial.print("  BMP280[");
        Serial.print(packet.bmp_ok ? "OK" : "FAIL");
        Serial.print("] T=");
        Serial.print(packet.bmp_temp_c, 2);
        Serial.print("C P=");
        Serial.print(packet.bmp_pressure_hpa, 2);
        Serial.print("hPa");

        Serial.print("  MQ135[");
        Serial.print(packet.mq135_ok ? "OK" : "FAIL");
        Serial.print("] raw=");
        Serial.print(packet.mq135_raw);
        Serial.print(" ratio=");
        Serial.print(packet.mq135_ratio, 3);
        Serial.print(" ppmCO2~=");
        Serial.println(packet.mq135_ppm_co2, 1);
    }
}
