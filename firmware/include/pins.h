#ifndef VOLTWATCH_PINS_H
#define VOLTWATCH_PINS_H

#include <stdint.h>

namespace VoltWatch {
namespace Pins {

    // --- Dual Dedicated Hardware UART Ports for Dual PZEM-004T v3.0 ---
    // Using two separate HardwareSerial instances eliminates the PZEM factory address conflict
    // and optocoupler phototransistor bus contention/clamping without requiring external diodes.
    
    // Grid Mains Metrology Module (ESP32 Hardware Serial1)
    constexpr uint8_t PZEM_GRID_RX_PIN = 18;  // ESP32 GPIO 18 (RX1) connected to Grid PZEM TX
    constexpr uint8_t PZEM_GRID_TX_PIN = 19;  // ESP32 GPIO 19 (TX1) connected to Grid PZEM RX

    // Inverter Output Metrology Module (ESP32 Hardware Serial2)
    constexpr uint8_t PZEM_INV_RX_PIN  = 16;  // ESP32 GPIO 16 (RX2) connected to Inverter PZEM TX
    constexpr uint8_t PZEM_INV_TX_PIN  = 17;  // ESP32 GPIO 17 (TX2) connected to Inverter PZEM RX

    // Backward-compatible single-bus aliases (if wired with open-collector diode circuit)
    constexpr uint8_t PZEM_RX2_PIN     = PZEM_INV_RX_PIN;
    constexpr uint8_t PZEM_TX2_PIN     = PZEM_INV_TX_PIN;

    // --- Status and Indicator Peripherals ---
    #ifndef STATUS_LED_PIN
    #ifdef LED_BUILTIN
    constexpr uint8_t STATUS_LED_PIN   = LED_BUILTIN; // Uses board-specific LED_BUILTIN if defined
    #else
    constexpr uint8_t STATUS_LED_PIN   = 2;   // GPIO 2 (Onboard blue LED for 30-pin boards; for 38-pin clones connect external LED)
    #endif
    #endif
    constexpr uint8_t ALERT_BUZZER_PIN = 25;  // GPIO 25 (Audible alarm / Buzzer trigger)

    // --- Analog ADC Pin (Legacy DC monitoring / auxiliary sensing) ---
    constexpr uint8_t VOLTAGE_ADC_PIN  = 34;  // ADC1 Channel 6 (GPIO 34)

    // --- I2C Bus Pins ---
    constexpr uint8_t I2C_SDA_PIN      = 21;  // ESP32 I2C SDA
    constexpr uint8_t I2C_SCL_PIN      = 22;  // ESP32 I2C SCL

} // namespace Pins
} // namespace VoltWatch

#endif // VOLTWATCH_PINS_H
