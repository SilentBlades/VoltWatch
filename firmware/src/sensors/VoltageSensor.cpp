#include "VoltageSensor.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace VoltWatch {

VoltageSensor::VoltageSensor(uint8_t pin, const VoltageDividerConfig& divider)
    : m_pin(pin), m_config(divider), m_mockValue(0), m_mockEnabled(false) {
}

void VoltageSensor::init() {
#ifdef ARDUINO
    pinMode(m_pin, INPUT);
    #if defined(ESP32)
    // ESP32 default 11dB attenuation gives full 0 - 3.3V measurement range
    analogSetPinAttenuation(m_pin, ADC_11db);
    #endif
#endif
}

uint16_t VoltageSensor::readRawAdc() {
    if (m_mockEnabled) {
        return m_mockValue;
    }

#ifdef ARDUINO
    return static_cast<uint16_t>(analogRead(m_pin));
#else
    return m_mockValue;
#endif
}

float VoltageSensor::calculateVoltage(uint16_t adcValue) const {
    if (m_config.adcResolution == 0 || m_config.r2Ohms <= 0.0f) {
        return 0.0f;
    }

    // 1. Calculate the pin voltage sensed at the ESP32 ADC pin
    float pinVoltage = (static_cast<float>(adcValue) / static_cast<float>(m_config.adcResolution))
                     * m_config.vRef
                     * m_config.calibrationFactor;

    // 2. Scale up by the voltage divider factor: Vin = Vpin * (R1 + R2) / R2
    float dividerMultiplier = (m_config.r1Ohms + m_config.r2Ohms) / m_config.r2Ohms;

    return pinVoltage * dividerMultiplier;
}

float VoltageSensor::readInstantaneousVoltage() {
    return calculateVoltage(readRawAdc());
}

float VoltageSensor::getDividerRatio() const {
    if (m_config.r2Ohms <= 0.0f) {
        return 1.0f;
    }
    return (m_config.r1Ohms + m_config.r2Ohms) / m_config.r2Ohms;
}

void VoltageSensor::setCalibrationFactor(float factor) {
    if (factor > 0.0f) {
        m_config.calibrationFactor = factor;
    }
}

void VoltageSensor::setDividerConfig(const VoltageDividerConfig& config) {
    m_config = config;
}

void VoltageSensor::setMockAdcValue(uint16_t mockValue) {
    m_mockValue = mockValue;
    m_mockEnabled = true;
}

void VoltageSensor::setMockEnabled(bool enabled) {
    m_mockEnabled = enabled;
}

} // namespace VoltWatch
